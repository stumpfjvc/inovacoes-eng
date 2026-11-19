// NoiseTrack: média do nível para a calibração na bancada (Parte 11).
// Funções puras: não usam Serial, millis() nem hardware. O loop() soma cada
// janela nova e, a cada CALIB_PERIODO_MS, pede a média e imprime (CALIBRACAO 1).
//
// A média é em ENERGIA, nunca média de dB: cada janela vira 10^(dB/10), as
// energias são somadas, e no fim volta para dB com 10 x log10(média).
// Ex.: 80 e 70 dB dão 77,4 dB (e não 75), porque 80 dB tem 10 vezes a energia de 70.
#pragma once
#include <stdint.h>

// Soma de um período. É como uma classe Java só com campos públicos.
struct AcumuladorCalib {
  double   somaEnergia;   // soma de 10^(dB/10) das janelas válidas
  uint32_t janelas;       // quantas janelas válidas entraram
  float    minDb;         // menor nível de janela válida no período
  float    maxDb;         // maior nível de janela válida no período
};

// Começa um período novo (sem nenhuma janela).
void calib_zerar(AcumuladorCalib* a);

// Soma uma janela. Janela inválida (valido = false) é ignorada.
void calib_somar(AcumuladorCalib* a, float db, bool valido);

// Média em energia do período, em dB, em *mediaDb.
// Devolve false ("sem sinal") se nenhuma janela válida entrou.
bool calib_media(const AcumuladorCalib& a, float* mediaDb);
