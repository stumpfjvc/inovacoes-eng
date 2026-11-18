// NoiseTrack: firmware do medidor de ruído (ESP32)
// Ver README.md e o plano de implementação por partes.
//
// Regra geral: o loop() nunca usa delay() nem fica esperando rede.
// Cada tarefa roda quando chega a hora dela, medida com millis().

#include "config.h"
#include "estado.h"
#include "leds.h"
#include "buzzer.h"
#include "audio.h"
#include "nivel_db.h"
#include "teste_nivel_db.h"
#include "regra.h"
#include "teste_regra.h"

static uint32_t tUltimoBatimento = 0;

// Regra dos X segundos (Parte 6). Começa em CALMO.
static Regra regra;

#if DEBUG_AUDIO
// Lê uma janela a cada JANELA_MS, imprime o nível em dB de cada canal e o
// combinado, e alimenta a regra. Na Parte 7 isto vai para uma tarefa FreeRTOS.
static int32_t janelaA[AMOSTRAS_POR_JANELA];
#if NUM_MICROFONES == 2
static int32_t  janelaB[AMOSTRAS_POR_JANELA];
static int32_t* const ptrJanelaB = janelaB;
#else
static int32_t* const ptrJanelaB = nullptr;   // com 1 microfone, não há canal B
#endif
static uint32_t tUltimaJanela = 0;

// Escreve "xx.x dB" ou "--" (canal sem sinal ou ausente) em "texto".
static void formatarDb(char* texto, size_t tamanho, float db, bool valido) {
  if (valido) snprintf(texto, tamanho, "%.1f dB", db);
  else        snprintf(texto, tamanho, "--");
}

static void processarJanela(uint32_t agora) {
  if (agora - tUltimaJanela < JANELA_MS) return;
  tUltimaJanela = agora;

  if (!audio_ler_janela(janelaA, ptrJanelaB)) {
    Serial.println("[audio] janela descartada");
    return;
  }
  // Na fonte real, a leitura acima esperou ~125 ms: o instante certo da janela é agora.
  const uint32_t fimJanela = millis();
  const NivelDb nivel = nivel_calcular(janelaA, ptrJanelaB, AMOSTRAS_POR_JANELA);

  char textoA[16], textoB[16], textoComb[16];
  formatarDb(textoA, sizeof(textoA), nivel.dbA, nivel.validoA);
  formatarDb(textoB, sizeof(textoB), nivel.dbB, nivel.validoB);
  formatarDb(textoComb, sizeof(textoComb), nivel.dbCombinado, nivel.validoCombinado);
  Serial.printf("[nivel] A = %s | B = %s | comb = %s%s\n", textoA, textoB, textoComb,
                nivel.canalDescartado ? " (canal sem sinal fora da conta)" : "");

  // Limite e X ainda são os padrões; na Parte 8 virão da memória (Preferences).
  const RegraSaida r = regra.atualizar(nivel.dbCombinado, nivel.validoCombinado, fimJanela,
                                       LIMITE_PADRAO_DB, TEMPO_X_PADRAO_S);
  if (r.mudou) {
    Serial.printf("[regra] estado=%s acima=%lu ms", nomeEstado(r.estado),
                  (unsigned long)r.acimaMs);
    if (r.evento != EventoRegra::NENHUM) Serial.printf(" (%s)", nomeEvento(r.evento));
    Serial.println();
  }
}
#endif

#if MODO_DEMO_SAIDAS
// Demonstração: troca de estado a cada DEMO_PASSO_MS.
// Será substituída pela regra real dos X segundos (Partes 4–7).
static Estado   estadoDemo       = Estado::CALMO;
static uint32_t tUltimoPassoDemo = 0;

static void atualizarDemo(uint32_t agora) {
  if (agora - tUltimoPassoDemo >= DEMO_PASSO_MS) {
    tUltimoPassoDemo = agora;
    switch (estadoDemo) {
      case Estado::CALMO:    estadoDemo = Estado::CONTANDO; break;
      case Estado::CONTANDO: estadoDemo = Estado::ALERTA;   break;
      case Estado::ALERTA:   estadoDemo = Estado::CALMO;    break;
      case Estado::SEM_SINAL: estadoDemo = Estado::CALMO;   break;
    }
    Serial.printf("[demo] estado = %s\n", nomeEstado(estadoDemo));
  }
  leds_mostrar(estadoDemo);
  buzzer_atualizar(estadoDemo == Estado::ALERTA, agora);
}
#endif

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== NoiseTrack: firmware em desenvolvimento ===");
  Serial.printf("Limite padrao: %d dB | Tempo X padrao: %d s\n",
                LIMITE_PADRAO_DB, TEMPO_X_PADRAO_S);

  buzzer_iniciar();   // primeiro, para o buzzer não apitar durante o resto do setup
  leds_iniciar();

#if TESTE_NIVEL_DB
  teste_nivel_db_rodar();   // só imprime PASS/FAIL; não trava se falhar
#endif
#if TESTE_REGRA
  teste_regra_rodar();      // idem
#endif

  if (!audio_iniciar()) {
    Serial.println("[audio] ERRO: fonte de audio nao iniciou");
  }
}

void loop() {
  const uint32_t agora = millis();

  // Mensagem de "vivo": mostra que o loop continua rodando sem travar.
  if (agora - tUltimoBatimento >= BATIMENTO_MS) {
    tUltimoBatimento = agora;
    Serial.printf("[vivo] %lu ms\n", (unsigned long)agora);
  }

#if DEBUG_AUDIO
  processarJanela(agora);
#endif

#if MODO_DEMO_SAIDAS
  atualizarDemo(agora);
#else
  // LEDs e buzzer seguem o estado real da regra. Chamados a cada volta do loop
  // (não só a cada janela) para o bipe do buzzer manter o ritmo.
  leds_mostrar(regra.estado());
  buzzer_atualizar(regra.estado() == Estado::ALERTA, millis());
#endif
}
