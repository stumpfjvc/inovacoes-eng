// NoiseTrack: cálculo do nível em dB (Parte 5). Sem Serial, sem hardware, sem millis().
#include "nivel_db.h"
#include "config.h"
#include <math.h>

// Piso em dBFS que corresponde a DB_PISO em dB SPL (para RMS = 0 não virar -infinito).
static constexpr float DBFS_PISO = DB_PISO - OFFSET_BASE_DB;

static float limitar(float v, float minimo, float maximo) {
  if (v < minimo) return minimo;
  if (v > maximo) return maximo;
  return v;
}

float nivel_dbfs(const int32_t* amostras, size_t n, bool* valido) {
  *valido = false;
  if (amostras == nullptr || n == 0) return DBFS_PISO;

  // 1) Desvio DC: soma todas as amostras e divide por n para achar a média.
  //    int64_t é como o long do Java: 2000 amostras de 24 bits cabem com folga.
  int64_t soma = 0;
  bool variou = false;
  for (size_t i = 0; i < n; i++) {
    soma += amostras[i];
    if (amostras[i] != amostras[0]) variou = true;
  }
  if (!variou) return DBFS_PISO;   // todas iguais: mic desligado ou silêncio absoluto

  // Média arredondada para o inteiro mais próximo. A fração que sobra mudaria o
  // RMS em menos de 0,5, quando o menor RMS real fica na casa das centenas.
  const int64_t nn = (int64_t)n;
  const int64_t media = (soma >= 0 ? soma + nn / 2 : soma - nn / 2) / nn;

  // 2) RMS: subtrai a média de cada amostra, eleva ao quadrado e soma tudo.
  //    Cada quadrado fica abaixo de 2^49 e a soma de 2000 deles abaixo de 2^60,
  //    então o int64_t não estoura (o limite dele é 2^63).
  int64_t somaQuadrados = 0;
  for (size_t i = 0; i < n; i++) {
    const int64_t d = (int64_t)amostras[i] - media;
    somaQuadrados += d * d;
  }
  //    RMS = raiz quadrada de (soma dos quadrados dividida por n).
  const double rms = sqrt((double)somaQuadrados / (double)n);
  if (!(rms > 0.0)) return DBFS_PISO;

  // 3) dBFS = 20 x log10 de (RMS dividido pelo fundo de escala).
  //    0 dBFS seria um RMS igual ao maior valor possível; tudo abaixo é negativo.
  *valido = true;
  return 20.0f * log10f((float)(rms / MIC_FUNDO_ESCALA));
}

float nivel_spl_de_dbfs(float dbfs, float ajusteCalDb) {
  // 4) dB SPL = dBFS + OFFSET_BASE_DB (120) + ajuste de calibração do microfone.
  // 5) Limita a DB_PISO..DB_TETO.
  return limitar(dbfs + OFFSET_BASE_DB + ajusteCalDb, DB_PISO, DB_TETO);
}

float nivel_combinar(float dbA, float dbB, int modo) {
  if (modo == COMBINACAO_MAIOR) return dbA > dbB ? dbA : dbB;

  // Média em energia: cada nível volta a ser energia (10 elevado a dB dividido por 10),
  // tira-se a média das duas energias e volta-se para dB (10 x log10 da média).
  // Ex.: 80 e 70 dB dão 77,4 dB, porque o som mais forte domina a energia.
  const float energiaA = powf(10.0f, dbA / 10.0f);
  const float energiaB = powf(10.0f, dbB / 10.0f);
  return 10.0f * log10f((energiaA + energiaB) / 2.0f);
}

static float nivelCanal(const int32_t* amostras, size_t n, float ajusteCalDb, bool* valido) {
  const float dbfs = nivel_dbfs(amostras, n, valido);
  return *valido ? nivel_spl_de_dbfs(dbfs, ajusteCalDb) : DB_PISO;
}

NivelDb nivel_calcular(const int32_t* canalA, const int32_t* canalB, size_t n) {
  NivelDb r = {};
  r.dbA = nivelCanal(canalA, n, OFFSET_CAL_MIC_A_DB, &r.validoA);

  if (canalB != nullptr) {
    r.dbB = nivelCanal(canalB, n, OFFSET_CAL_MIC_B_DB, &r.validoB);
  } else {
    r.dbB = DB_PISO;
    r.validoB = false;
  }

  if (r.validoA && r.validoB) {
    r.dbCombinado = nivel_combinar(r.dbA, r.dbB, COMBINACAO_MICS);
  } else if (r.validoA) {
    r.dbCombinado = r.dbA;          // só A tem sinal (ou só existe A)
  } else if (r.validoB) {
    r.dbCombinado = r.dbB;          // A sem sinal: usa só B
  } else {
    r.dbCombinado = DB_PISO;
  }
  r.validoCombinado = r.validoA || r.validoB;
  // Um canal entregue (não nullptr) que ficou fora da conta por não ter sinal.
  r.canalDescartado = (canalA != nullptr && !r.validoA) || (canalB != nullptr && !r.validoB);
  return r;
}
