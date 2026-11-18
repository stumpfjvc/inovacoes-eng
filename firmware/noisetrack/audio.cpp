// NoiseTrack: fonte de áudio. FONTE_SIMULADA (config.h) escolhe a versão:
//   0 = INMP441 real, pela API antiga driver/i2s.h (núcleo Arduino 2.x);
//   1 = amostras sintéticas, para o Wokwi (que não tem INMP441).
#include "audio.h"
#include "config.h"

#if FONTE_SIMULADA
// =====================================================================
// FONTE SIMULADA
// =====================================================================
#include <math.h>
#include <string.h>
#include <stdlib.h>

// Faixas da simulação (mesma ideia do script.ino antigo, agora em dB).
static constexpr int      SIM_FUNDO_MIN_DB   = 35;
static constexpr int      SIM_FUNDO_MAX_DB   = 45;
static constexpr int      SIM_RAJADA_MIN_DB  = 60;
static constexpr int      SIM_RAJADA_MAX_DB  = 90;
static constexpr uint32_t SIM_RAJADA_MIN_MS  = 1000;
static constexpr uint32_t SIM_RAJADA_MAX_MS  = 4000;
static constexpr uint32_t SIM_INTERVALO_MIN_MS = 5000;
static constexpr uint32_t SIM_INTERVALO_MAX_MS = 15000;
// Faixa aceita para o valor digitado no Serial.
static constexpr long     SIM_FORCADO_MIN_DB = 20;
static constexpr long     SIM_FORCADO_MAX_DB = 120;

static float    nivelDb        = 40.0f;  // nível atual do canal A (dB)
static float    diferencaB_Db  = 0.0f;   // diferença fixa do canal B, sorteada no início
static long     nivelForcadoDb = -1;     // valor digitado no Serial (-1 = automático)
static bool     emRajada       = false;
static int      alvoRajadaDb   = 0;
static uint32_t tFimRajada     = 0;
static uint32_t tProxRajada    = 0;
static uint32_t estadoSorteio  = 1;      // estado do gerador rápido de ruído

// Linha digitada no Serial, montada aos poucos (sem esperar).
static char    linha[16];
static uint8_t tamLinha   = 0;
static bool    linhaLonga = false;

// "Já passou do instante t?" Funciona mesmo quando millis() dá a volta (~49 dias).
static bool passou(uint32_t agora, uint32_t t) { return (int32_t)(agora - t) >= 0; }

// Gerador xorshift: barato o bastante para 4000 números por janela.
static uint32_t sorteio32() {
  uint32_t x = estadoSorteio;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  estadoSorteio = x;
  return x;
}

// RMS que o INMP441 entregaria para um som de "db" dB SPL (sem calibração).
static float rmsDeDb(float db) {
  const float rmsRef = MIC_FUNDO_ESCALA * powf(10.0f, MIC_SENSIBILIDADE_DBFS / 20.0f);
  return rmsRef * powf(10.0f, (db - MIC_REF_DB_SPL) / 20.0f);
}

// Interpreta a linha digitada: um número inteiro de 20 a 120, ou "auto" / "-1".
// Qualquer outra coisa é ignorada (sem virar 0, como acontecia com parseInt).
static void interpretarLinha(char* s) {
  while (*s == ' ' || *s == '\t') s++;
  char* fim = s + strlen(s);
  while (fim > s && (fim[-1] == ' ' || fim[-1] == '\t')) *--fim = '\0';
  if (*s == '\0') return;   // linha vazia

  if (strcmp(s, "auto") == 0 || strcmp(s, "-1") == 0) {
    nivelForcadoDb = -1;
    Serial.println("[sim] nivel automatico");
    return;
  }

  char* resto = nullptr;
  long v = strtol(s, &resto, 10);
  if (resto == s || *resto != '\0' || v < SIM_FORCADO_MIN_DB || v > SIM_FORCADO_MAX_DB) {
    Serial.printf("[sim] '%s' ignorado: digite %ld a %ld, ou auto\n",
                  s, SIM_FORCADO_MIN_DB, SIM_FORCADO_MAX_DB);
    return;
  }
  nivelForcadoDb = v;
  Serial.printf("[sim] nivel forcado = %ld dB\n", v);
}

// Lê só o que já chegou no Serial; nunca espera.
static void processarSerial() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (linhaLonga) {
        Serial.println("[sim] linha longa demais, ignorada");
      } else if (tamLinha > 0) {
        linha[tamLinha] = '\0';
        interpretarLinha(linha);
      }
      tamLinha   = 0;
      linhaLonga = false;
    } else if (tamLinha < sizeof(linha) - 1) {
      linha[tamLinha++] = c;
    } else {
      linhaLonga = true;
    }
  }
}

// Avança a simulação: fundo calmo com rajadas de barulho de tempos em tempos.
static void atualizarNivel(uint32_t agora) {
  if (nivelForcadoDb >= 0) {
    nivelDb = (float)nivelForcadoDb;
    return;
  }

  if (!emRajada && passou(agora, tProxRajada)) {   // começa uma rajada
    emRajada     = true;
    alvoRajadaDb = random(SIM_RAJADA_MIN_DB, SIM_RAJADA_MAX_DB + 1);
    const uint32_t duracao = random(SIM_RAJADA_MIN_MS, SIM_RAJADA_MAX_MS + 1);
    tFimRajada   = agora + duracao;
    Serial.printf("[sim] rajada de %d dB por %lu ms\n", alvoRajadaDb, (unsigned long)duracao);
  }
  if (emRajada && passou(agora, tFimRajada)) {     // termina a rajada
    emRajada    = false;
    tProxRajada = agora + random(SIM_INTERVALO_MIN_MS, SIM_INTERVALO_MAX_MS + 1);
  }

  const float alvo = emRajada ? (float)alvoRajadaDb
                              : (float)random(SIM_FUNDO_MIN_DB, SIM_FUNDO_MAX_DB + 1);
  // Suaviza (60% do valor anterior, 40% do alvo) e soma uma variação de ±1,5 dB.
  nivelDb = nivelDb * 0.6f + alvo * 0.4f + random(-15, 16) / 10.0f;
}

// Enche o canal com ruído uniforme cujo RMS corresponde a "db".
// Ruído uniforme entre -a e +a tem RMS = a / raiz(3); por isso a = RMS * raiz(3).
static void gerarCanal(int32_t* destino, float db) {
  float amplitude = rmsDeDb(db) * 1.7320508f;
  if (amplitude > MIC_FUNDO_ESCALA) amplitude = MIC_FUNDO_ESCALA;   // satura como o mic real
  for (size_t i = 0; i < AMOSTRAS_POR_JANELA; i++) {
    const float u = (sorteio32() / 4294967295.0f) * 2.0f - 1.0f;   // de -1 a +1
    destino[i] = (int32_t)(u * amplitude);
  }
}

bool audio_iniciar() {
  randomSeed(esp_random());
  estadoSorteio = esp_random() | 1;   // o xorshift não pode começar em 0
  // Canal B: 1,0 a 3,0 dB acima ou abaixo do A, como dois microfones diferentes.
  diferencaB_Db = random(10, 31) / 10.0f * (random(2) ? 1.0f : -1.0f);
  tProxRajada   = millis() + 3000;    // primeira rajada logo no começo
  Serial.printf("[audio] fonte SIMULADA, %d microfone(s)", NUM_MICROFONES);
  if (NUM_MICROFONES == 2) Serial.printf(", canal B %+.1f dB", diferencaB_Db);
  Serial.printf("\n[audio] digite um nivel (%ld a %ld dB) ou 'auto' no Serial\n",
                SIM_FORCADO_MIN_DB, SIM_FORCADO_MAX_DB);
  return true;
}

bool audio_ler_janela(int32_t* canalA, int32_t* canalB) {
  if (canalA == nullptr) return false;
  processarSerial();
  atualizarNivel(millis());
  gerarCanal(canalA, nivelDb);
#if NUM_MICROFONES == 2
  if (canalB == nullptr) return false;
  gerarCanal(canalB, nivelDb + diferencaB_Db);
#else
  (void)canalB;
#endif
  return true;
}

#else
// =====================================================================
// FONTE REAL: INMP441 por I2S (API antiga driver/i2s.h)
// =====================================================================
#include "driver/i2s.h"

static constexpr i2s_port_t PORTA_I2S = I2S_NUM_0;
// O i2s_read devolve as amostras intercaladas: com 2 microfones, cada "quadro"
// tem 2 valores (esquerdo, direito). Lemos aos pedaços para não precisar de
// um buffer do tamanho da janela inteira.
static constexpr size_t   QUADROS_POR_BLOCO = 256;
static constexpr uint32_t TIMEOUT_BLOCO_MS  = 100;   // um bloco leva 16 ms; 100 ms = algo errado
static int32_t bloco[QUADROS_POR_BLOCO * NUM_MICROFONES];
static bool    iniciado = false;

bool audio_iniciar() {
  i2s_config_t cfg = {};
  cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate          = FREQ_AMOSTRAGEM_HZ;
  cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT;
#if NUM_MICROFONES == 2
  cfg.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;   // estéreo: os dois canais
#else
  cfg.channel_format       = MIC_CANAIS_TROCADOS ? I2S_CHANNEL_FMT_ONLY_RIGHT
                                                 : I2S_CHANNEL_FMT_ONLY_LEFT;
#endif
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags     = 0;
  cfg.dma_buf_count        = 10;    // 10 x 256 quadros = 160 ms de folga
  cfg.dma_buf_len          = 256;
  cfg.use_apll             = false;

  i2s_pin_config_t pinos = {};
  pinos.mck_io_num   = I2S_PIN_NO_CHANGE;
  pinos.bck_io_num   = PINO_MIC_SCK;
  pinos.ws_io_num    = PINO_MIC_WS;
  pinos.data_out_num = I2S_PIN_NO_CHANGE;   // sem saída de áudio
  pinos.data_in_num  = PINO_MIC_SD;

  esp_err_t err = i2s_driver_install(PORTA_I2S, &cfg, 0, nullptr);
  if (err != ESP_OK) {
    Serial.printf("[audio] ERRO i2s_driver_install: %s\n", esp_err_to_name(err));
    return false;
  }
  err = i2s_set_pin(PORTA_I2S, &pinos);
  if (err != ESP_OK) {
    Serial.printf("[audio] ERRO i2s_set_pin: %s\n", esp_err_to_name(err));
    i2s_driver_uninstall(PORTA_I2S);
    return false;
  }
  i2s_zero_dma_buffer(PORTA_I2S);
  iniciado = true;
  Serial.printf("[audio] INMP441 por I2S, %d microfone(s), %lu Hz\n",
                NUM_MICROFONES, (unsigned long)FREQ_AMOSTRAGEM_HZ);
  return true;
}

// O INMP441 manda 24 bits dentro de uma palavra de 32, encostados à esquerda
// (os 8 bits de baixo vêm zerados). Deslocar 8 casas para a direita devolve o
// valor real de 24 bits. Como o tipo é int32_t (com sinal), o deslocamento
// mantém o sinal: amostras negativas continuam negativas.
static inline int32_t converter(int32_t bruto) { return bruto >> 8; }

bool audio_ler_janela(int32_t* canalA, int32_t* canalB) {
  if (!iniciado || canalA == nullptr) return false;
#if NUM_MICROFONES == 2
  if (canalB == nullptr) return false;
#else
  (void)canalB;
#endif

  size_t feitos = 0;
  while (feitos < AMOSTRAS_POR_JANELA) {
    const size_t faltam  = AMOSTRAS_POR_JANELA - feitos;
    const size_t quadros = faltam < QUADROS_POR_BLOCO ? faltam : QUADROS_POR_BLOCO;
    const size_t pedidos = quadros * NUM_MICROFONES * sizeof(int32_t);
    size_t lidos = 0;

    // Única espera permitida: aguarda o DMA ter as amostras (até TIMEOUT_BLOCO_MS).
    const esp_err_t err = i2s_read(PORTA_I2S, bloco, pedidos, &lidos,
                                   pdMS_TO_TICKS(TIMEOUT_BLOCO_MS));
    if (err != ESP_OK || lidos != pedidos) {
      Serial.printf("[audio] falha no i2s_read: %s, %u de %u bytes\n",
                    esp_err_to_name(err), (unsigned)lidos, (unsigned)pedidos);
      return false;
    }

    for (size_t i = 0; i < quadros; i++) {
#if NUM_MICROFONES == 2
      const int32_t esquerdo = converter(bloco[2 * i]);
      const int32_t direito  = converter(bloco[2 * i + 1]);
      canalA[feitos + i] = MIC_CANAIS_TROCADOS ? direito  : esquerdo;
      canalB[feitos + i] = MIC_CANAIS_TROCADOS ? esquerdo : direito;
#else
      canalA[feitos + i] = converter(bloco[i]);
#endif
    }
    feitos += quadros;
  }
  return true;
}
#endif
