// NoiseTrack: cálculo do nível em dB a partir das janelas de amostras (Parte 5).
// Funções puras: não usam Serial, millis() nem hardware. Recebem números e
// devolvem números, por isso podem ser testadas sozinhas (ver teste_nivel_db).
#pragma once
#include <stdint.h>
#include <stddef.h>

// Resultado de uma janela. É como uma classe Java só com campos públicos.
struct NivelDb {
  float dbA;              // nível do mic A, em dB SPL (DB_PISO se inválido)
  float dbB;              // nível do mic B, em dB SPL (DB_PISO se inválido ou ausente)
  float dbCombinado;      // número único para a regra dos X segundos (Parte 6)
  bool  validoA;          // o canal A teve sinal (amostras variando)?
  bool  validoB;          // idem para B; sempre false se canalB = nullptr
  bool  validoCombinado;  // false só se nenhum canal válido
  bool  canalDescartado;  // um canal foi entregue mas ficou fora por não ter sinal
};

// Calcula tudo para uma janela. canalB = nullptr quando há só 1 microfone.
// Usa COMBINACAO_MICS e os ajustes OFFSET_CAL_MIC_A_DB / _B_DB do config.h.
NivelDb nivel_calcular(const int32_t* canalA, const int32_t* canalB, size_t n);

// --- Peças usadas por nivel_calcular, públicas para o autoteste ---

// dBFS de um canal: tira o desvio DC, calcula o RMS e compara com o fundo de escala.
// *valido = false se todas as amostras forem iguais (nesse caso devolve o piso em dBFS).
float nivel_dbfs(const int32_t* amostras, size_t n, bool* valido);

// Converte dBFS em dB SPL: soma OFFSET_BASE_DB e o ajuste, e limita a DB_PISO..DB_TETO.
float nivel_spl_de_dbfs(float dbfs, float ajusteCalDb);

// Junta dois níveis válidos: COMBINACAO_MEDIA_ENERGIA ou COMBINACAO_MAIOR.
float nivel_combinar(float dbA, float dbB, int modo);
