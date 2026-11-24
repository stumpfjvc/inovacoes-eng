// NoiseTrack: nuvem (Supabase). As decisões e contas ficam em nuvem_logica.cpp
// (puro e testado); aqui ficam a tarefa, o HTTPS e o que é impresso.
//
// Quem faz o quê (como threads em Java):
//   - tarefa da nuvem (prioridade 1, núcleo 0): a cada NUVEM_INTERVALO_MS (1 s)
//     monta o pedido, fala com o servidor (pode demorar alguns segundos) e guarda
//     o resultado. A conexão HTTPS fica aberta entre os envios (keep-alive).
//     Um envio de cada vez, sempre: se um demorar, os ciclos perdidos são pulados;
//   - loop(): aplica e grava o limite/X recebido (a gravação na flash fica no
//     loop(), como no resto do projeto) e imprime o comando "nuvem";
//   - tarefa de áudio (prioridade 2, núcleo 1): não sabe que a nuvem existe.
#include "nuvem.h"
#include "config.h"

#if NUVEM_ATIVA
#include "nuvem_logica.h"
#include "nuvem_ca.h"
#include "tarefa_audio.h"
#include "armazenamento.h"
#include "rede.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <string.h>

// A chave do aparelho vem do secrets.h (fora do Git). Sem ela, a nuvem desliga.
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef NUVEM_CHAVE_APARELHO
#define NUVEM_CHAVE_APARELHO ""
#endif

static constexpr uint16_t PORTA_HTTPS = 443;
static constexpr uint32_t PASSO_MS    = 100;   // a tarefa confere a hora a cada 0,1 s

// ---------- Estado compartilhado (tarefa da nuvem <-> loop) ----------
// Protegido por trava, como um "synchronized" do Java.
struct StatusNuvem {
  ResultadoNuvem ultimo        = ResultadoNuvem::NENHUM;
  int            ultimoHttp    = 0;
  uint32_t       tUltimaTentativa = 0;
  bool           teveSucesso   = false;
  uint32_t       tUltimoSucesso = 0;
  uint32_t       esperaMs      = NUVEM_INTERVALO_MS;
  uint32_t       tentativas    = 0;
  uint32_t       sucessos      = 0;
  uint32_t       conexoes      = 0;   // conexões TLS abertas (com keep-alive, quase sempre 1)
  uint32_t       ciclosPulados = 0;   // ciclos perdidos porque um envio demorou
  uint32_t       duracaoMs     = 0;   // quanto durou o último envio
  bool           temConfig     = false;   // já recebeu limite/X válidos
  int            limiteRecebido   = 0;
  int            segundosRecebidos = 0;
};

static SemaphoreHandle_t trava   = nullptr;
static TaskHandle_t      tarefa  = nullptr;
static StatusNuvem       status;
static bool              pendente = false;     // config nova para o loop() aplicar
static int               pendLimite = 0, pendSegundos = 0;
static volatile bool     wifiConectado = false;   // escrito pelo loop(), lido pela tarefa
static const char*       motivoDesligada = "";
static char              host[101];

static void travar()    { xSemaphoreTake(trava, portMAX_DELAY); }
static void destravar() { xSemaphoreGive(trava); }

// ---------- Só a tarefa da nuvem usa ----------
static WiFiClientSecure cliente;
static char             json[384];     // corpo do pedido
static char             pedido[1100];  // cabeçalho + corpo, enviados juntos numa escrita só
static char             bruto[3072];   // resposta HTTP crua (cabeçalhos + corpo)
static uint32_t         contConexoes = 0;

// Abre a conexão HTTPS (aperto de mão TLS incluído). Bloqueia só a tarefa da nuvem.
static bool abrirConexao() {
#if NUVEM_TLS_VALIDAR
  cliente.setCACert(NUVEM_CA_RAIZ);
#else
  cliente.setInsecure();   // criptografa, mas NÃO confere o certificado (ver README)
#endif
  cliente.setHandshakeTimeout((NUVEM_TIMEOUT_MS + 999) / 1000);   // em segundos
  if (!cliente.connect(host, PORTA_HTTPS, (int32_t)NUVEM_TIMEOUT_MS)) {
    char erro[96] = "";
    cliente.lastError(erro, sizeof(erro));
    cliente.stop();
    if (erro[0] != '\0') Serial.printf("[nuvem] conexao falhou: %s\n", erro);
    return false;
  }
  contConexoes++;
  return true;
}

// Joga fora bytes que tenham sobrado de uma resposta anterior (só acontece numa
// conexão reaproveitada). Sem isso, eles seriam lidos como início da próxima.
static void esvaziarEntrada() {
  uint8_t lixo[64];
  int disponivel;
  while ((disponivel = cliente.available()) > 0) {
    const int pedir = disponivel < (int)sizeof(lixo) ? disponivel : (int)sizeof(lixo);
    if (cliente.read(lixo, (size_t)pedir) <= 0) break;
  }
}

// Lê a resposta até ela ficar inteira (pelo Content-Length ou pelo último pedaço:
// o servidor NÃO fecha a conexão), até a conexão cair, o tempo acabar ou o
// buffer encher. Devolve quantos bytes chegaram.
static size_t lerResposta(bool* completa, bool* servidorFecha, size_t* tamTotal) {
  *completa = false;
  *servidorFecha = false;
  *tamTotal = 0;
  size_t n = 0;
  const uint32_t t0 = millis();
  while ((uint32_t)(millis() - t0) < NUVEM_TIMEOUT_RESPOSTA_MS && n < sizeof(bruto) - 1) {
    const int disponivel = cliente.available();
    if (disponivel > 0) {
      const size_t cabe  = sizeof(bruto) - 1 - n;
      const size_t pedir = (size_t)disponivel < cabe ? (size_t)disponivel : cabe;
      const int lidos = cliente.read((uint8_t*)bruto + n, pedir);
      if (lidos > 0) {
        n += (size_t)lidos;
        bruto[n] = '\0';
        const FimHttp f = nuvem_fim_da_resposta(bruto, n, tamTotal, servidorFecha);
        if (f == FimHttp::COMPLETA) { *completa = true; break; }
        if (f == FimHttp::INVALIDA) break;
      }
    } else if (!cliente.connected()) {
      break;
    } else {
      vTaskDelay(pdMS_TO_TICKS(5));   // nada chegou ainda: dá a vez para outras tarefas
    }
  }
  bruto[n] = '\0';
  return n;
}

// Um envio completo. Só existe UM por vez: quem chama é a tarefa da nuvem, e ela
// só começa o próximo depois que este termina (nunca há dois pedidos juntos).
// Bloqueia a TAREFA DA NUVEM por até alguns segundos; o alarme local continua
// rodando na outra tarefa, no outro núcleo.
static ResultadoNuvem sincronizar(int* http, int* limite, int* segundos) {
  *http = 0;

  // O MAIOR nível das janelas desde o envio anterior (a chamada já zera a conta)
  // e o estado de AGORA. Se por algum motivo nenhuma janela chegou, usa a atual.
  const LeituraMaxima pico = tarefa_audio_pegar_maximo();
  const EstadoCompartilhado e = tarefa_audio_copiar();   // cópia, com a trava da tarefa de áudio
  float nivel  = e.dbCombinado;
  bool  valido = e.validoCombinado;
  if (pico.janelas > 0) {
    nivel  = pico.db;
    valido = pico.valido;
  }

  const size_t tamJson = nuvem_montar_json(rede_id(), NUVEM_CHAVE_APARELHO, nivel, valido,
                                           e.estado, json, sizeof(json));
  if (tamJson == 0) return ResultadoNuvem::RESPOSTA_INVALIDA;

  // Pedido HTTP escrito à mão (o HTTPClient ocuparia mais flash), com cabeçalho e
  // corpo no mesmo buffer: uma escrita só, um pacote só.
  // "Connection: keep-alive": o servidor mantém a conexão aberta para o próximo envio.
  const int tamCab = snprintf(pedido, sizeof(pedido),
                              "POST /rest/v1/rpc/aparelho_sync HTTP/1.1\r\n"
                              "Host: %s\r\n"
                              "apikey: %s\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: %u\r\n"
                              "Connection: keep-alive\r\n\r\n",
                              host, NUVEM_CHAVE_PUBLICA, (unsigned)tamJson);
  if (tamCab <= 0 || (size_t)tamCab + tamJson >= sizeof(pedido)) {
    return ResultadoNuvem::RESPOSTA_INVALIDA;
  }
  memcpy(pedido + tamCab, json, tamJson);
  const size_t tamPedido = (size_t)tamCab + tamJson;

  // Até 2 tentativas, e só quando a conexão reaproveitada estava morta (o servidor
  // fechou enquanto esperávamos): aí abre uma nova e repete, sem esperar o próximo ciclo.
  bool   enviou = false, completa = false, servidorFecha = false;
  size_t n = 0, tamTotal = 0;
  for (int tentativa = 0; tentativa < 2; tentativa++) {
    const bool reaproveitada = cliente.connected();
    if (!reaproveitada && !abrirConexao()) return ResultadoNuvem::SEM_CONEXAO;
    esvaziarEntrada();

    enviou = cliente.write((const uint8_t*)pedido, tamPedido) == tamPedido;
    n = enviou ? lerResposta(&completa, &servidorFecha, &tamTotal) : 0;

    if ((!enviou || n == 0) && reaproveitada) {   // conexão velha: fecha e tenta com uma nova
      cliente.stop();
      continue;
    }
    break;
  }
  if (!enviou) {
    cliente.stop();
    return ResultadoNuvem::SEM_CONEXAO;
  }

  // Só mantém a conexão aberta se a resposta chegou inteira, sem sobras e sem o
  // servidor ter pedido para fechar. Qualquer dúvida: fecha (o próximo envio reconecta).
  if (!completa || servidorFecha || n > tamTotal) cliente.stop();

  char corpo[512];
  int codigo = 0;
  if (!nuvem_ler_resposta_http(bruto, n, &codigo, corpo, sizeof(corpo))) {
    cliente.stop();   // resposta quebrada: não reaproveita
    return ResultadoNuvem::SEM_RESPOSTA;
  }
  *http = codigo;
  return nuvem_interpretar_resposta(codigo, corpo, limite, segundos);
}

static void tarefaNuvem(void*) {
  uint32_t       espera         = NUVEM_INTERVALO_MS;
  uint32_t       proximo        = 0;       // instante do próximo envio
  bool           primeira       = true;    // manda já, sem esperar o intervalo
  bool           semWifiTratado = false;
  ResultadoNuvem anterior       = ResultadoNuvem::NENHUM;

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(PASSO_MS));   // espera DENTRO da tarefa da nuvem (o loop() não espera)
    if (!wifiConectado) {                  // sem Wi-Fi do local (ou no portal): nem tenta
      if (!semWifiTratado) {
        cliente.stop();                    // a conexão velha era de outra rede: não serve mais
        semWifiTratado = true;
      }
      primeira = true;                     // quando o Wi-Fi voltar, manda logo
      continue;
    }
    semWifiTratado = false;
    const uint32_t inicio = millis();
    if (!primeira && (int32_t)(inicio - proximo) < 0) continue;   // ainda não é a hora
    primeira = false;

    int http = 0, limite = 0, segundos = 0;
    const ResultadoNuvem r = sincronizar(&http, &limite, &segundos);
    const bool ok = (r == ResultadoNuvem::OK);
    const uint32_t fim = millis();

    espera = nuvem_proxima_espera(espera, ok);
    uint32_t pulados = 0;
    proximo = nuvem_proximo_envio(inicio, fim, espera, &pulados);   // pula os ciclos perdidos

    travar();
    status.ultimo           = r;
    status.ultimoHttp       = http;
    status.tUltimaTentativa = inicio;
    status.esperaMs         = espera;
    status.tentativas++;
    status.conexoes         = contConexoes;
    status.ciclosPulados   += pulados;
    status.duracaoMs        = (uint32_t)(fim - inicio);
    if (ok) {
      status.teveSucesso       = true;
      status.tUltimoSucesso    = fim;
      status.sucessos++;
      status.temConfig         = true;
      status.limiteRecebido    = limite;
      status.segundosRecebidos = segundos;
      pendente     = true;   // o loop() compara com o atual e grava se mudou
      pendLimite   = limite;
      pendSegundos = segundos;
    }
    destravar();

    // Imprime só quando muda (o primeiro sucesso e cada falha), para não
    // encher o Serial com um "ok" por segundo.
    if (!ok) {
      Serial.printf("[nuvem] falha: %s", nuvem_nome_resultado(r));
      if (http != 0) Serial.printf(" (HTTP %d)", http);
      Serial.printf(" | proxima tentativa em %.1f s\n", espera / 1000.0f);
      if (r == ResultadoNuvem::APARELHO_INVALIDO) {
        Serial.println("[nuvem] o banco nao reconhece este ID/chave: confira o NUVEM_CHAVE_APARELHO "
                       "do secrets.h e se o SQL do novo_aparelho.sh foi rodado no Supabase");
      }
    } else if (anterior != ResultadoNuvem::OK) {
      Serial.printf("[nuvem] ok: enviado e recebido limite %d dB, X %d s (de novo a cada %lu ms)\n",
                    limite, segundos, (unsigned long)NUVEM_INTERVALO_MS);
    }
    anterior = r;
  }
}

// ---------- Funções públicas ----------
void nuvem_iniciar() {
  const char* motivo = "";
  if (!nuvem_config_valida(NUVEM_URL, NUVEM_CHAVE_PUBLICA, NUVEM_CHAVE_APARELHO, &motivo)) {
    motivoDesligada = motivo;
    Serial.printf("[nuvem] desligada: %s\n", motivo);
    return;
  }
#if NUVEM_TLS_VALIDAR
  if (strstr(NUVEM_CA_RAIZ, "BEGIN CERTIFICATE") == nullptr) {
    motivoDesligada = "NUVEM_TLS_VALIDAR 1, mas o nuvem_ca.h ainda nao tem o certificado";
    Serial.printf("[nuvem] desligada: %s\n", motivoDesligada);
    return;
  }
#endif
  nuvem_extrair_host(NUVEM_URL, host, sizeof(host));   // já validado acima

  trava = xSemaphoreCreateMutex();
  if (trava == nullptr) {
    motivoDesligada = "sem memoria para a trava";
    Serial.println("[nuvem] desligada: sem memoria para a trava");
    return;
  }
  const BaseType_t ok = xTaskCreatePinnedToCore(tarefaNuvem, "nuvem", PILHA_TAREFA_NUVEM, nullptr,
                                                PRIORIDADE_TAREFA_NUVEM, &tarefa, NUCLEO_TAREFA_NUVEM);
  if (ok != pdPASS) {
    tarefa = nullptr;
    motivoDesligada = "nao foi possivel criar a tarefa";
    Serial.println("[nuvem] desligada: nao foi possivel criar a tarefa");
    return;
  }

  char mascara[48];
  nuvem_mascarar_chave(NUVEM_CHAVE_PUBLICA, mascara, sizeof(mascara));
  Serial.printf("[nuvem] ligada: %s | ID %s | chave publica %s | a cada %lu ms com o Wi-Fi conectado\n",
                host, rede_id(), mascara, (unsigned long)NUVEM_INTERVALO_MS);
#if !NUVEM_TLS_VALIDAR
  Serial.println("[nuvem] AVISO: HTTPS SEM verificacao do certificado (NUVEM_TLS_VALIDAR 0): "
                 "a conexao e criptografada, mas o aparelho nao confere se fala mesmo com o "
                 "Supabase. Ver nuvem_ca.h para ligar a verificacao.");
#endif
}

void nuvem_atualizar(uint32_t agora) {
  (void)agora;
  if (tarefa == nullptr) return;
  // Só como cliente do Wi-Fi do local. No portal (AP de setup) a rede não é CONECTADO.
  wifiConectado = (rede_estado() == EstadoRede::CONECTADO);

  travar();
  const bool temPendente = pendente;
  const int  limite = pendLimite, segundos = pendSegundos;
  pendente = false;
  destravar();
  if (!temPendente) return;

  // A nuvem manda: se for diferente do que está em uso, aplica e grava.
  const EstadoCompartilhado e = tarefa_audio_copiar();
  const int limiteAtual = (int)e.limiteDb, segundosAtual = e.tempoXs;
  if (limite == limiteAtual && segundos == segundosAtual) return;   // igual: nem toca na flash

  const ResultadoSalvar r = armazenamento_salvar(limite, segundos);   // mesma função da página
  Serial.printf("[nuvem] configuracao da nuvem aplicada: limite %d -> %d dB, X %d -> %d s (%s)\n",
                limiteAtual, limite, segundosAtual, segundos,
                !r.aceito ? "recusada" : (r.erroFlash ? "ERRO ao gravar" : "gravada"));
}

void nuvem_imprimir_status() {
  if (tarefa == nullptr) {
    Serial.printf("[nuvem] desligada: %s\n", motivoDesligada[0] != '\0' ? motivoDesligada : "?");
    return;
  }
  travar();
  const StatusNuvem s = status;
  destravar();
  const uint32_t agora = millis();

  Serial.printf("[nuvem] ligada | ID %s | servidor %s | TLS %s\n", rede_id(), host,
                NUVEM_TLS_VALIDAR ? "verificado (nuvem_ca.h)" : "SEM verificacao (NUVEM_TLS_VALIDAR 0)");
  Serial.printf("[nuvem] Wi-Fi do local: %s | ultimo resultado: %s", wifiConectado ? "conectado" : "desconectado",
                nuvem_nome_resultado(s.ultimo));
  if (s.ultimoHttp != 0) Serial.printf(" (HTTP %d)", s.ultimoHttp);
  if (s.ultimo != ResultadoNuvem::NENHUM) {
    Serial.printf(" ha %lu s", (unsigned long)((uint32_t)(agora - s.tUltimaTentativa) / 1000));
  }
  Serial.println();
  if (s.teveSucesso) {
    Serial.printf("[nuvem] ultimo sucesso: ha %lu s", (unsigned long)((uint32_t)(agora - s.tUltimoSucesso) / 1000));
  } else {
    Serial.print("[nuvem] ultimo sucesso: nenhum ainda");
  }
  Serial.printf(" | espera atual: %.1f s | tentativas %lu, sucessos %lu\n",
                s.esperaMs / 1000.0f, (unsigned long)s.tentativas, (unsigned long)s.sucessos);
  Serial.printf("[nuvem] conexoes TLS abertas: %lu (com keep-alive fica perto de 1) | "
                "ultimo envio levou %lu ms | ciclos pulados: %lu | intervalo %lu ms\n",
                (unsigned long)s.conexoes, (unsigned long)s.duracaoMs,
                (unsigned long)s.ciclosPulados, (unsigned long)NUVEM_INTERVALO_MS);
  const EstadoCompartilhado e = tarefa_audio_copiar();
  if (s.temConfig) {
    Serial.printf("[nuvem] configuracao recebida: limite %d dB, X %d s | em uso: limite %d dB, X %d s\n",
                  s.limiteRecebido, s.segundosRecebidos, (int)e.limiteDb, e.tempoXs);
  } else {
    Serial.printf("[nuvem] configuracao recebida: nenhuma ainda | em uso: limite %d dB, X %d s\n",
                  (int)e.limiteDb, e.tempoXs);
  }
  Serial.printf("[nuvem] pilha livre minima da tarefa: %lu de %lu bytes | memoria livre %lu (minima %lu)\n",
                (unsigned long)uxTaskGetStackHighWaterMark(tarefa), (unsigned long)PILHA_TAREFA_NUVEM,
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap());
}

#else
// NUVEM_ATIVA 0: nada é compilado (nem o HTTPS).
void nuvem_iniciar() { Serial.println("[nuvem] desligada (NUVEM_ATIVA 0)"); }
void nuvem_atualizar(uint32_t) {}
void nuvem_imprimir_status() { Serial.println("[nuvem] desligada (NUVEM_ATIVA 0)"); }
#endif
