// NoiseTrack: firmware do medidor de ruído (ESP32)
// Ver README.md e o plano de implementação por partes.
//
// Regra geral: o loop() nunca usa delay() nem fica esperando rede.
// Cada tarefa roda quando chega a hora dela, medida com millis().

#include "config.h"
#include "estado.h"
#include "leds.h"

static uint32_t tUltimoBatimento = 0;

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
}
#endif

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== NoiseTrack: firmware em desenvolvimento ===");
  Serial.printf("Limite padrao: %d dB | Tempo X padrao: %d s\n",
                LIMITE_PADRAO_DB, TEMPO_X_PADRAO_S);

  leds_iniciar();
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
}
