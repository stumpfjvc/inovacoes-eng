// NoiseTrack: firmware do medidor de ruído (ESP32)
// Ver README.md e o plano de implementação por partes.
//
// Regra geral: o loop() nunca usa delay() nem fica esperando rede.
// Cada tarefa roda quando chega a hora dela, medida com millis().

#include "config.h"

static uint32_t tUltimoBatimento = 0;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== NoiseTrack: firmware em desenvolvimento ===");
  Serial.printf("Limite padrao: %d dB | Tempo X padrao: %d s\n",
                LIMITE_PADRAO_DB, TEMPO_X_PADRAO_S);
}

void loop() {
  const uint32_t agora = millis();

  // Mensagem de "vivo": mostra que o loop continua rodando sem travar.
  if (agora - tUltimoBatimento >= BATIMENTO_MS) {
    tUltimoBatimento = agora;
    Serial.printf("[vivo] %lu ms\n", (unsigned long)agora);
  }
}
