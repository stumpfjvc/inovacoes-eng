// NoiseTrack: autoteste da lógica de rede. Simula o tempo de 100 em 100 ms.
#include "teste_wifi.h"
#include "config.h"

#if TESTE_WIFI
#include "rede_logica.h"
#include <string.h>

static constexpr uint32_t PASSO_MS = 100;   // o rede.cpp roda a máquina a cada 100 ms

// Uma simulação: a máquina, o relógio falso, a entrada e o que ela mandou fazer.
struct SimRede {
  MaquinaRede m;
  uint32_t    agora = 0;
  EntradaRede e     = {};
  int         tentativas = 0;
  uint32_t    tTentativa[8] = {};
  bool        ligouAp = false, desligouAp = false, conectou = false, desconectou = false;
  bool        tLigouApMarcado = false;
  uint32_t    tLigouAp = 0;

  void zerarContagem() {
    tentativas = 0;
    ligouAp = desligouAp = conectou = desconectou = false;
    tLigouApMarcado = false;
  }
};

// Roda a máquina por "duracaoMs", um passo de PASSO_MS por vez.
static void rodar(SimRede& s, uint32_t duracaoMs) {
  for (uint32_t t = 0; t < duracaoMs; t += PASSO_MS) {
    const DecisaoRede d = s.m.atualizar(s.e, s.agora);
    if (d.tentarConectar) {
      if (s.tentativas < 8) s.tTentativa[s.tentativas] = s.agora;
      s.tentativas++;
    }
    if (d.ligarAp) {
      s.ligouAp = true;
      if (!s.tLigouApMarcado) { s.tLigouAp = s.agora; s.tLigouApMarcado = true; }
    }
    if (d.desligarAp)       s.desligouAp  = true;
    if (d.acabouDeConectar) s.conectou    = true;
    if (d.desconectar)      s.desconectou = true;
    s.e.credencialNova = false;   // o aviso de rede nova vale um passo só
    s.agora += PASSO_MS;
  }
}

static int falhas = 0;

static void conferir(const char* nome, bool ok) {
  if (!ok) falhas++;
  Serial.printf("[teste-wifi] %s: %s\n", nome, ok ? "PASS" : "FAIL");
}

static bool perto(uint32_t a, uint32_t b, uint32_t tol) { return (a > b ? a - b : b - a) <= tol; }

// (a) Sem credencial -> AP; salva a rede -> conecta; cai; tenta a cada
// WIFI_RETRY_MS; volta. "inicio" permite começar perto da volta do millis().
static void casoCicloCompleto(uint32_t inicio, const char* sufixo) {
  char nome[72];
  SimRede s;
  s.agora = inicio;
  s.e.permitirAp = true;

  rodar(s, PASSO_MS);
  snprintf(nome, sizeof(nome), "a) sem credencial vai a AP_SETUP%s", sufixo);
  conferir(nome, s.m.estado() == EstadoRede::AP_SETUP && s.ligouAp);

  s.zerarContagem();
  s.e.temCredencial = true;
  s.e.credencialNova = true;   // o portal salvou uma rede
  rodar(s, PASSO_MS);
  const bool tentou = s.m.estado() == EstadoRede::CONECTANDO && s.tentativas == 1;
  s.e.conectado = true;
  rodar(s, PASSO_MS);
  snprintf(nome, sizeof(nome), "a) conecta e desliga o AP%s", sufixo);
  conferir(nome, tentou && s.m.estado() == EstadoRede::CONECTADO && s.conectou && s.desligouAp);

  s.zerarContagem();
  s.e.conectado = false;       // caiu
  const uint32_t tQueda = s.agora;
  rodar(s, 3 * WIFI_RETRY_MS + PASSO_MS);
  bool espacado = s.tentativas == 3 &&
                  perto((uint32_t)(s.tTentativa[0] - tQueda), WIFI_RETRY_MS, PASSO_MS);
  for (int i = 1; i < 3 && espacado; i++) {
    espacado = perto((uint32_t)(s.tTentativa[i] - s.tTentativa[i - 1]), WIFI_RETRY_MS, PASSO_MS);
  }
  snprintf(nome, sizeof(nome), "a) caiu: tenta a cada WIFI_RETRY_MS (%d tentativas)%s",
           s.tentativas, sufixo);
  conferir(nome, s.m.estado() == EstadoRede::CAIU && espacado && !s.ligouAp);

  s.zerarContagem();
  s.e.conectado = true;
  rodar(s, PASSO_MS);
  snprintf(nome, sizeof(nome), "a) volta a CONECTADO%s", sufixo);
  conferir(nome, s.m.estado() == EstadoRede::CONECTADO && s.conectou);
}

bool teste_wifi_rodar() {
  falhas = 0;
  Serial.println("[teste-wifi] === autoteste da logica de rede ===");

  // ---------- (a) máquina de estados ----------
  casoCicloCompleto(0, "");
  // Começa 7 s antes da volta: a queda e as tentativas atravessam o zero.
  casoCicloCompleto(4294967295UL - 7000, " [volta do millis]");

  {  // Rede salva que não responde: liga o AP depois de WIFI_TIMEOUT_MS.
    SimRede s;
    s.e.temCredencial = true;
    s.e.permitirAp = true;
    rodar(s, WIFI_TIMEOUT_MS + 2 * PASSO_MS);
    conferir("a) sem conexao em WIFI_TIMEOUT_MS liga o AP",
             s.m.estado() == EstadoRede::AP_SETUP && s.tLigouApMarcado &&
             perto(s.tLigouAp, WIFI_TIMEOUT_MS, PASSO_MS));

    // Com o AP ligado e ninguém nele: tenta a rede salva a cada WIFI_RETRY_AP_MS.
    s.zerarContagem();
    rodar(s, 2 * WIFI_RETRY_AP_MS + PASSO_MS);
    const int semCliente = s.tentativas;
    // Com alguém no portal: não tenta (cada tentativa atrapalharia o AP).
    s.zerarContagem();
    s.e.clientesNoAp = true;
    rodar(s, 2 * WIFI_RETRY_AP_MS + PASSO_MS);
    conferir("a) AP ligado: tenta a cada WIFI_RETRY_AP_MS, e nao com alguem no portal",
             semCliente == 2 && s.tentativas == 0 && s.m.estado() == EstadoRede::AP_SETUP);
  }

  {  // Wokwi (AP não permitido): nunca liga o AP, só tenta de novo.
    SimRede s;
    s.e.temCredencial = true;
    s.e.permitirAp = false;
    rodar(s, WIFI_TIMEOUT_MS + 2 * WIFI_RETRY_MS + PASSO_MS);
    conferir("a) Wokwi: sem AP, tenta de novo", !s.ligouAp &&
             s.m.estado() == EstadoRede::CAIU && s.tentativas == 3);
  }

  {  // "wifi apagar" estando conectado: desconecta e liga o AP.
    SimRede s;
    s.e.temCredencial = true;
    s.e.permitirAp = true;
    s.e.conectado = true;
    rodar(s, 2 * PASSO_MS);
    s.zerarContagem();
    s.e.temCredencial = false;
    s.e.conectado = false;
    rodar(s, PASSO_MS);
    conferir("a) apagar a rede desconecta e liga o AP",
             s.desconectou && s.ligouAp && s.m.estado() == EstadoRede::AP_SETUP);
  }

  // ---------- (b) validação do portal ----------
  char texto[80];
  memset(texto, 'a', sizeof(texto));
  texto[32] = '\0';
  const bool ssid32 = rede_ssid_valido(texto);
  texto[32] = 'a';
  texto[33] = '\0';
  const bool ssid33 = rede_ssid_valido(texto);
  conferir("b) nome da rede de 1 a 32 caracteres",
           !rede_ssid_valido("") && rede_ssid_valido("a") && ssid32 && !ssid33 &&
           !rede_ssid_valido("rede\n") && !rede_ssid_valido(nullptr));

  memset(texto, 'b', sizeof(texto));
  texto[63] = '\0';
  const bool senha63 = rede_senha_valida(texto);
  texto[63] = 'b';
  texto[64] = '\0';
  const bool senha64 = rede_senha_valida(texto);
  conferir("b) senha vazia ou de 8 a 63 caracteres",
           rede_senha_valida("") && !rede_senha_valida("1234567") &&
           rede_senha_valida("12345678") && senha63 && !senha64 &&
           !rede_senha_valida("senha\tcom tab"));

  // ---------- (c) senha gerada do AP ----------
  const uint8_t mac1[6] = {0x24, 0x0A, 0xC4, 0x12, 0x34, 0x56};
  const uint8_t mac2[6] = {0x24, 0x0A, 0xC4, 0x12, 0x34, 0x57};   // só o último byte muda
  char s1[REDE_TAM_SENHA_AP + 1], s1b[REDE_TAM_SENHA_AP + 1];
  char s2[REDE_TAM_SENHA_AP + 1], s3[REDE_TAM_SENHA_AP + 1];
  rede_gerar_senha_ap(mac1, "segredo-de-teste", s1);
  rede_gerar_senha_ap(mac1, "segredo-de-teste", s1b);
  rede_gerar_senha_ap(mac2, "segredo-de-teste", s2);
  rede_gerar_senha_ap(mac1, "segredo-de-testf", s3);

  bool alfabetoOk = strlen(s1) == REDE_TAM_SENHA_AP;
  for (size_t i = 0; i < strlen(s1) && alfabetoOk; i++) {
    alfabetoOk = strchr(REDE_ALFABETO_SENHA, s1[i]) != nullptr && strchr("0Oo1lI", s1[i]) == nullptr;
  }
  Serial.printf("[teste-wifi] (senha de exemplo: %s)\n", s1);
  conferir("c) 8 caracteres do alfabeto permitido", alfabetoOk);
  conferir("c) mesmo MAC e segredo: mesma senha", strcmp(s1, s1b) == 0);
  conferir("c) outro MAC: outra senha", strcmp(s1, s2) != 0);
  conferir("c) outro segredo: outra senha", strcmp(s1, s3) != 0);

  Serial.printf("[teste-wifi] === %s (%d falha(s)) ===\n",
                falhas == 0 ? "TUDO PASS" : "HOUVE FAIL", falhas);
  return falhas == 0;
}

#else
bool teste_wifi_rodar() { return true; }
#endif
