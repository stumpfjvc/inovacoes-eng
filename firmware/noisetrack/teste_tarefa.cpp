// NoiseTrack: autoteste da tarefa de áudio. Confere se ela está viva e no ritmo.
#include "teste_tarefa.h"
#include "config.h"

#if TESTE_TAREFA
#include "tarefa_audio.h"

static constexpr uint32_t DURACAO_TESTE_MS = 3000;
// Em 3 s cabem 3000 / 125 = 24 janelas. Aceita de 20 a 28 (cerca de ±15%).
static constexpr uint32_t JANELAS_ESPERADAS = DURACAO_TESTE_MS / JANELA_MS;
static constexpr uint32_t JANELAS_MIN       = JANELAS_ESPERADAS * 85 / 100;
static constexpr uint32_t JANELAS_MAX       = JANELAS_ESPERADAS * 115 / 100 + 1;

static bool     ativo          = false;
static uint32_t tInicio        = 0;
static uint32_t janelasNoInicio = 0;

void teste_tarefa_iniciar(uint32_t agora) {
  tInicio         = agora;
  janelasNoInicio = tarefa_audio_copiar().contadorJanelas;
  ativo           = true;
}

void teste_tarefa_verificar(uint32_t agora) {
  if (!ativo || (uint32_t)(agora - tInicio) < DURACAO_TESTE_MS) return;
  ativo = false;   // roda uma vez só

  const EstadoCompartilhado e = tarefa_audio_copiar();
  const uint32_t janelas = e.contadorJanelas - janelasNoInicio;
  const bool ok = janelas >= JANELAS_MIN && janelas <= JANELAS_MAX;
  Serial.printf("[teste-tarefa] %lu janelas em %lu ms (esperado %lu a %lu): %s\n",
                (unsigned long)janelas, (unsigned long)DURACAO_TESTE_MS,
                (unsigned long)JANELAS_MIN, (unsigned long)JANELAS_MAX, ok ? "PASS" : "FAIL");
  Serial.printf("[teste-tarefa] pilha livre minima: %lu de %lu bytes\n",
                (unsigned long)tarefa_audio_pilha_livre_min(),
                (unsigned long)PILHA_TAREFA_AUDIO);
}

#else
void teste_tarefa_iniciar(uint32_t) {}
void teste_tarefa_verificar(uint32_t) {}
#endif
