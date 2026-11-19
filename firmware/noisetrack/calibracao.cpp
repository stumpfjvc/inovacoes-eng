// NoiseTrack: média em energia para a calibração. Só contas, sem hardware.
#include "calibracao.h"
#include <math.h>

void calib_zerar(AcumuladorCalib* a) {
  a->somaEnergia = 0.0;
  a->janelas     = 0;
  a->minDb       = 0.0f;
  a->maxDb       = 0.0f;
}

void calib_somar(AcumuladorCalib* a, float db, bool valido) {
  if (!valido || !isfinite(db)) return;
  // double: 10^(130/10) = 10^13 somado por centenas de janelas perde precisão em float.
  a->somaEnergia += pow(10.0, (double)db / 10.0);
  if (a->janelas == 0 || db < a->minDb) a->minDb = db;
  if (a->janelas == 0 || db > a->maxDb) a->maxDb = db;
  a->janelas++;
}

bool calib_media(const AcumuladorCalib& a, float* mediaDb) {
  if (a.janelas == 0) return false;
  *mediaDb = (float)(10.0 * log10(a.somaEnergia / (double)a.janelas));
  return true;
}
