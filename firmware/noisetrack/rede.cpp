// NoiseTrack: Wi-Fi, AP de setup com portal, mDNS e NTP (Parte 9).
// As decisões (para qual estado ir, quando tentar de novo) ficam em
// rede_logica.cpp, que é puro e testável. Aqui só se executa o que ela manda.
#include "rede.h"
#include "config.h"
#include "armazenamento.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <time.h>
#include <esp_mac.h>

// Segredo da senha do AP: vem do secrets.h (fora do Git), se existir.
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_AP_SEGREDO
#define WIFI_AP_SEGREDO "noisetrack-segredo-padrao"
#define WIFI_AP_SEGREDO_PADRAO 1
#endif

static constexpr uint32_t PASSO_MAQUINA_MS = 100;    // a máquina de estados roda a cada 100 ms
static constexpr uint32_t PASSO_NTP_MS     = 1000;   // confere o relógio a cada 1 s
static constexpr time_t   HORARIO_VALIDO   = 1700000000;   // nov/2023: antes disso, relógio ainda zerado
static constexpr const char* WOKWI_SSID    = "Wokwi-GUEST";
static constexpr int32_t     WOKWI_CANAL   = 6;

static MaquinaRede maquina;
static WebServer   servidor(80);
static DNSServer   dns;
static bool        portalAtivo = false;

static char ssid[WIFI_TAM_SSID];
static char senha[WIFI_TAM_SENHA];
static bool temCredencial  = false;
static bool credencialNova = false;
static char senhaAp[REDE_TAM_SENHA_AP + 1];

static uint32_t tUltimoPasso    = 0;
static uint32_t tUltimoNtp      = 0;
static bool     mdnsIniciado    = false;
static bool     ntpIniciado     = false;
static bool     ntpSincronizado = false;

// ---------- Credenciais ----------
static void carregarCredenciais() {
#if USANDO_WOKWI
  strncpy(ssid, WOKWI_SSID, sizeof(ssid));
  senha[0] = '\0';
  temCredencial = true;   // no Wokwi a rede é sempre a Wokwi-GUEST
#else
  temCredencial = armazenamento_wifi_ler(ssid, sizeof(ssid), senha, sizeof(senha));
#endif
}

// ---------- Portal (só com o AP de setup ligado) ----------
// Troca caracteres especiais para o nome da rede não quebrar o HTML da página.
static String escaparHtml(const char* s) {
  String r;
  for (; *s; s++) {
    switch (*s) {
      case '&':  r += "&amp;";  break;
      case '<':  r += "&lt;";   break;
      case '>':  r += "&gt;";   break;
      case '"':  r += "&quot;"; break;
      case '\'': r += "&#39;";  break;
      default:   r += *s;
    }
  }
  return r;
}

static const char HTML_INICIO[] =
    "<!doctype html><html lang=\"pt-BR\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>NoiseTrack</title><style>"
    "body{font-family:sans-serif;max-width:22rem;margin:2rem auto;padding:0 1rem}"
    "input,button{width:100%;padding:.6rem;margin:.3rem 0 1rem;font-size:1rem;box-sizing:border-box}"
    "</style></head><body><h1>NoiseTrack</h1>";
static const char HTML_FIM[] = "</body></html>";

static void enviarPagina(int codigo, const String& corpo) {
  servidor.send(codigo, "text/html; charset=utf-8", String(HTML_INICIO) + corpo + HTML_FIM);
}

static void paginaInicio() {
  String corpo =
      "<p>Escolha o Wi-Fi do local.</p>"
      "<form method=\"post\" action=\"/salvar\">"
      "<label>Nome da rede<input name=\"ssid\" maxlength=\"32\" required></label>"
      "<label>Senha (deixe vazia se a rede for aberta)"
      "<input name=\"senha\" type=\"password\" maxlength=\"63\"></label>"
      "<button>Salvar e conectar</button></form><p><small>Estado: ";
  corpo += nomeEstadoRede(maquina.estado());
  if (temCredencial) {
    corpo += " | rede salva: ";
    corpo += escaparHtml(ssid);
  }
  corpo += "</small></p>";
  enviarPagina(200, corpo);
}

static void salvarPortal() {
  const String novoSsid  = servidor.arg("ssid");
  const String novaSenha = servidor.arg("senha");
  const ResultadoSalvar r = armazenamento_wifi_salvar(novoSsid.c_str(), novaSenha.c_str());
  if (!r.aceito) {
    enviarPagina(400, "<p>Nome da rede (1 a 32 caracteres) ou senha (vazia, ou de 8 a 63) "
                      "inv&aacute;lidos.</p><p><a href=\"/\">Voltar</a></p>");
    return;
  }
  if (r.erroFlash) {
    enviarPagina(500, "<p>Erro ao gravar na mem&oacute;ria.</p><p><a href=\"/\">Voltar</a></p>");
    return;
  }
  rede_credenciais_mudaram();
  Serial.printf("[rede] portal salvou a rede '%s' (%s)\n", ssid,
                r.gravou ? "gravada" : "igual a salva");   // a senha nunca é impressa
  enviarPagina(200, "<p>Salvo. Conectando a <b>" + escaparHtml(ssid) + "</b>.</p>"
                    "<p>Quando conectar, a rede " + String(WIFI_AP_NOME) + " some e o aparelho "
                    "fica em <b>http://" + String(MDNS_NOME) + ".local</b>. Se n&atilde;o "
                    "conectar, ela volta e d&aacute; para tentar de novo.</p>");
}

// Qualquer outro endereço vai para a página inicial. É isso que faz o Android e
// o iPhone abrirem o portal sozinhos ("Fazer login na rede").
static void redirecionar() {
  servidor.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
  servidor.send(302, "text/plain", "");
}

// ---------- Ações pedidas pela máquina de estados ----------
static void conectar() {
  WiFi.disconnect();   // cancela uma tentativa anterior, se houver (não espera)
#if USANDO_WOKWI
  WiFi.begin(WOKWI_SSID, "", WOKWI_CANAL);
#else
  WiFi.begin(ssid, senha[0] != '\0' ? senha : nullptr);
#endif
  Serial.printf("[rede] tentando conectar a '%s'\n", ssid);
}

static void ligarAp() {
  WiFi.mode(WIFI_AP_STA);   // AP e cliente juntos: continua podendo tentar a rede salva
  WiFi.softAP(WIFI_AP_NOME, senhaAp);
  const IPAddress ip = WiFi.softAPIP();
  dns.start(53, "*", ip);   // responde a qualquer nome com o IP do portal
  servidor.begin();
  portalAtivo = true;
  Serial.printf("[rede] AP de setup ligado: '%s', portal em http://%s/\n",
                WIFI_AP_NOME, ip.toString().c_str());
}

static void desligarAp() {
  portalAtivo = false;
  servidor.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);   // desliga só o AP; o Wi-Fi do local continua
  Serial.println("[rede] AP de setup desligado");
}

static void aoConectar() {
  Serial.printf("[rede] conectado a '%s' | IP %s | http://%s.local\n",
                ssid, WiFi.localIP().toString().c_str(), MDNS_NOME);
  if (!mdnsIniciado) {
    mdnsIniciado = MDNS.begin(MDNS_NOME);   // só responde na rede local
    if (!mdnsIniciado) Serial.println("[rede] ERRO: mDNS nao iniciou");
  }
  if (!ntpIniciado) {
    // Só agenda: o pedido ao servidor de horário acontece em segundo plano.
    configTime(FUSO_HORARIO_S, 0, NTP_SERVIDOR_1, NTP_SERVIDOR_2);
    ntpIniciado = true;
    Serial.println("[ntp] pedindo o horario (sem internet, segue sem horario)");
  }
}

static void conferirNtp(uint32_t agora) {
  if (!ntpIniciado || ntpSincronizado) return;
  if ((uint32_t)(agora - tUltimoNtp) < PASSO_NTP_MS) return;
  tUltimoNtp = agora;
  const time_t t = time(nullptr);   // só lê o relógio; não espera a rede
  if (t < HORARIO_VALIDO) return;
  ntpSincronizado = true;
  struct tm hora;
  localtime_r(&t, &hora);
  char texto[24];
  strftime(texto, sizeof(texto), "%d/%m/%Y %H:%M:%S", &hora);
  Serial.printf("[ntp] horario acertado: %s (UTC-3)\n", texto);
}

// ---------- Funções públicas ----------
void rede_iniciar() {
  WiFi.persistent(false);         // o núcleo não grava a rede na flash por conta própria
  WiFi.setAutoReconnect(false);   // quem decide reconectar é a máquina de estados
  WiFi.setHostname(MDNS_NOME);    // nome do aparelho no roteador (rede local)
  WiFi.mode(WIFI_STA);

  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  rede_gerar_senha_ap(mac, WIFI_AP_SEGREDO, senhaAp);

  char etiqueta[64];
  rede_texto_etiqueta(etiqueta, sizeof(etiqueta));
  Serial.printf("[rede] %s\n", etiqueta);
#ifdef WIFI_AP_SEGREDO_PADRAO
  Serial.println("[rede] AVISO: sem secrets.h, a senha do AP usa o segredo padrao e e previsivel");
#endif
#if USANDO_WOKWI
  Serial.println("[rede] Wokwi: sem portal, conectando direto a 'Wokwi-GUEST'");
#endif

  servidor.on("/", HTTP_GET, paginaInicio);
  servidor.on("/salvar", HTTP_POST, salvarPortal);
  servidor.onNotFound(redirecionar);

  carregarCredenciais();
  if (!temCredencial) Serial.println("[rede] nenhuma rede salva: o AP de setup vai ligar");
}

void rede_atualizar(uint32_t agora) {
  if (portalAtivo) {          // atende quem estiver no portal (não espera se não houver ninguém)
    dns.processNextRequest();
    servidor.handleClient();
  }

  if ((uint32_t)(agora - tUltimoPasso) >= PASSO_MAQUINA_MS) {
    tUltimoPasso = agora;
    EntradaRede e = {};
    e.temCredencial  = temCredencial;
    e.conectado      = WiFi.status() == WL_CONNECTED;   // só lê o estado; não espera
    e.clientesNoAp   = portalAtivo && WiFi.softAPgetStationNum() > 0;
    e.credencialNova = credencialNova;
    e.permitirAp     = (USANDO_WOKWI == 0);
    credencialNova   = false;

    const DecisaoRede d = maquina.atualizar(e, agora);
    if (d.mudou) Serial.printf("[rede] estado=%s\n", nomeEstadoRede(d.estado));
    if (d.desconectar)      WiFi.disconnect();
    if (d.ligarAp)          ligarAp();
    if (d.tentarConectar)   conectar();
    if (d.desligarAp)       desligarAp();
    if (d.acabouDeConectar) aoConectar();
  }

  conferirNtp(agora);
}

EstadoRede rede_estado() { return maquina.estado(); }

void rede_credenciais_mudaram() {
  carregarCredenciais();
  credencialNova = true;   // a máquina recomeça a conexão no próximo passo
}

void rede_imprimir_status() {
  Serial.printf("[wifi] estado=%s | rede salva: %s", nomeEstadoRede(maquina.estado()),
                temCredencial ? ssid : "(nenhuma)");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf(" | IP %s | sinal %d dBm", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }
  if (maquina.apLigado()) {
    Serial.printf(" | AP ligado (%d conectado(s))", WiFi.softAPgetStationNum());
  }
  if (ntpSincronizado) {
    const time_t t = time(nullptr);
    struct tm hora;
    localtime_r(&t, &hora);
    char texto[24];
    strftime(texto, sizeof(texto), "%d/%m/%Y %H:%M:%S", &hora);
    Serial.printf(" | %s", texto);
  } else {
    Serial.print(" | sem horario");
  }
  Serial.println();
}

void rede_texto_etiqueta(char* saida, size_t tam) {
  snprintf(saida, tam, "Rede: %s | Senha: %s", WIFI_AP_NOME, senhaAp);
}
