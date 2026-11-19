// NoiseTrack: cálculo do nível em dB a partir das janelas de amostras (Parte 5).
// Funções puras: não usam Serial, millis() nem hardware. Recebem números e
// devolvem números, por isso podem ser testadas sozinhas (ver teste_nivel_db).
#pragma once
#include <stdint.h>
#include <stddef.h>

// Filtro passa-alta Butterworth de 2ª ordem (transformada bilinear com
// pré-distorção, forma direta I). Mesma conta do C:\dev\teste-mic, que foi
// testada na placa. Funciona como uma classe Java: "FiltroPassaAlta f;" cria,
// f.configurar(200, 16000) calcula os coeficientes e f.processar(x) filtra uma
// amostra. x1, x2, y1, y2 são as amostras anteriores (o "estado"): elas passam
// de uma janela para a próxima, por isso cada canal tem o seu filtro.
// Usa double: com o polo tão perto de 1 (200 Hz em 16 kHz), float perde precisão.
struct FiltroPassaAlta {
  double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
  double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

  void configurar(double corteHz, double taxaHz);   // também zera o estado
  double processar(double x) {
    const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x;
    y2 = y1; y1 = y;
    return y;
  }
};

// Resultado de uma janela. É como uma classe Java só com campos públicos.
struct NivelDb {
  float dbA;              // nível do mic A, em dB SPL (DB_PISO se inválido)
  float dbB;              // nível do mic B, em dB SPL (DB_PISO se inválido ou ausente)
  float dbfsA;            // dBFS do mic A usado na conta (depois do filtro, se houver)
  float dbfsB;            // idem para B
  float dbCombinado;      // número único para a regra dos X segundos (Parte 6)
  bool  validoA;          // o canal A teve sinal (amostras variando)?
  bool  validoB;          // idem para B; sempre false se canalB = nullptr
  bool  validoCombinado;  // false só se nenhum canal válido
  bool  canalDescartado;  // um canal foi entregue mas ficou fora por não ter sinal
};

// Calcula tudo para uma janela. canalB = nullptr quando há só 1 microfone.
// Usa COMBINACAO_MICS e os ajustes OFFSET_CAL_MIC_A_DB / _B_DB do config.h.
// filtroA / filtroB: o passa-alta de cada canal (o estado deles avança), ou
// nullptr para a conta antiga, só com a remoção da média.
NivelDb nivel_calcular(const int32_t* canalA, const int32_t* canalB, size_t n,
                       FiltroPassaAlta* filtroA = nullptr, FiltroPassaAlta* filtroB = nullptr);

// --- Peças usadas por nivel_calcular, públicas para o autoteste ---

// dBFS de um canal, comparando o RMS com o fundo de escala.
//   filtro = nullptr: tira o desvio DC (a média da janela) e calcula o RMS.
//   filtro != nullptr: passa cada amostra pelo filtro (o estado dele avança) e
//   calcula o RMS da saída; o passa-alta já elimina o desvio DC.
// *valido = false se todas as amostras forem iguais (nesse caso devolve o piso em dBFS).
float nivel_dbfs(const int32_t* amostras, size_t n, bool* valido,
                 FiltroPassaAlta* filtro = nullptr);

// Converte dBFS em dB SPL: soma OFFSET_BASE_DB e o ajuste, e limita a DB_PISO..DB_TETO.
float nivel_spl_de_dbfs(float dbfs, float ajusteCalDb);

// Junta dois níveis válidos: COMBINACAO_MEDIA_ENERGIA ou COMBINACAO_MAIOR.
float nivel_combinar(float dbA, float dbB, int modo);
