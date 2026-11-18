// NoiseTrack: módulo de LED RGB WS2812 (4 LEDs).
#pragma once
#include "estado.h"

// Prepara o módulo e deixa os LEDs apagados.
void leds_iniciar();

// Mostra a cor do estado: verde = calmo, amarelo = contando, vermelho = alerta.
// Só envia dados ao módulo quando o estado muda.
void leds_mostrar(Estado e);
