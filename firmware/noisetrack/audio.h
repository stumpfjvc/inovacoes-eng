// NoiseTrack: fonte de áudio (1 ou 2 microfones INMP441 por I2S, ou fonte simulada).
// Entrega janelas de amostras cruas; o cálculo de dB fica na Parte 5.
#pragma once
#include <stdint.h>

// Prepara a fonte de áudio. Devolve false se o I2S não puder ser iniciado.
bool audio_iniciar();

// Preenche uma janela de AMOSTRAS_POR_JANELA amostras por canal (config.h).
// canalA = mic 1 (esquerdo); canalB = mic 2 (direito).
// Com NUM_MICROFONES = 1, só canalA é preenchido e canalB pode ser nullptr.
// Devolve false se a leitura falhar; nesse caso a janela não deve ser usada.
//
// Na fonte real, espera dentro do i2s_read até as amostras chegarem
// (cerca de JANELA_MS). Na simulada, devolve na hora.
bool audio_ler_janela(int32_t* canalA, int32_t* canalB);
