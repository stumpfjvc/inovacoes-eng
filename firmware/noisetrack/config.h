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

// Módulo WS2812 (dados)
constexpr uint8_t PINO_LEDS = 4;

// Módulo de buzzer ativo (I/O)
constexpr uint8_t PINO_BUZZER = 27;

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
