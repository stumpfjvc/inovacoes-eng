// NoiseTrack: módulo de buzzer ativo de 3 pinos (VCC, I/O, GND).
#pragma once
#include <stdint.h>

// Configura o pino já no nível "desligado" (sem apito ao ligar).
void buzzer_iniciar();

// Chamar uma vez por janela. Com alerta = true, bipa em pulsos
// (BUZZER_PULSO_MS ligado, BUZZER_PAUSA_MS desligado); com false, fica em silêncio.
// Não usa delay().
void buzzer_atualizar(bool alerta, uint32_t agora);

// true se o som está ligado agora ou foi desligado depois do instante "desde".
// A tarefa de áudio usa isto para saber se uma janela pode ter ouvido o apito.
bool buzzer_tocou_desde(uint32_t desde);
