// NoiseTrack: estados do ambiente, usados pelos LEDs, pelo buzzer e (depois) pela página.
#pragma once
#include <stdint.h>

enum class Estado : uint8_t {
  CALMO,     // abaixo do limite                  -> verde
  CONTANDO,  // acima do limite, contando X       -> amarelo
  ALERTA     // ficou X segundos acima do limite  -> vermelho
};

inline const char* nomeEstado(Estado e) {
  switch (e) {
    case Estado::CALMO:    return "calmo";
    case Estado::CONTANDO: return "contando";
    case Estado::ALERTA:   return "alerta";
  }
  return "?";
}
