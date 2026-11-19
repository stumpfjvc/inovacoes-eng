// NoiseTrack: autoteste das contas de dB. Gera sinais conhecidos e confere o resultado.
#include "teste_nivel_db.h"
#include "config.h"

#if TESTE_NIVEL_DB
#include "nivel_db.h"
#include <math.h>
#include <stdlib.h>

static constexpr float    TOLERANCIA_DB = 0.1f;
static constexpr uint32_t FREQ_TESTE_HZ = 1000;   // 1 kHz: 16 amostras por ciclo
static_assert((AMOSTRAS_POR_JANELA * FREQ_TESTE_HZ) % FREQ_AMOSTRAGEM_HZ == 0,
              "a janela de teste precisa ter um numero inteiro de ciclos");

// Valores esperados, calculados à mão:
// senoide de pico P tem RMS = P dividido pela raiz de 2, então
// dBFS = 20 x log10(1 / raiz de 2) = -3,01 (pico cheio) e
// dBFS = 20 x log10(0,5 / raiz de 2) = -9,03 (meio pico).
static constexpr float ESPERADO_CHEIA_DBFS = -3.0103f;
static constexpr float ESPERADO_MEIA_DBFS  = -9.0309f;
// 10 x log10 da média de 10^8 e 10^7 = 10 x log10(5,5 x 10^7) = 77,40.
static constexpr float ESPERADO_MEDIA_80_70 = 77.4036f;

static int falhas = 0;

static void conferir(const char* nome, float obtido, float esperado) {
  const bool ok = isfinite(obtido) && fabsf(obtido - esperado) <= TOLERANCIA_DB;
  if (!ok) falhas++;
  Serial.printf("[teste] %s: %.3f (esperado %.3f) %s\n",
                nome, obtido, esperado, ok ? "PASS" : "FAIL");
}

static void conferirBool(const char* nome, bool obtido, bool esperado) {
  const bool ok = obtido == esperado;
  if (!ok) falhas++;
  Serial.printf("[teste] %s: %s (esperado %s) %s\n", nome,
                obtido ? "sim" : "nao", esperado ? "sim" : "nao", ok ? "PASS" : "FAIL");
}

// Senoide de pico "pico", somada a um desvio DC, com número inteiro de ciclos.
static void gerarSenoide(int32_t* destino, size_t n, float pico, int32_t dc) {
  const float passo = 2.0f * (float)M_PI * FREQ_TESTE_HZ / FREQ_AMOSTRAGEM_HZ;
  for (size_t i = 0; i < n; i++) {
    destino[i] = (int32_t)lroundf(pico * sinf(passo * (float)i)) + dc;
  }
}

static void preencher(int32_t* destino, size_t n, int32_t valor) {
  for (size_t i = 0; i < n; i++) destino[i] = valor;
}

bool teste_nivel_db_rodar() {
  const size_t n = AMOSTRAS_POR_JANELA;
  falhas = 0;
  Serial.println("[teste] === autoteste do nivel em dB ===");

  // Buffers emprestados da memória só durante o teste (e devolvidos no fim).
  int32_t* a = (int32_t*)malloc(n * sizeof(int32_t));
  int32_t* b = (int32_t*)malloc(n * sizeof(int32_t));
  if (a == nullptr || b == nullptr) {
    Serial.println("[teste] FAIL: sem memoria para o teste");
    free(a);
    free(b);
    return false;
  }

  bool valido = false;

  // 1) Senoide com pico igual ao fundo de escala.
  gerarSenoide(a, n, MIC_FUNDO_ESCALA, 0);
  conferir("1 senoide cheia (dBFS)", nivel_dbfs(a, n, &valido), ESPERADO_CHEIA_DBFS);

  // 2) Senoide com metade do pico.
  gerarSenoide(a, n, MIC_FUNDO_ESCALA / 2.0f, 0);
  conferir("2 senoide meia (dBFS)", nivel_dbfs(a, n, &valido), ESPERADO_MEIA_DBFS);

  // 3) A mesma do caso 2 com +100000 de DC: se o DC sai, o resultado não muda.
  gerarSenoide(a, n, MIC_FUNDO_ESCALA / 2.0f, 100000);
  conferir("3 senoide meia + DC (dBFS)", nivel_dbfs(a, n, &valido), ESPERADO_MEIA_DBFS);

  // 4) Silêncio: todas as amostras em zero devem dar DB_PISO, sem NaN nem inf.
  preencher(a, n, 0);
  const NivelDb silencio = nivel_calcular(a, nullptr, n);
  conferir("4 silencio (dB SPL)", silencio.dbCombinado, DB_PISO);
  conferirBool("4 silencio marcado como sem sinal", silencio.validoA, false);

  // 5) Combinação de 80 dB e 70 dB nos dois modos.
  conferir("5 media em energia de 80 e 70",
           nivel_combinar(80.0f, 70.0f, COMBINACAO_MEDIA_ENERGIA), ESPERADO_MEDIA_80_70);
  conferir("5 maior de 80 e 70",
           nivel_combinar(80.0f, 70.0f, COMBINACAO_MAIOR), 80.0f);

  // 6) Canal B parado (todas as amostras iguais): o combinado é o próprio A.
  gerarSenoide(a, n, MIC_FUNDO_ESCALA / 100.0f, 0);
  preencher(b, n, 12345);
  const NivelDb umMorto = nivel_calcular(a, b, n);
  conferir("6 B parado: combinado = A", umMorto.dbCombinado, umMorto.dbA);
  conferirBool("6 B parado marcado como descartado", umMorto.canalDescartado, true);

  free(a);
  free(b);
  Serial.printf("[teste] === %s (%d falha(s)) ===\n", falhas == 0 ? "TUDO PASS" : "HOUVE FAIL", falhas);
  return falhas == 0;
}

#else
bool teste_nivel_db_rodar() { return true; }
#endif
