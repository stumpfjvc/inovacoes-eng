// NoiseTrack: estados do ambiente, usados pelos LEDs, pelo buzzer e (depois) pela página.
#pragma once
#include <stdint.h>

enum class Estado : uint8_t {
  CALMO,     // abaixo do limite                  -> verde
  CONTANDO,  // acima do limite, contando X       -> amarelo
  ALERTA,    // ficou X segundos acima do limite  -> vermelho
  SEM_SINAL  // nenhum microfone com sinal (erro) -> azul, sem alerta
};

inline const char* nomeEstado(Estado e) {
  switch (e) {
    case Estado::CALMO:     return "calmo";
    case Estado::CONTANDO:  return "contando";
    case Estado::ALERTA:    return "alerta";
    case Estado::SEM_SINAL: return "sem_sinal";
  }
  return "?";
}
