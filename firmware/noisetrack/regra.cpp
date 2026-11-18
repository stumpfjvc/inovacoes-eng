// NoiseTrack: regra dos X segundos contínuos. Sem Serial, sem hardware, sem millis().
#include "regra.h"
#include "config.h"

const char* nomeEvento(EventoRegra e) {
  switch (e) {
    case EventoRegra::NENHUM:       return "nenhum";
    case EventoRegra::DISPAROU:     return "disparou";
    case EventoRegra::NORMALIZOU:   return "normalizou";
    case EventoRegra::PERDEU_SINAL: return "perdeu_sinal";
  }
  return "?";
}

// Tempo decorrido de t0 até agora. A subtração sem sinal continua certa quando
// millis() dá a volta: 5 - 4294967291 dá 10, e não um número negativo.
static uint32_t decorrido(uint32_t agora, uint32_t t0) { return (uint32_t)(agora - t0); }

RegraSaida Regra::atualizar(float nivelDb, bool valido, uint32_t agora,
                            float limiteDb, int tempoXs) {
  // Validação: fora da faixa (ou NaN, que falha em qualquer comparação) usa o padrão.
  if (!(limiteDb >= LIMITE_MIN_DB && limiteDb <= LIMITE_MAX_DB)) limiteDb = LIMITE_PADRAO_DB;
  if (tempoXs < TEMPO_X_MIN_S || tempoXs > TEMPO_X_MAX_S) tempoXs = TEMPO_X_PADRAO_S;

  const uint32_t tempoXMs     = (uint32_t)tempoXs * 1000;
  const float    limiteAbaixo = limiteDb - HISTERESE_DB;
  const bool     acima        = valido && nivelDb > limiteDb;
  const bool     abaixo       = !valido || nivelDb < limiteAbaixo;   // sem sinal conta como abaixo

  const Estado anterior = estado_;
  RegraSaida s = {};
  s.evento = EventoRegra::NENHUM;

  switch (estado_) {
    case Estado::CALMO:
      if (!valido) {
        estado_ = Estado::SEM_SINAL;
        s.evento = EventoRegra::PERDEU_SINAL;
      } else if (acima) {                       // regra 1: passou do limite
        estado_  = Estado::CONTANDO;
        tInicio_ = agora;
        acimaMs_ = 0;
      }
      break;

    case Estado::CONTANDO:
      if (!valido) {                            // regra 5: sem sinal zera e não dispara
        estado_  = Estado::SEM_SINAL;
        acimaMs_ = 0;
        s.evento = EventoRegra::PERDEU_SINAL;
      } else if (abaixo) {                      // regra 2: caiu abaixo de limite - histerese
        estado_  = Estado::CALMO;
        acimaMs_ = 0;
      } else {                                  // entre limite - histerese e acima: continua
        acimaMs_ = decorrido(agora, tInicio_);
        if (acimaMs_ >= tempoXMs) {             // regra 3: X segundos seguidos
          estado_    = Estado::ALERTA;
          segurando_ = false;
          s.evento   = EventoRegra::DISPAROU;
        }
      }
      break;

    case Estado::ALERTA:
      acimaMs_ = decorrido(agora, tInicio_);    // continua crescendo; a página limita em X
      if (!abaixo) {                            // voltou a ficar alto: cancela o HOLD
        segurando_ = false;
      } else {
        if (!segurando_) {                      // regras 4 e 5: começa o HOLD_MS
          segurando_  = true;
          tSegurando_ = agora;
        }
        if (decorrido(agora, tSegurando_) >= HOLD_MS) {
          segurando_ = false;
          acimaMs_   = 0;
          if (valido) {
            estado_  = Estado::CALMO;
            s.evento = EventoRegra::NORMALIZOU;
          } else {
            estado_  = Estado::SEM_SINAL;
            s.evento = EventoRegra::PERDEU_SINAL;
          }
        }
      }
      break;

    case Estado::SEM_SINAL:
      acimaMs_ = 0;
      if (valido) estado_ = Estado::CALMO;      // regra 5: o sinal voltou, recomeça calmo
      break;
  }

  s.estado         = estado_;
  s.mudou          = estado_ != anterior;
  s.instanteEvento = (s.evento != EventoRegra::NENHUM) ? agora : 0;
  s.acimaMs        = acimaMs_;
  return s;
}
