// NoiseTrack: regra dos X segundos contínuos (Parte 6).
// Máquina de estados pura: não usa Serial, hardware nem millis(). O instante
// atual entra como parâmetro, então dá para testar simulando o tempo.
#pragma once
#include <stdint.h>
#include "estado.h"

// Acontecimentos que a página (Parte 10) vai querer registrar.
enum class EventoRegra : uint8_t {
  NENHUM,
  DISPAROU,     // CONTANDO -> ALERTA
  NORMALIZOU,   // ALERTA -> CALMO, depois de HOLD_MS abaixo
  PERDEU_SINAL  // entrou em SEM_SINAL
};

const char* nomeEvento(EventoRegra e);

// Resultado de uma janela.
struct RegraSaida {
  Estado      estado;          // estado depois desta janela
  bool        mudou;           // o estado mudou nesta janela?
  EventoRegra evento;          // acontecimento desta janela (ou NENHUM)
  uint32_t    instanteEvento;  // "agora" da janela em que o evento ocorreu
  uint32_t    acimaMs;         // tempo acima do limite desde o início da contagem
};

// Funciona como uma classe Java: "Regra r;" cria o objeto (já em CALMO) e
// r.atualizar(...) é chamado uma vez por janela.
class Regra {
 public:
  // nivelDb e valido: o dbCombinado e o validoCombinado da Parte 5.
  // agora: instante da janela em ms. limiteDb e tempoXs fora da faixa do
  // config.h (30–120 dB, 1–600 s) são trocados pelos padrões (65 dB, 5 s).
  RegraSaida atualizar(float nivelDb, bool valido, uint32_t agora,
                       float limiteDb, int tempoXs);

  Estado   estado()  const { return estado_; }
  uint32_t acimaMs() const { return acimaMs_; }

 private:
  Estado   estado_      = Estado::CALMO;
  uint32_t tInicio_     = 0;      // quando o nível passou do limite
  uint32_t acimaMs_     = 0;      // tempo acima do limite (para a barra da página)
  bool     segurando_   = false;  // em ALERTA, contando o HOLD_MS?
  uint32_t tSegurando_  = 0;      // quando o HOLD_MS começou
};
