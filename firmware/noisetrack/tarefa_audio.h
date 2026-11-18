// NoiseTrack: tarefa FreeRTOS do alerta local (Parte 7).
// Uma "thread" própria lê o áudio, calcula o dB, aplica a regra e controla os
// LEDs e o buzzer. Não depende do loop() nem de rede.
#pragma once
#include <stdint.h>
#include "estado.h"
#include "regra.h"

// Tudo o que a tarefa publica. O loop() e, depois, a página leem uma CÓPIA.
struct EstadoCompartilhado {
  // Nível da última janela (Parte 5)
  float dbA;
  float dbB;
  float dbCombinado;
  bool  validoA;
  bool  validoB;
  bool  validoCombinado;
  bool  canalDescartado;
  // Regra (Parte 6)
  Estado   estado;
  uint32_t acimaMs;                 // tempo acima do limite, para a barra da página
  float    limiteDb;                // limite em uso (pode ser mudado com tarefa_audio_definir_config)
  int      tempoXs;                 // X em uso, em segundos
  EventoRegra ultimoEvento;         // último disparou / normalizou / perdeu_sinal
  uint32_t instanteUltimoEvento;    // millis() desse evento
  uint32_t contadorEventos;         // soma 1 a cada evento (para saber se houve um novo)
  // Saúde da tarefa
  uint32_t contadorJanelas;         // janelas processadas desde a partida
  uint32_t falhasSeguidas;          // leituras do I2S que falharam em sequência
};

// Cria a trava e a tarefa. Chamar no setup(), depois de audio_iniciar(),
// leds_iniciar() e buzzer_iniciar(). Devolve false se não conseguir criar.
bool tarefa_audio_iniciar();

// Devolve uma cópia do estado, tirada com a trava fechada.
EstadoCompartilhado tarefa_audio_copiar();

// Troca o limite e o X usados pela regra (Parte 8 e página).
// Valores fora da faixa do config.h são recusados (devolve false).
bool tarefa_audio_definir_config(float limiteDb, int tempoXs);

// Menor quantidade de pilha livre que a tarefa já teve, em bytes.
uint32_t tarefa_audio_pilha_livre_min();
