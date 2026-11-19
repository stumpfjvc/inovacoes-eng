// NoiseTrack: limite, X e calibração na flash, com a biblioteca Preferences.
// A Preferences guarda pares chave -> valor dentro de um "namespace", parecido
// com um Map<String, Integer> do Java que sobrevive ao desligar.
#include "armazenamento.h"
#include "config.h"
#include "tarefa_audio.h"
#include "rede_logica.h"
#include <Preferences.h>
#include <string.h>

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

// ---------- Wi-Fi do local (namespace "wifi") ----------
static constexpr const char* NAMESPACE_WIFI = "wifi";
static constexpr const char* CHAVE_SSID     = "ssid";
static constexpr const char* CHAVE_SENHA    = "senha";

static Preferences prefsWifi;   // segundo namespace, aberto só quando for usado
static bool        wifiAberto = false;

static bool abrirWifi() {
  if (!wifiAberto) wifiAberto = prefsWifi.begin(NAMESPACE_WIFI, false);
  return wifiAberto;
}

// Lê um texto guardado; devolve "" se faltar.
static void lerTexto(const char* chave, char* destino, size_t tam) {
  destino[0] = '\0';
  if (prefsWifi.isKey(chave)) prefsWifi.getString(chave, destino, tam);
}

// Grava um texto só se ele for diferente do que já está na flash.
static bool gravarTextoSeMudou(const char* chave, const char* v, bool* erro) {
  char atual[WIFI_TAM_SENHA];
  lerTexto(chave, atual, sizeof(atual));
  if (prefsWifi.isKey(chave) && strcmp(atual, v) == 0) return false;
  if (prefsWifi.putString(chave, v) == 0 && v[0] != '\0') {   // texto vazio grava 0 bytes
    *erro = true;
    return false;
  }
  return true;
}

bool armazenamento_wifi_ler(char* ssid, size_t tamSsid, char* senha, size_t tamSenha) {
  ssid[0] = '\0';
  senha[0] = '\0';
  if (!abrirWifi()) return false;
  lerTexto(CHAVE_SSID, ssid, tamSsid);
  lerTexto(CHAVE_SENHA, senha, tamSenha);
  if (!rede_ssid_valido(ssid) || !rede_senha_valida(senha)) {   // vazio ou estragado
    ssid[0] = '\0';
    senha[0] = '\0';
    return false;
  }
  return true;
}

ResultadoSalvar armazenamento_wifi_salvar(const char* ssid, const char* senha) {
  ResultadoSalvar r = {};
  if (!rede_ssid_valido(ssid) || !rede_senha_valida(senha)) return r;   // recusado
  r.aceito = true;
  if (!abrirWifi()) {
    r.erroFlash = true;
    return r;
  }
  const bool gravouSsid  = gravarTextoSeMudou(CHAVE_SSID, ssid, &r.erroFlash);
  const bool gravouSenha = gravarTextoSeMudou(CHAVE_SENHA, senha, &r.erroFlash);
  r.gravou = gravouSsid || gravouSenha;
  return r;
}

bool armazenamento_wifi_apagar() {
  if (!abrirWifi()) return false;
  if (prefsWifi.isKey(CHAVE_SSID))  prefsWifi.remove(CHAVE_SSID);
  if (prefsWifi.isKey(CHAVE_SENHA)) prefsWifi.remove(CHAVE_SENHA);
  return true;
}
