// NoiseTrack: módulo de buzzer ativo de 3 pinos, com lógica invertida configurável.
#include "buzzer.h"
#include "config.h"
#include "driver/gpio.h"

// Nível elétrico que liga ou desliga o som, conforme BUZZER_ATIVO_EM_BAIXO (config.h).
static constexpr uint8_t NIVEL_LIGADO    = BUZZER_ATIVO_EM_BAIXO ? LOW  : HIGH;
static constexpr uint8_t NIVEL_DESLIGADO = BUZZER_ATIVO_EM_BAIXO ? HIGH : LOW;

// O buzzer só é atualizado uma vez por janela, então um pulso de 250 ms vira
// 2 janelas. Sem folga, uma janela que chegasse 1 ms adiantada (249 ms) faria o
// pulso durar 3 janelas. Meia janela de folga arredonda para o mais próximo.
static constexpr uint32_t FOLGA_MS = JANELA_MS / 2;

static bool     somLigado      = false;   // o pino está no nível "ligado" agora?
static bool     alertaAnterior = false;
static uint32_t tUltimaTroca   = 0;
static bool     jaDesligou     = false;   // já houve algum apito desde a partida?
static uint32_t tDesligou      = 0;       // quando o último apito acabou

static void escrever(bool ligado, uint32_t agora) {
  if (somLigado && !ligado) {     // o som está acabando agora
    tDesligou  = agora;
    jaDesligou = true;
  }
  digitalWrite(PINO_BUZZER, ligado ? NIVEL_LIGADO : NIVEL_DESLIGADO);
  somLigado = ligado;
}

// "Já passou dur ms desde t0?", com a folga de meia janela.
static bool venceu(uint32_t agora, uint32_t t0, uint32_t dur) {
  return (uint32_t)(agora - t0) + FOLGA_MS >= dur;
}

void buzzer_iniciar() {
  // Grava o nível "desligado" direto no registrador antes de virar saída,
  // para o pino não passar nem um instante pelo nível que liga o som.
  gpio_set_level((gpio_num_t)PINO_BUZZER, NIVEL_DESLIGADO);
  pinMode(PINO_BUZZER, OUTPUT);
  escrever(false, 0);
}

void buzzer_atualizar(bool alerta, uint32_t agora) {
  if (!alerta) {
    if (somLigado) escrever(false, agora);
    alertaAnterior = false;
    return;
  }

  if (!alertaAnterior) {          // o alerta acabou de começar: bipa na hora
    alertaAnterior = true;
    tUltimaTroca = agora;
    escrever(true, agora);
    return;
  }

  const uint32_t duracao = somLigado ? BUZZER_PULSO_MS : BUZZER_PAUSA_MS;
  if (venceu(agora, tUltimaTroca, duracao)) {   // fim do pulso ou da pausa
    tUltimaTroca = agora;
    escrever(!somLigado, agora);
  }
}

bool buzzer_tocou_desde(uint32_t desde) {
  if (somLigado) return true;
  if (!jaDesligou) return false;
  // Comparação com sinal: continua certa quando millis() dá a volta (~49 dias).
  return (int32_t)(tDesligou - desde) > 0;
}
