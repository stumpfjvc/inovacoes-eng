// NoiseTrack: módulo de LED RGB WS2812 (4 LEDs), biblioteca Adafruit NeoPixel.
#include "leds.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel fita(NUM_LEDS, PINO_LEDS, NEO_GRB + NEO_KHZ800);

static bool   jaMostrou      = false;
static Estado estadoMostrado = Estado::CALMO;

static uint32_t corDoEstado(Estado e) {
  switch (e) {
    case Estado::CALMO:    return fita.Color(0, 255, 0);     // verde
    case Estado::CONTANDO: return fita.Color(255, 180, 0);   // amarelo (menos verde para não puxar para o limão)
    case Estado::ALERTA:   return fita.Color(255, 0, 0);     // vermelho
    case Estado::SEM_SINAL: return fita.Color(0, 0, 255);    // azul: erro, sem microfone
  }
  return 0;
}

void leds_iniciar() {
  fita.begin();
  fita.setBrightness(BRILHO_LEDS);   // ~30%: menos consumo e menos ofuscamento
  fita.clear();
  fita.show();
}

void leds_mostrar(Estado e) {
  if (jaMostrou && e == estadoMostrado) return;   // nada mudou: não reenvia
  fita.fill(corDoEstado(e));
  fita.show();
  estadoMostrado = e;
  jaMostrou = true;
}
