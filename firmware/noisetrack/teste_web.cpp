// NoiseTrack: autoteste da parte pura da página. Não usa a rede.
#include "teste_web.h"
#include "config.h"

#if TESTE_WEB
#include "web_logica.h"
#include "rede_logica.h"
#include <string.h>

static int falhas = 0;

static void conferir(const char* nome, bool ok) {
  if (!ok) falhas++;
  Serial.printf("[teste-web] %s: %s\n", nome, ok ? "PASS" : "FAIL");
}

// Bloqueio por tentativas erradas, a partir de um instante qualquer.
static void casoBloqueio(uint32_t inicio, const char* sufixo) {
  char nome[80];
  BloqueioSenha b;
  for (int i = 0; i < WEB_TENTATIVAS_MAX - 1; i++) b.registrarErro(inicio);
  const bool livreCom4 = !b.bloqueado(inicio) && b.restantes() == 1;
  b.registrarErro(inicio);   // a 5ª seguida bloqueia
  const bool bloqueou = b.bloqueado(inicio);
  const bool aindaBloqueado = b.bloqueado(inicio + WEB_BLOQUEIO_MS - 1);
  const bool esperaCerta = b.esperaMs(inicio + WEB_BLOQUEIO_MS / 2) == WEB_BLOQUEIO_MS / 2;
  const bool liberou = !b.bloqueado(inicio + WEB_BLOQUEIO_MS) &&
                       b.restantes() == WEB_TENTATIVAS_MAX;
  snprintf(nome, sizeof(nome), "bloqueio: 5 erradas bloqueiam por 60 s%s", sufixo);
  conferir(nome, livreCom4 && bloqueou && aindaBloqueado && esperaCerta && liberou);
}

bool teste_web_rodar() {
  falhas = 0;
  Serial.println("[teste-web] === autoteste da pagina ===");
  char json[WEB_TAM_JSON];

  // ---------- JSON ----------
  {
    EstadoCompartilhado e = {};
    e.dbCombinado = 63.2f;  e.validoCombinado = true;
    e.dbA = 62.9f;          e.validoA = true;
    e.dbB = 63.5f;          e.validoB = true;
    e.estado = Estado::CONTANDO;
    e.acimaMs = 1250;
    e.limiteDb = 65.0f;     e.tempoXs = 5;
    e.ultimoEvento = EventoRegra::DISPAROU;
    e.contadorEventos = 3;
    web_montar_json(e, "noisetrack-a1b2c3", json, sizeof(json));
    const char* esperado =
        "{\"id\":\"noisetrack-a1b2c3\",\"db\":63.2,\"a\":62.9,\"b\":63.5,\"valido\":true,"
        "\"estado\":\"contando\",\"acima_ms\":1250,\"limite\":65,\"x\":5,"
        "\"evento\":\"disparou\",\"evento_n\":3}";
    conferir("JSON completo", strcmp(json, esperado) == 0);
    if (strcmp(json, esperado) != 0) Serial.printf("[teste-web]   obtido: %s\n", json);
  }
  {
    EstadoCompartilhado e = {};   // nenhum microfone com sinal
    e.estado = Estado::SEM_SINAL;
    e.limiteDb = 70.0f;  e.tempoXs = 8;
    e.ultimoEvento = EventoRegra::PERDEU_SINAL;
    e.contadorEventos = 1;
    web_montar_json(e, "x", json, sizeof(json));
    const char* esperado =
        "{\"id\":\"x\",\"db\":null,\"a\":null,\"b\":null,\"valido\":false,"
        "\"estado\":\"sem_sinal\",\"acima_ms\":0,\"limite\":70,\"x\":8,"
        "\"evento\":\"perdeu_sinal\",\"evento_n\":1}";
    conferir("JSON sem sinal (null)", strcmp(json, esperado) == 0);
    if (strcmp(json, esperado) != 0) Serial.printf("[teste-web]   obtido: %s\n", json);

    char pequeno[40];
    conferir("JSON que nao cabe devolve 0", web_montar_json(e, "x", pequeno, sizeof(pequeno)) == 0);
  }

  // ---------- Validação do formulário ----------
  {
    int l = 0, x = 0;
    const bool bons = web_ler_config("30", "1", &l, &x) && l == 30 && x == 1 &&
                      web_ler_config("120", "600", &l, &x) && l == 120 && x == 600 &&
                      web_ler_config("70", "8", &l, &x) && l == 70 && x == 8;
    const bool ruins = !web_ler_config("29", "5", &l, &x) && !web_ler_config("121", "5", &l, &x) &&
                       !web_ler_config("70", "0", &l, &x) && !web_ler_config("70", "601", &l, &x) &&
                       !web_ler_config("abc", "5", &l, &x) && !web_ler_config("70.5", "5", &l, &x) &&
                       !web_ler_config("", "5", &l, &x) && !web_ler_config("70", "5x", &l, &x) &&
                       !web_ler_config(nullptr, "5", &l, &x);
    conferir("validacao de limite (30-120) e X (1-600)", bons && ruins);
  }

  // ---------- Senha ----------
  conferir("senha: so a exata confere",
           web_senha_confere("abcdefgh", "abcdefgh") && !web_senha_confere("abcdefgx", "abcdefgh") &&
           !web_senha_confere("abcdefg", "abcdefgh") && !web_senha_confere("abcdefghi", "abcdefgh") &&
           !web_senha_confere("", "abcdefgh") && !web_senha_confere(nullptr, "abcdefgh"));

  {
    const uint8_t mac[6] = {0x24, 0x0A, 0xC4, 0x12, 0x34, 0x56};
    char ap[REDE_TAM_SENHA_AP + 1], semPrefixo[REDE_TAM_SENHA_AP + 1], cfg[REDE_TAM_SENHA_AP + 1];
    rede_gerar_senha_ap(mac, "segredo", ap);
    rede_gerar_senha(mac, "", "segredo", semPrefixo);
    rede_gerar_senha(mac, REDE_PREFIXO_SENHA_CONFIG, "segredo", cfg);
    conferir("senha de configuracao diferente da do AP",
             strcmp(ap, semPrefixo) == 0 && strcmp(ap, cfg) != 0 && strlen(cfg) == REDE_TAM_SENHA_AP);
  }

  // ---------- Bloqueio por tentativas ----------
  casoBloqueio(0, "");
  casoBloqueio(4294967295UL - WEB_BLOQUEIO_MS / 2, " [volta do millis]");
  {
    BloqueioSenha b;
    for (int i = 0; i < WEB_TENTATIVAS_MAX - 1; i++) b.registrarErro(1000);
    b.registrarAcerto();   // uma certa no meio zera a contagem
    for (int i = 0; i < WEB_TENTATIVAS_MAX - 1; i++) b.registrarErro(2000);
    conferir("bloqueio: senha certa zera a contagem", !b.bloqueado(2000));
  }

  // ---------- WebSocket ----------
  {
    char chave[32];
    const bool achou = ws_extrair_chave("Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==", chave,
                                        sizeof(chave)) &&
                       strcmp(chave, "dGhlIHNhbXBsZSBub25jZQ==") == 0;
    const bool minusc = ws_extrair_chave("sec-websocket-key:abc==  ", chave, sizeof(chave)) &&
                        strcmp(chave, "abc==") == 0;
    const bool outra = !ws_extrair_chave("Host: 192.168.0.10:81", chave, sizeof(chave));
    conferir("WebSocket: le a chave do pedido", achou && minusc && outra);

    // Exemplo oficial do protocolo (RFC 6455, seção 1.3).
    char aceite[32];
    conferir("WebSocket: resposta do aperto de mao (RFC 6455)",
             ws_calcular_aceite("dGhlIHNhbXBsZSBub25jZQ==", aceite, sizeof(aceite)) &&
             strcmp(aceite, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") == 0);

    uint8_t c[4];
    const bool curto  = ws_cabecalho_texto(5, c) == 2 && c[0] == 0x81 && c[1] == 5;
    const bool limite = ws_cabecalho_texto(125, c) == 2 && c[1] == 125;
    const bool medio  = ws_cabecalho_texto(126, c) == 4 && c[1] == 126 && c[2] == 0 && c[3] == 126;
    const bool maior  = ws_cabecalho_texto(300, c) == 4 && c[2] == 0x01 && c[3] == 0x2C;
    conferir("WebSocket: cabecalho do quadro de texto", curto && limite && medio && maior);
  }

  Serial.printf("[teste-web] === %s (%d falha(s)) ===\n",
                falhas == 0 ? "TUDO PASS" : "HOUVE FAIL", falhas);
  return falhas == 0;
}

#else
bool teste_web_rodar() { return true; }
#endif
