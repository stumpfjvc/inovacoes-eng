// NoiseTrack: módulo de buzzer ativo de 3 pinos (VCC, I/O, GND).
#pragma once
#include <stdint.h>

// Configura o pino já no nível "desligado" (sem apito ao ligar).
void buzzer_iniciar();

// Chamar a cada volta do loop. Com alerta = true, bipa de forma intermitente
// (BUZZER_BIPE_MS ligado, BUZZER_BIPE_MS desligado); com false, fica em silêncio.
// Não usa delay().
void buzzer_atualizar(bool alerta, uint32_t agora);
