// NoiseTrack: módulo de buzzer ativo de 3 pinos, com lógica invertida configurável.
#include "buzzer.h"
#include "config.h"
#include "driver/gpio.h"

// Nível elétrico que liga ou desliga o som, conforme BUZZER_ATIVO_EM_BAIXO (config.h).
static constexpr uint8_t NIVEL_LIGADO    = BUZZER_ATIVO_EM_BAIXO ? LOW  : HIGH;
static constexpr uint8_t NIVEL_DESLIGADO = BUZZER_ATIVO_EM_BAIXO ? HIGH : LOW;

static bool     somLigado      = false;   // o pino está no nível "ligado" agora?
static bool     alertaAnterior = false;
static uint32_t tUltimaTroca   = 0;

static void escrever(bool ligado) {
  digitalWrite(PINO_BUZZER, ligado ? NIVEL_LIGADO : NIVEL_DESLIGADO);
  somLigado = ligado;
}

void buzzer_iniciar() {
  // Grava o nível "desligado" direto no registrador antes de virar saída,
  // para o pino não passar nem um instante pelo nível que liga o som.
  gpio_set_level((gpio_num_t)PINO_BUZZER, NIVEL_DESLIGADO);
  pinMode(PINO_BUZZER, OUTPUT);
  escrever(false);
}

void buzzer_atualizar(bool alerta, uint32_t agora) {
  if (!alerta) {
    if (somLigado) escrever(false);
    alertaAnterior = false;
    return;
  }

  if (!alertaAnterior) {          // o alerta acabou de começar: bipa na hora
    alertaAnterior = true;
    tUltimaTroca = agora;
    escrever(true);
    return;
  }

  if (agora - tUltimaTroca >= BUZZER_BIPE_MS) {   // alterna liga/desliga
    tUltimaTroca = agora;
    escrever(!somLigado);
  }
}
