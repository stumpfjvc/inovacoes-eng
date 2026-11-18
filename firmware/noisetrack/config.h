// NoiseTrack: configurações do firmware (pinos, padrões e tempos).
// Todos os valores ajustáveis do projeto ficam neste arquivo.
#pragma once
#include <Arduino.h>

// ================== AMBIENTE ==================
#define USANDO_WOKWI    1   // 1 = simulador Wokwi | 0 = placa física
#define FONTE_SIMULADA  1   // 1 = nível gerado por software | 0 = microfone INMP441 (Parte 4)

// ================== PINOS ==================
// Ponto de partida, a confirmar na montagem (ver README).

// Microfone INMP441 (I2S)
constexpr uint8_t PINO_MIC_SCK = 26;
constexpr uint8_t PINO_MIC_WS  = 25;
constexpr uint8_t PINO_MIC_SD  = 33;

// Os dois microfones dividem SCK, WS e SD. Mic 1: L/R no GND (canal esquerdo).
// Mic 2: L/R no 3V3 (canal direito).
#define NUM_MICROFONES 2   // 1 = só o mic 1 (esquerdo) | 2 = os dois
static_assert(NUM_MICROFONES == 1 || NUM_MICROFONES == 2, "NUM_MICROFONES deve ser 1 ou 2");
// true = troca esquerdo e direito (com 1 microfone, passa a ler o canal direito).
// Usar se na bancada os canais vierem trocados.
constexpr bool MIC_CANAIS_TROCADOS = false;

// Referência do INMP441 (datasheet): um som de 94 dB SPL dá -26 dBFS.
// A fonte simulada usa isto para transformar dB em amplitude; a Parte 5
// usa o caminho inverso, ajustado pela calibração.
constexpr float MIC_REF_DB_SPL         = 94.0f;
constexpr float MIC_SENSIBILIDADE_DBFS = -26.0f;
constexpr float MIC_FUNDO_ESCALA       = 8388607.0f;   // maior valor de 24 bits (2^23 - 1)

// ================== NÍVEL EM dB (Parte 5) ==================
// dB SPL = dBFS + OFFSET_BASE_DB + ajuste do microfone.
// OFFSET_BASE_DB = 94 - (-26) = 120: o som de 94 dB SPL que dá -26 dBFS.
constexpr float OFFSET_BASE_DB = MIC_REF_DB_SPL - MIC_SENSIBILIDADE_DBFS;

// Ajuste fino de cada microfone, em dB, somado ao nível dele.
// Fica em 0 até a calibração na placa (Parte 11): com um decibelímetro ao lado,
// se o mic A mostrar 3 dB a menos que o aparelho, OFFSET_CAL_MIC_A_DB = 3.0f.
constexpr float OFFSET_CAL_MIC_A_DB = 0.0f;
constexpr float OFFSET_CAL_MIC_B_DB = 0.0f;

// Faixa do resultado. Silêncio absoluto (RMS = 0) vira DB_PISO, nunca -infinito.
constexpr float DB_PISO = 20.0f;
constexpr float DB_TETO = 130.0f;

// Como juntar os dois microfones num número só.
#define COMBINACAO_MEDIA_ENERGIA 0   // média das energias (padrão do grupo)
#define COMBINACAO_MAIOR         1   // o maior dos dois níveis
#define COMBINACAO_MICS COMBINACAO_MEDIA_ENERGIA
static_assert(COMBINACAO_MICS == 0 || COMBINACAO_MICS == 1, "COMBINACAO_MICS deve ser 0 ou 1");

// Autoteste das contas de dB, roda uma vez no setup e imprime PASS/FAIL.
#define TESTE_NIVEL_DB 1

// Módulo WS2812 (dados)
constexpr uint8_t PINO_LEDS = 4;
constexpr uint8_t NUM_LEDS    = 4;
constexpr uint8_t BRILHO_LEDS = 77;   // 0–255; 77 ≈ 30%

// Módulo de buzzer ativo (I/O)
constexpr uint8_t PINO_BUZZER = 27;
// 0 = o módulo toca com o sinal em nível ALTO | 1 = toca em nível BAIXO (lógica invertida).
// Conferir na bancada (ver README) e ajustar aqui.
#define BUZZER_ATIVO_EM_BAIXO 0
constexpr uint32_t BUZZER_BIPE_MS = 500;   // bipe intermitente: 500 ms ligado, 500 ms desligado

// ================== REGRA DE ALERTA (valores padrão) ==================
// Usados enquanto não houver configuração salva na memória (Parte 8).
constexpr int LIMITE_PADRAO_DB = 65;   // limite em dB
constexpr int TEMPO_X_PADRAO_S = 5;    // segundos contínuos acima do limite

constexpr int LIMITE_MIN_DB = 30;
constexpr int LIMITE_MAX_DB = 120;
constexpr int TEMPO_X_MIN_S = 1;
constexpr int TEMPO_X_MAX_S = 600;

// ================== TEMPOS ==================
constexpr uint32_t JANELA_MS    = 125;    // duração de cada janela de medição (Parte 5)
constexpr uint32_t BATIMENTO_MS = 1000;   // intervalo da mensagem de "vivo" no Serial

// ================== ÁUDIO ==================
constexpr uint32_t FREQ_AMOSTRAGEM_HZ = 16000;   // amostras por segundo, por canal
static_assert((FREQ_AMOSTRAGEM_HZ * JANELA_MS) % 1000 == 0,
              "JANELA_MS deve dar um número inteiro de amostras");
// Amostras de cada canal numa janela: 16000 * 125 / 1000 = 2000.
constexpr size_t AMOSTRAS_POR_JANELA = FREQ_AMOSTRAGEM_HZ * JANELA_MS / 1000;

// Teste: imprime no Serial o nível em dB de cada canal a cada janela.
// Remover na Parte 7.
#define DEBUG_AUDIO 1

// ================== DEMONSTRAÇÃO DAS SAÍDAS ==================
// Enquanto a medição não existe (Partes 4–7), alterna calmo -> contando -> alerta
// para testar LEDs e buzzer. Desligar (0) quando a regra real entrar.
#define MODO_DEMO_SAIDAS 1
constexpr uint32_t DEMO_PASSO_MS = 2000;
