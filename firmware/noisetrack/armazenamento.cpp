// NoiseTrack: limite, X e calibração na flash, com a biblioteca Preferences.
// A Preferences guarda pares chave -> valor dentro de um "namespace", parecido
// com um Map<String, Integer> do Java que sobrevive ao desligar.
#include "armazenamento.h"
#include "config.h"
#include "tarefa_audio.h"
#include <Preferences.h>

static constexpr const char* NAMESPACE      = "noisetrack";
static constexpr const char* CHAVE_LIMITE   = "limite_db";
static constexpr const char* CHAVE_TEMPO_X  = "tempo_x_s";
static constexpr const char* CHAVE_OFFSET[] = {"offset_cal_a", "offset_cal_b"};

static Preferences prefs;
static bool        aberto = false;

static bool limiteValido(int v) { return v >= LIMITE_MIN_DB && v <= LIMITE_MAX_DB; }
static bool tempoXValido(int v) { return v >= TEMPO_X_MIN_S && v <= TEMPO_X_MAX_S; }
static bool offsetValido(float v) { return v >= OFFSET_CAL_MIN_DB && v <= OFFSET_CAL_MAX_DB; }
static bool micValido(int mic) { return mic == 0 || mic == 1; }

// Lê um inteiro guardado; se faltar ou estiver fora da faixa, devolve o padrão.
static int lerInt(const char* chave, int padrao, bool (*valido)(int)) {
  if (!aberto || !prefs.isKey(chave)) return padrao;
  const int v = prefs.getInt(chave, padrao);
  return valido(v) ? v : padrao;
}

// Grava um inteiro só se ele for diferente do que já está na flash.
// Devolve true se escreveu; *erro = true se tentou e falhou.
static bool gravarIntSeMudou(const char* chave, int v, bool* erro) {
  if (prefs.isKey(chave) && prefs.getInt(chave, v + 1) == v) return false;
  if (prefs.putInt(chave, v) == 0) {   // putInt devolve quantos bytes gravou
    *erro = true;
    return false;
  }
  return true;
}

bool armazenamento_iniciar() {
  aberto = prefs.begin(NAMESPACE, false);   // false = leitura e escrita
  const int limite = armazenamento_limite_db();
  const int tempoX = armazenamento_tempo_x_s();
  const bool temLimite = aberto && prefs.isKey(CHAVE_LIMITE);
  const bool temX      = aberto && prefs.isKey(CHAVE_TEMPO_X);
  tarefa_audio_definir_config((float)limite, tempoX);
  Serial.printf("[prefs] limite = %d dB%s | X = %d s%s\n",
                limite, temLimite ? "" : " (padrao)", tempoX, temX ? "" : " (padrao)");
  if (!aberto) Serial.println("[prefs] ERRO: memoria nao abriu; usando os padroes");
  return aberto;
}

int armazenamento_limite_db() { return lerInt(CHAVE_LIMITE, LIMITE_PADRAO_DB, limiteValido); }
int armazenamento_tempo_x_s() { return lerInt(CHAVE_TEMPO_X, TEMPO_X_PADRAO_S, tempoXValido); }

ResultadoSalvar armazenamento_salvar(int limiteDb, int tempoXs) {
  ResultadoSalvar r = {};
  if (!limiteValido(limiteDb) || !tempoXValido(tempoXs)) return r;   // recusado
  r.aceito = true;

  // A tarefa passa a usar os valores novos já na próxima janela.
  tarefa_audio_definir_config((float)limiteDb, tempoXs);

  if (!aberto) {
    r.erroFlash = true;
    return r;
  }
  const bool gravouLimite = gravarIntSeMudou(CHAVE_LIMITE, limiteDb, &r.erroFlash);
  const bool gravouX      = gravarIntSeMudou(CHAVE_TEMPO_X, tempoXs, &r.erroFlash);
  r.gravou = gravouLimite || gravouX;
  return r;
}

float armazenamento_offset_cal(int mic) {
  if (!aberto || !micValido(mic) || !prefs.isKey(CHAVE_OFFSET[mic])) return 0.0f;
  const float v = prefs.getFloat(CHAVE_OFFSET[mic], 0.0f);
  return offsetValido(v) ? v : 0.0f;
}

ResultadoSalvar armazenamento_salvar_offset_cal(int mic, float db) {
  ResultadoSalvar r = {};
  if (!micValido(mic) || !offsetValido(db)) return r;   // recusado (NaN também cai aqui)
  r.aceito = true;
  if (!aberto) {
    r.erroFlash = true;
    return r;
  }
  const char* chave = CHAVE_OFFSET[mic];
  if (prefs.isKey(chave) && prefs.getFloat(chave, db + 1.0f) == db) return r;   // igual: não grava
  if (prefs.putFloat(chave, db) == 0) {
    r.erroFlash = true;
    return r;
  }
  r.gravou = true;
  return r;
}
