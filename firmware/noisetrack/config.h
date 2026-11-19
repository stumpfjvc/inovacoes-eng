// NoiseTrack: configurações do firmware (pinos, padrões e tempos).
// Todos os valores ajustáveis do projeto ficam neste arquivo.
//
// NÃO edite este arquivo para a bancada. Copie config_local.example.h para
// config_local.h (fora do Git) e mude lá. Cada chave abaixo marcada com
// "#ifndef" só usa o valor daqui se o config_local.h não tiver definido antes.
// Sem config_local.h, vale exatamente o que está aqui (o Wokwi depende disso).
#pragma once
#include <Arduino.h>

#if __has_include("config_local.h")
#include "config_local.h"
#endif

// ================== AMBIENTE ==================
#ifndef USANDO_WOKWI
#define USANDO_WOKWI    1   // 1 = simulador Wokwi | 0 = placa física
#endif
#ifndef FONTE_SIMULADA
#define FONTE_SIMULADA  1   // 1 = nível gerado por software | 0 = microfone INMP441 (Parte 4)
#endif

// ================== PINOS ==================
// Ponto de partida, a confirmar na montagem (ver README).

// Microfone INMP441 (I2S)
constexpr uint8_t PINO_MIC_SCK = 26;
constexpr uint8_t PINO_MIC_WS  = 25;
constexpr uint8_t PINO_MIC_SD  = 33;

// Os dois microfones dividem SCK, WS e SD. Mic 1: L/R no GND (canal esquerdo).
// Mic 2: L/R no 3V3 (canal direito).
#ifndef NUM_MICROFONES
#define NUM_MICROFONES 2   // 1 = só o mic 1 (esquerdo) | 2 = os dois
#endif
static_assert(NUM_MICROFONES == 1 || NUM_MICROFONES == 2, "NUM_MICROFONES deve ser 1 ou 2");
// true = troca esquerdo e direito (com 1 microfone, passa a ler o canal direito).
// Usar se na bancada os canais vierem trocados (no config_local.h: LOCAL_MIC_CANAIS_TROCADOS).
#ifndef LOCAL_MIC_CANAIS_TROCADOS
#define LOCAL_MIC_CANAIS_TROCADOS false
#endif
constexpr bool MIC_CANAIS_TROCADOS = LOCAL_MIC_CANAIS_TROCADOS;

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
// Na bancada, preencher no config_local.h: LOCAL_OFFSET_CAL_MIC_A_DB e _B_DB.
#ifndef LOCAL_OFFSET_CAL_MIC_A_DB
#define LOCAL_OFFSET_CAL_MIC_A_DB 0.0f
#endif
#ifndef LOCAL_OFFSET_CAL_MIC_B_DB
#define LOCAL_OFFSET_CAL_MIC_B_DB 0.0f
#endif
constexpr float OFFSET_CAL_MIC_A_DB = LOCAL_OFFSET_CAL_MIC_A_DB;
constexpr float OFFSET_CAL_MIC_B_DB = LOCAL_OFFSET_CAL_MIC_B_DB;

// Faixa do resultado. Silêncio absoluto (RMS = 0) vira DB_PISO, nunca -infinito.
constexpr float DB_PISO = 20.0f;
constexpr float DB_TETO = 130.0f;

// Como juntar os dois microfones num número só.
#define COMBINACAO_MEDIA_ENERGIA 0   // média das energias (padrão do grupo)
#define COMBINACAO_MAIOR         1   // o maior dos dois níveis
#define COMBINACAO_MICS COMBINACAO_MEDIA_ENERGIA
static_assert(COMBINACAO_MICS == 0 || COMBINACAO_MICS == 1, "COMBINACAO_MICS deve ser 0 ou 1");

// Autoteste das contas de dB, roda uma vez no setup e imprime PASS/FAIL.
#ifndef TESTE_NIVEL_DB
#define TESTE_NIVEL_DB 1
#endif

// Módulo WS2812 (dados)
constexpr uint8_t PINO_LEDS = 4;
constexpr uint8_t NUM_LEDS    = 4;
constexpr uint8_t BRILHO_LEDS = 77;   // 0–255; 77 ≈ 30%

// Módulo de buzzer ativo (I/O)
constexpr uint8_t PINO_BUZZER = 27;
// 0 = o módulo toca com o sinal em nível ALTO | 1 = toca em nível BAIXO (lógica invertida).
// Fica 0 aqui porque o Wokwi depende disso. O módulo da bancada ("低电平触发",
// acionado em nível baixo) usa 1, definido no config_local.h.
#ifndef BUZZER_ATIVO_EM_BAIXO
#define BUZZER_ATIVO_EM_BAIXO 0
#endif
constexpr uint32_t BUZZER_BIPE_MS = 500;   // bipe intermitente: 500 ms ligado, 500 ms desligado

// ================== REGRA DE ALERTA (valores padrão) ==================
// Usados enquanto não houver configuração salva na memória (Parte 8).
constexpr int LIMITE_PADRAO_DB = 65;   // limite em dB
constexpr int TEMPO_X_PADRAO_S = 5;    // segundos contínuos acima do limite

constexpr int LIMITE_MIN_DB = 30;
constexpr int LIMITE_MAX_DB = 120;
constexpr int TEMPO_X_MIN_S = 1;
constexpr int TEMPO_X_MAX_S = 600;

// Histerese: depois de passar do limite, o nível só conta como "abaixo"
// quando cai a menos de (limite - HISTERESE_DB). Entre os dois, nada zera.
constexpr float HISTERESE_DB = 3.0f;
// Tempo que o nível precisa ficar abaixo de (limite - HISTERESE_DB)
// para o alerta acabar.
constexpr uint32_t HOLD_MS = 2000;

// Autoteste da regra dos X segundos, roda uma vez no setup e imprime PASS/FAIL.
#ifndef TESTE_REGRA
#define TESTE_REGRA 1
#endif

// ================== TEMPOS ==================
constexpr uint32_t JANELA_MS    = 125;    // duração de cada janela de medição (Parte 5)
constexpr uint32_t BATIMENTO_MS = 1000;   // intervalo da mensagem de "vivo" no Serial

// ================== ÁUDIO ==================
constexpr uint32_t FREQ_AMOSTRAGEM_HZ = 16000;   // amostras por segundo, por canal
static_assert((FREQ_AMOSTRAGEM_HZ * JANELA_MS) % 1000 == 0,
              "JANELA_MS deve dar um número inteiro de amostras");
// Amostras de cada canal numa janela: 16000 * 125 / 1000 = 2000.
constexpr size_t AMOSTRAS_POR_JANELA = FREQ_AMOSTRAGEM_HZ * JANELA_MS / 1000;

// ================== TAREFA DE ÁUDIO (Parte 7) ==================
// Leitura, dB, regra, LEDs e buzzer rodam numa tarefa FreeRTOS própria
// (parecida com uma Thread do Java), separada do loop().
constexpr uint32_t PILHA_TAREFA_AUDIO      = 8192;   // bytes; o mesmo que o loop() tem
constexpr UBaseType_t PRIORIDADE_TAREFA_AUDIO = 2;   // o loop() roda com prioridade 1
constexpr BaseType_t  NUCLEO_TAREFA_AUDIO     = 1;   // o Wi-Fi (Parte 9) fica no núcleo 0

// Leituras do I2S que podem falhar seguidas antes de virar SEM_SINAL.
constexpr uint32_t FALHAS_ATE_SEM_SINAL = 5;

// Autoteste da tarefa: depois de 3 s, confere se as janelas estão avançando
// (cerca de 8 por segundo) e mostra quanto sobrou da pilha.
#ifndef TESTE_TAREFA
#define TESTE_TAREFA 1
#endif

// ================== MEMÓRIA (Parte 8) ==================
// Faixa aceita para os ajustes de calibração guardados (Parte 11).
constexpr float OFFSET_CAL_MIN_DB = -20.0f;
constexpr float OFFSET_CAL_MAX_DB = 20.0f;

// Autoteste da memória no setup. Ele grava na flash a cada partida:
// desligar (0) no aparelho instalado.
#ifndef TESTE_PREFS
#define TESTE_PREFS 1
#endif

// ================== COMANDOS NO SERIAL ==================
// 1 = lê comandos no Serial ("limite N", "x N", "wifi", "wifi apagar", "web",
// "etiqueta" e, só com FONTE_SIMULADA 1, o nível forçado) | 0 = nada é lido.
#ifndef COMANDOS_SERIAL
#define COMANDOS_SERIAL 1
#endif

// ================== CALIBRAÇÃO NA BANCADA (Parte 11) ==================
// 1 = em vez de uma linha [nivel] por janela, o loop() imprime a cada
// CALIB_PERIODO_MS a média em energia de A, B e combinado, com mínimo e máximo
// (linha "[calib]"). 0 = normal. Só muda o que é impresso, não a medição.
#ifndef CALIBRACAO
#define CALIBRACAO 0
#endif
constexpr uint32_t CALIB_PERIODO_MS = 5000;

// ================== REDE (Parte 9) ==================
// Tempo máximo da primeira tentativa de conexão antes de ligar o AP de setup.
constexpr uint32_t WIFI_TIMEOUT_MS = 15000;
// Depois de cair, tenta reconectar a cada WIFI_RETRY_MS.
constexpr uint32_t WIFI_RETRY_MS = 5000;
// Com o AP de setup ligado e ninguém conectado nele, tenta a rede salva de novo
// a cada WIFI_RETRY_AP_MS (cada tentativa atrapalha o AP por uns segundos).
constexpr uint32_t WIFI_RETRY_AP_MS = 30000;

constexpr const char* WIFI_AP_NOME = "NoiseTrack-Setup";   // rede do AP de setup
constexpr const char* MDNS_NOME    = "noisetrack";         // noisetrack.local

// Horário: UTC-3, sem horário de verão.
constexpr long        FUSO_HORARIO_S = -3 * 3600;
constexpr const char* NTP_SERVIDOR_1 = "pool.ntp.org";
constexpr const char* NTP_SERVIDOR_2 = "a.st1.ntp.br";

// Autoteste da lógica de rede (máquina de estados, validação, senha do AP).
#ifndef TESTE_WIFI
#define TESTE_WIFI 1
#endif

// ================== PÁGINA (Parte 10) ==================
// Só ligam com o Wi-Fi do local CONECTADO (o portal do AP usa a porta 80 no setup).
constexpr uint16_t WEB_PORTA_HTTP = 80;   // página, /status e /config
constexpr uint16_t WEB_PORTA_WS   = 81;   // WebSocket (medidor ao vivo)
constexpr uint8_t  WEB_MAX_CLIENTES_WS = 3;

// Senha de configuração: WEB_TENTATIVAS_MAX erradas seguidas bloqueiam por WEB_BLOQUEIO_MS.
constexpr int      WEB_TENTATIVAS_MAX = 5;
constexpr uint32_t WEB_BLOQUEIO_MS    = 60000;

// Autoteste da parte pura da página (JSON, validação, bloqueio, WebSocket).
#ifndef TESTE_WEB
#define TESTE_WEB 1
#endif
