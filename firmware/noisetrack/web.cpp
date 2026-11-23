// NoiseTrack: página do ESP32 com medidor ao vivo (Parte 10).
// HTTP (biblioteca WebServer do núcleo) na porta 80 e um WebSocket mínimo,
// feito aqui mesmo com o WiFiServer do núcleo, na porta 81. Sem bibliotecas
// externas. Tudo roda a partir do loop() e nunca espera a rede.
#include "web.h"
#include "web_logica.h"
#include "pagina_html.h"
#include "config.h"
#include "tarefa_audio.h"
#include "armazenamento.h"
#include "rede.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <lwip/sockets.h>
#include <errno.h>

static constexpr uint32_t CONSULTA_MS          = 20;     // olha se há janela nova a cada 20 ms
static constexpr uint32_t APERTO_TIMEOUT_MS    = 3000;   // tempo máximo do aperto de mão
static constexpr uint8_t  FALHAS_ENVIO_MAX     = 16;     // ~2 s de quadros recusados: desiste
static constexpr size_t   TAM_LINHA            = 128;
static constexpr size_t   LEITURA_MAX_POR_VOLTA = 256;   // não fica preso lendo um cliente

static WebServer     servidor(WEB_PORTA_HTTP);
static WiFiServer    servidorWs(WEB_PORTA_WS, WEB_MAX_CLIENTES_WS);
static bool          ativo       = false;
static bool          servicoMdns = false;
static BloqueioSenha bloqueio;

// ---------- Clientes do WebSocket ----------
enum class FaseWs : uint8_t { LIVRE, APERTO_DE_MAO, ABERTO };

struct ClienteWs {
  WiFiClient cliente;
  FaseWs     fase = FaseWs::LIVRE;
  char       linha[TAM_LINHA];        // linha do pedido sendo montada
  size_t     tamLinha = 0;
  char       chave[32];               // Sec-WebSocket-Key do navegador
  bool       temChave = false;
  uint32_t   tInicio = 0;
  uint8_t    falhasEnvio = 0;
};

static ClienteWs clientes[WEB_MAX_CLIENTES_WS];
static uint32_t  tUltimaConsulta = 0;
static uint32_t  ultimaJanela    = 0;

static int clientesAbertos() {
  int n = 0;
  for (auto& c : clientes) if (c.fase == FaseWs::ABERTO) n++;
  return n;
}

static void fechar(ClienteWs& c) {
  c.cliente.stop();
  c.fase = FaseWs::LIVRE;
  c.tamLinha = 0;
  c.temChave = false;
  c.falhasEnvio = 0;
}

static bool montarJson(char* json, size_t tam) {
  const EstadoCompartilhado e = tarefa_audio_copiar();   // a mesma cópia que o loop() usa
  return web_montar_json(e, rede_id(), json, tam) > 0;
}

// Envia um quadro de texto SEM esperar: se o envio do TCP estiver cheio (cliente
// lento), este quadro é pulado. Assim o loop() nunca fica preso num cliente.
static void enviarTexto(ClienteWs& c, const char* dados, size_t n) {
  uint8_t quadro[4 + WEB_TAM_JSON];
  if (n > WEB_TAM_JSON) return;
  const size_t cab = ws_cabecalho_texto(n, quadro);
  memcpy(quadro + cab, dados, n);
  const size_t total = cab + n;

  const int fd = c.cliente.fd();
  if (fd < 0) { fechar(c); return; }
  const int r = lwip_send(fd, quadro, total, MSG_DONTWAIT);   // MSG_DONTWAIT = não esperar
  if (r == (int)total) { c.falhasEnvio = 0; return; }
  if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    if (++c.falhasEnvio >= FALHAS_ENVIO_MAX) fechar(c);   // parado há ~2 s: desiste
    return;
  }
  fechar(c);   // erro, ou só parte do quadro saiu (ele ficaria quebrado)
}

static void responderAperto(ClienteWs& c) {
  char aceite[32];
  if (!c.temChave || !ws_calcular_aceite(c.chave, aceite, sizeof(aceite))) {
    c.cliente.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
    fechar(c);
    return;
  }
  char resposta[160];
  snprintf(resposta, sizeof(resposta),
           "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
           "Connection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", aceite);
  c.cliente.write((const uint8_t*)resposta, strlen(resposta));
  c.fase = FaseWs::ABERTO;
  c.falhasEnvio = 0;
  Serial.printf("[web] cliente ao vivo conectado (%d de %d)\n",
                clientesAbertos(), WEB_MAX_CLIENTES_WS);

  char json[WEB_TAM_JSON];   // manda o estado atual na hora, sem esperar a próxima janela
  if (montarJson(json, sizeof(json))) enviarTexto(c, json, strlen(json));
}

static void aceitarNovos(uint32_t agora) {
  WiFiClient novo = servidorWs.available();   // não espera: devolve vazio se não houver
  if (!novo) return;
  for (auto& c : clientes) {
    if (c.fase == FaseWs::LIVRE) {
      c.cliente  = novo;
      c.fase     = FaseWs::APERTO_DE_MAO;
      c.tamLinha = 0;
      c.temChave = false;
      c.tInicio  = agora;
      return;
    }
  }
  novo.print("HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n\r\n");
  novo.stop();
  Serial.println("[web] cliente ao vivo recusado: limite de clientes (usa o polling)");
}

// Lê o pedido do navegador linha por linha, só o que já chegou.
static void atenderAperto(ClienteWs& c, uint32_t agora) {
  if ((uint32_t)(agora - c.tInicio) >= APERTO_TIMEOUT_MS || !c.cliente.connected()) {
    fechar(c);
    return;
  }
  size_t lidos = 0;
  while (c.fase == FaseWs::APERTO_DE_MAO && c.cliente.available() > 0 &&
         lidos < LEITURA_MAX_POR_VOLTA) {
    const int ch = c.cliente.read();
    if (ch < 0) break;
    lidos++;
    if (ch == '\n') {
      if (c.tamLinha > 0 && c.linha[c.tamLinha - 1] == '\r') c.tamLinha--;
      c.linha[c.tamLinha] = '\0';
      if (c.tamLinha == 0) {   // linha vazia: o pedido acabou
        responderAperto(c);
        return;
      }
      if (!c.temChave) c.temChave = ws_extrair_chave(c.linha, c.chave, sizeof(c.chave));
      c.tamLinha = 0;
    } else if (c.tamLinha < TAM_LINHA - 1) {
      c.linha[c.tamLinha++] = (char)ch;   // linhas longas são cortadas (a da chave é curta)
    }
  }
}

// A página não manda mensagens: o que chegar é descartado. Só o pedido de
// fechamento (tipo 8) é respondido.
static void atenderAberto(ClienteWs& c) {
  if (!c.cliente.connected()) { fechar(c); return; }
  const int disponivel = c.cliente.available();
  if (disponivel <= 0) return;
  uint8_t buf[64];
  const int lidos = c.cliente.read(buf, disponivel < (int)sizeof(buf) ? disponivel : sizeof(buf));
  if (lidos > 0 && (buf[0] & 0x0F) == 0x08) {
    const uint8_t fim[2] = {0x88, 0x00};
    const int fd = c.cliente.fd();
    if (fd >= 0) lwip_send(fd, fim, sizeof(fim), MSG_DONTWAIT);
    fechar(c);
  }
}

// ---------- Rotas HTTP ----------
static void responderJson(int codigo, const char* json) {
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.send(codigo, "application/json", json);
}

static void rotaPagina() {
  servidor.send_P(200, "text/html; charset=utf-8", PAGINA_HTML);   // direto da flash
}

static void rotaStatus() {
  char json[WEB_TAM_JSON];
  if (!montarJson(json, sizeof(json))) {
    responderJson(500, "{\"ok\":false}");
    return;
  }
  responderJson(200, json);
}

static void responderBloqueado(uint32_t agora) {
  char json[64];
  snprintf(json, sizeof(json), "{\"ok\":false,\"erro\":\"bloqueado\",\"espera_s\":%lu}",
           (unsigned long)((bloqueio.esperaMs(agora) + 999) / 1000));
  responderJson(429, json);
}

// POST /config: limite, x e senha. Grava pelo armazenamento, aqui no loop()
// (o servidor roda dentro do loop), nunca na tarefa de áudio.
static void rotaConfig() {
  const uint32_t agora = millis();
  if (bloqueio.bloqueado(agora)) {
    responderBloqueado(agora);
    return;
  }
  if (!web_senha_confere(servidor.arg("senha").c_str(), rede_senha_config())) {
    bloqueio.registrarErro(agora);
    Serial.printf("[web] senha de configuracao errada (restam %d)\n", bloqueio.restantes());
    if (bloqueio.bloqueado(agora)) {
      Serial.printf("[web] configuracao bloqueada por %lu s\n",
                    (unsigned long)(WEB_BLOQUEIO_MS / 1000));
      responderBloqueado(agora);
      return;
    }
    char json[64];
    snprintf(json, sizeof(json), "{\"ok\":false,\"erro\":\"senha\",\"restam\":%d}",
             bloqueio.restantes());
    responderJson(403, json);
    return;
  }
  bloqueio.registrarAcerto();

  // Valor inválido: responde qual campo e por quê, e NÃO aplica nada.
  int limite = 0, tempoXs = 0;
  const ErroConfig erro = web_validar_config(servidor.arg("limite").c_str(),
                                             servidor.arg("x").c_str(), &limite, &tempoXs);
  if (erro != ErroConfig::NENHUM) {
    char texto[96], json[160];
    web_mensagem_erro_config(erro, texto, sizeof(texto));
    snprintf(json, sizeof(json), "{\"ok\":false,\"erro\":\"%s\",\"mensagem\":\"%s\"}",
             web_campo_erro_config(erro), texto);
    Serial.printf("[web] configuracao recusada (%s): limite='%s' x='%s'\n",
                  web_campo_erro_config(erro), servidor.arg("limite").c_str(),
                  servidor.arg("x").c_str());
    responderJson(400, json);
    return;
  }
  const ResultadoSalvar r = armazenamento_salvar(limite, tempoXs);
  if (!r.aceito) {   // não deve acontecer: a faixa é a mesma do armazenamento
    responderJson(400, "{\"ok\":false,\"erro\":\"faixa\",\"mensagem\":\"Valores recusados.\"}");
    return;
  }
  Serial.printf("[web] configuracao pela pagina: limite = %d dB, X = %d s (%s)\n", limite,
                tempoXs, r.erroFlash ? "ERRO ao gravar" : (r.gravou ? "gravado" : "igual"));
  char json[80];
  snprintf(json, sizeof(json), "{\"ok\":true,\"gravou\":%s,\"erro_flash\":%s}",
           r.gravou ? "true" : "false", r.erroFlash ? "true" : "false");
  responderJson(200, json);
}

static void rotaNaoEncontrada() { servidor.send(404, "text/plain", "nao encontrado"); }

// ---------- Ligar e desligar ----------
static void ligar() {
  servidor.begin();
  servidorWs.begin();
  servidorWs.setNoDelay(true);
  ativo = true;
  if (!servicoMdns) servicoMdns = MDNS.addService("http", "tcp", WEB_PORTA_HTTP);
  Serial.printf("[web] pagina em http://%s/ e http://%s.local/ (ao vivo na porta %u)\n",
                WiFi.localIP().toString().c_str(), MDNS_NOME, WEB_PORTA_WS);
}

void web_parar() {
  if (!ativo) return;
  for (auto& c : clientes) if (c.fase != FaseWs::LIVRE) fechar(c);
  servidorWs.end();
  servidor.stop();
  ativo = false;
  Serial.println("[web] pagina desligada (Wi-Fi do local fora)");
}

void web_iniciar() {
  servidor.on("/", HTTP_GET, rotaPagina);
  servidor.on("/status", HTTP_GET, rotaStatus);
  servidor.on("/config", HTTP_POST, rotaConfig);
  servidor.onNotFound(rotaNaoEncontrada);
}

void web_atualizar(uint32_t agora) {
  // Só com o Wi-Fi do local conectado. Nesse estado o AP de setup (e o portal,
  // que também usa a porta 80) já foi desligado pela máquina de estados da rede.
  const bool deveRodar = rede_estado() == EstadoRede::CONECTADO;
  if (deveRodar && !ativo) ligar();
  else if (!deveRodar && ativo) web_parar();
  if (!ativo) return;

  servidor.handleClient();
  aceitarNovos(agora);
  for (auto& c : clientes) {
    if (c.fase == FaseWs::APERTO_DE_MAO) atenderAperto(c, agora);
    else if (c.fase == FaseWs::ABERTO)   atenderAberto(c);
  }

  // A cada janela nova da tarefa (~8 por segundo), manda o estado a todos.
  if ((uint32_t)(agora - tUltimaConsulta) < CONSULTA_MS) return;
  tUltimaConsulta = agora;
  if (clientesAbertos() == 0) return;
  const EstadoCompartilhado e = tarefa_audio_copiar();
  if (e.contadorJanelas == ultimaJanela) return;
  ultimaJanela = e.contadorJanelas;
  char json[WEB_TAM_JSON];
  const size_t n = web_montar_json(e, rede_id(), json, sizeof(json));
  if (n == 0) return;
  for (auto& c : clientes) if (c.fase == FaseWs::ABERTO) enviarTexto(c, json, n);
}

void web_imprimir_status() {
  if (ativo) {
    Serial.printf("[web] http://%s/ | http://%s.local/ | clientes ao vivo: %d de %d",
                  WiFi.localIP().toString().c_str(), MDNS_NOME, clientesAbertos(),
                  WEB_MAX_CLIENTES_WS);
  } else {
    Serial.print("[web] desligada (Wi-Fi do local nao conectado)");
  }
  Serial.printf(" | senha de configuracao: %s\n", rede_senha_config());
}
