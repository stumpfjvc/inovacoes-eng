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

static uint32_t tUltimoBatimento = 0;

#if DEBUG_AUDIO
// Teste da Parte 4: lê uma janela a cada JANELA_MS e imprime o RMS BRUTO de
// cada canal (sem dB; isso é a Parte 5). Remover na Parte 7.
static int32_t janelaA[AMOSTRAS_POR_JANELA];
#if NUM_MICROFONES == 2
static int32_t  janelaB[AMOSTRAS_POR_JANELA];
static int32_t* const ptrJanelaB = janelaB;
#else
static int32_t* const ptrJanelaB = nullptr;   // com 1 microfone, não há canal B
#endif
static uint32_t tUltimaJanela = 0;

// RMS = raiz da média dos quadrados das amostras.
static double rmsBruto(const int32_t* amostras, size_t n) {
  uint64_t soma = 0;   // cabe com folga: 2000 x (2^23)^2 < 2^63
  for (size_t i = 0; i < n; i++) {
    const int64_t a = amostras[i];
    soma += (uint64_t)(a * a);
  }
  return sqrt((double)soma / n);
}

static void depurarAudio(uint32_t agora) {
  if (agora - tUltimaJanela < JANELA_MS) return;
  tUltimaJanela = agora;

  if (!audio_ler_janela(janelaA, ptrJanelaB)) {
    Serial.println("[audio] janela descartada");
    return;
  }
#if NUM_MICROFONES == 2
  Serial.printf("[audio] RMS A = %.0f | RMS B = %.0f\n",
                rmsBruto(janelaA, AMOSTRAS_POR_JANELA),
                rmsBruto(janelaB, AMOSTRAS_POR_JANELA));
#else
  Serial.printf("[audio] RMS A = %.0f\n", rmsBruto(janelaA, AMOSTRAS_POR_JANELA));
#endif
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

#if MODO_DEMO_SAIDAS
  atualizarDemo(agora);
#endif

#if DEBUG_AUDIO
  depurarAudio(agora);
#endif
}
