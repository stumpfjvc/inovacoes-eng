// NoiseTrack: autoteste da memória. Grava valores de teste e devolve os originais.
#include "teste_prefs.h"
#include "config.h"

#if TESTE_PREFS
#include "armazenamento.h"
#include "tarefa_audio.h"

static int falhas = 0;

static void conferir(const char* nome, bool ok) {
  if (!ok) falhas++;
  Serial.printf("[teste-prefs] %s: %s\n", nome, ok ? "PASS" : "FAIL");
}

bool teste_prefs_rodar() {
  falhas = 0;
  Serial.println("[teste-prefs] === autoteste da memoria ===");

  // Guarda os valores atuais para devolver no fim.
  const int   limiteOriginal  = armazenamento_limite_db();
  const int   tempoXOriginal  = armazenamento_tempo_x_s();
  const float offsetAOriginal = armazenamento_offset_cal(0);

  // Valores de teste diferentes dos originais, para forçar uma gravação.
  const int limiteTeste = (limiteOriginal == 70) ? 71 : 70;
  const int tempoXTeste = (tempoXOriginal == 8) ? 9 : 8;

  // 1) Salvar e ler de volta da flash (e a tarefa recebeu os valores).
  const ResultadoSalvar r1 = armazenamento_salvar(limiteTeste, tempoXTeste);
  conferir("1 salvar valores novos (aceito e gravado)", r1.aceito && r1.gravou && !r1.erroFlash);
  conferir("1 ler de volta da flash",
           armazenamento_limite_db() == limiteTeste && armazenamento_tempo_x_s() == tempoXTeste);
  const EstadoCompartilhado e = tarefa_audio_copiar();
  conferir("1 tarefa recebeu os valores",
           (int)e.limiteDb == limiteTeste && e.tempoXs == tempoXTeste);

  // 2) Fora da faixa: recusado, e a flash continua com o valor anterior.
  const ResultadoSalvar r2a = armazenamento_salvar(LIMITE_MAX_DB + 1, tempoXTeste);
  const ResultadoSalvar r2b = armazenamento_salvar(limiteTeste, TEMPO_X_MIN_S - 1);
  conferir("2 fora da faixa recusado", !r2a.aceito && !r2a.gravou && !r2b.aceito && !r2b.gravou);
  conferir("2 flash nao mudou",
           armazenamento_limite_db() == limiteTeste && armazenamento_tempo_x_s() == tempoXTeste);

  // 3) Mesmo valor de novo: aceito, mas não regrava.
  const ResultadoSalvar r3 = armazenamento_salvar(limiteTeste, tempoXTeste);
  conferir("3 mesmo valor nao regrava", r3.aceito && !r3.gravou);

  // 4) Calibração reservada (Parte 11): grava, lê de volta, recusa fora da faixa.
  const float offsetTeste = (offsetAOriginal == 1.5f) ? 2.5f : 1.5f;
  const ResultadoSalvar r4 = armazenamento_salvar_offset_cal(0, offsetTeste);
  const ResultadoSalvar r4b = armazenamento_salvar_offset_cal(0, OFFSET_CAL_MAX_DB + 1.0f);
  conferir("4 offset de calibracao grava e le",
           r4.aceito && r4.gravou && armazenamento_offset_cal(0) == offsetTeste);
  conferir("4 offset fora da faixa recusado", !r4b.aceito);

  // Restaura os originais.
  armazenamento_salvar(limiteOriginal, tempoXOriginal);
  armazenamento_salvar_offset_cal(0, offsetAOriginal);
  conferir("restaurado",
           armazenamento_limite_db() == limiteOriginal &&
           armazenamento_tempo_x_s() == tempoXOriginal &&
           armazenamento_offset_cal(0) == offsetAOriginal);

  Serial.printf("[teste-prefs] === %s (%d falha(s)) ===\n",
                falhas == 0 ? "TUDO PASS" : "HOUVE FAIL", falhas);
  return falhas == 0;
}

#else
bool teste_prefs_rodar() { return true; }
#endif
