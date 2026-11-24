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

// Filtro passa-alta antes do RMS: corta o ronco de baixa frequência (vento,
// vibração da mesa, rede elétrica) que o INMP441 pega e que inflava o nível.
// Butterworth de 2ª ordem, testado na placa no projeto teste-mic.
// 1 = liga | 0 = conta antiga (só tira a média da janela).
#ifndef FILTRO_PASSA_ALTA
#define FILTRO_PASSA_ALTA 1
#endif
#ifndef FILTRO_HZ
#define FILTRO_HZ 200
#endif
static_assert(FILTRO_HZ > 0, "FILTRO_HZ deve ser maior que 0");

// Aquecimento: nos primeiros AQUECIMENTO_MIC_MS depois que a tarefa de áudio
// começa, as janelas são lidas e passam pelo filtro (para ele assentar), mas
// são descartadas: a regra não roda e o nível publicado fica "sem sinal".
// 0 = sem aquecimento (como era antes).
#ifndef AQUECIMENTO_MIC_MS
#define AQUECIMENTO_MIC_MS 5000
#endif

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
// Bipe em pulsos durante o alerta: BUZZER_PULSO_MS ligado, BUZZER_PAUSA_MS desligado.
// O buzzer é atualizado uma vez por janela (JANELA_MS = 125), então os tempos
// arredondam para o múltiplo de 125 ms mais próximo (250 = 2 janelas, 750 = 6).
#ifndef BUZZER_PULSO_MS
#define BUZZER_PULSO_MS 250
#endif
#ifndef BUZZER_PAUSA_MS
#define BUZZER_PAUSA_MS 750
#endif
// Contra a realimentação (o microfone ouvindo o próprio apito): uma janela que
// pegou o buzzer ligado, ou que começou menos de BUZZER_GUARDA_MS depois de ele
// desligar, não entra na regra. A regra só usa as janelas das pausas.
#ifndef BUZZER_GUARDA_MS
#define BUZZER_GUARDA_MS 80
#endif
static_assert(BUZZER_PULSO_MS > 0 && BUZZER_PAUSA_MS > 0, "pulso e pausa do buzzer devem ser > 0");
// (A conferência da pausa contra a guarda fica depois de JANELA_MS, em TEMPOS.)

// ================== REGRA DE ALERTA (valores padrão) ==================
// Usados enquanto não houver configuração salva na memória (Parte 8).
constexpr int LIMITE_PADRAO_DB = 65;   // limite em dB
constexpr int TEMPO_X_PADRAO_S = 5;    // segundos contínuos acima do limite

// Faixas aceitas (página, Serial, memória e regra usam as mesmas).
// Valor guardado fora da faixa é trocado pelo padrão ao ligar.
constexpr int LIMITE_MIN_DB = 30;
constexpr int LIMITE_MAX_DB = 100;
constexpr int TEMPO_X_MIN_S = 1;
constexpr int TEMPO_X_MAX_S = 60;

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

// Sem isto, a pausa do buzzer poderia não ter nenhuma janela limpa e o alerta
// nunca mais seria reavaliado. Guarda + 2 janelas garante ao menos uma limpa.
static_assert(BUZZER_PAUSA_MS >= BUZZER_GUARDA_MS + 2 * JANELA_MS,
              "BUZZER_PAUSA_MS curta demais: nenhuma janela da pausa ficaria fora da guarda");

// ================== ÁUDIO ==================
constexpr uint32_t FREQ_AMOSTRAGEM_HZ = 16000;   // amostras por segundo, por canal
static_assert((FREQ_AMOSTRAGEM_HZ * JANELA_MS) % 1000 == 0,
              "JANELA_MS deve dar um número inteiro de amostras");
// Amostras de cada canal numa janela: 16000 * 125 / 1000 = 2000.
constexpr size_t AMOSTRAS_POR_JANELA = FREQ_AMOSTRAGEM_HZ * JANELA_MS / 1000;
static_assert(FILTRO_HZ < FREQ_AMOSTRAGEM_HZ / 2,
              "FILTRO_HZ deve ficar abaixo da metade da taxa de amostragem");

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

// ================== DIAGNÓSTICO DO MICROFONE (temporário) ==================
// 1 = a cada 1 s, imprime no Serial uma linha [diag] do mic A: mínimo, máximo e
// média das amostras brutas de 24 bits da última janela, o RMS em dBFS (antes de
// qualquer offset de calibração) e o canal lido (esquerdo/direito). Só imprime,
// não muda regra, LEDs, buzzer nem rede. Ligar no config_local.h: DIAG_MIC 1.
#ifndef DIAG_MIC
#define DIAG_MIC 0
#endif
constexpr uint32_t DIAG_MIC_PERIODO_MS = 1000;

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

// ================== NUVEM (Supabase) ==================
// A cada NUVEM_INTERVALO_MS, com o Wi-Fi do local conectado, o aparelho manda o
// MAIOR nível das janelas desde o último envio e o estado atual para a função
// aparelho_sync, e recebe o limite e o X. A conexão HTTPS é reaproveitada.
// A NUVEM MANDA: se o limite/X de lá for diferente do daqui, vale o de lá (e é
// gravado na memória). Roda numa tarefa própria, de baixa prioridade: o alarme
// local (microfone, regra, LEDs, buzzer) nunca espera pela nuvem.
// 1 = liga (se URL e chaves estiverem preenchidas) | 0 = desliga e nem compila.
#ifndef NUVEM_ATIVA
#define NUVEM_ATIVA 1
#endif
// Endereço e chave PÚBLICA do projeto: preencher no config_local.h.
// Enquanto ficarem com estes modelos, a nuvem fica desligada (avisa no Serial).
#ifndef NUVEM_URL
#define NUVEM_URL "COLE_AQUI_A_URL_DO_PROJETO"
#endif
#ifndef NUVEM_CHAVE_PUBLICA
#define NUVEM_CHAVE_PUBLICA "COLE_AQUI_A_CHAVE_PUBLICA"
#endif
// (A chave SECRETA do aparelho, NUVEM_CHAVE_APARELHO, fica no secrets.h.)

// 1 = TLS verificado: confere a cadeia até uma raiz do nuvem_ca.h, o nome do
//     servidor e a data de cada certificado (só depois do NTP acertar o
//     relógio). Se algo não conferir, a nuvem NÃO envia (nunca cai para o modo
//     sem verificação); o alarme local segue normal.
// 0 = HTTPS criptografado, mas SEM conferir quem é o servidor (aviso no Serial).
//
// O PADRÃO É 1. O aperto de mão verificado gasta vários segundos de CPU
// (assinaturas ECDSA em software) e chegou a reiniciar a placa pelo watchdog; foi
// corrigido com a prioridade 0 da tarefa da nuvem (abaixo) e validado na placa:
// sem reinício, 49 de 49 envios, maior tempo sem a tarefa ociosa de 8 ms. A
// primeira conexão leva cerca de 10 s (o site só mostra Online depois disso).
// Detalhes: GUIA_BANCADA.md, "Reinício pelo watchdog". Para o modo sem
// verificação (só diagnóstico), no config_local.h: #define NUVEM_TLS_VALIDAR 0
#ifndef NUVEM_TLS_VALIDAR
#define NUVEM_TLS_VALIDAR 1
#endif

// Intervalo entre envios.
//   1000  = quase em tempo real (demonstração com um aparelho só). Padrão.
//   10000 = o ritmo do histórico: o banco guarda 1 leitura a cada 10 s de
//           qualquer jeito, e o tráfego cai 10 vezes.
// Para voltar a 10 s, ponha no config_local.h:  #define NUVEM_INTERVALO_MS 10000
// (custo de tráfego e passo a passo: GUIA_BANCADA.md, etapa k).
#ifndef NUVEM_INTERVALO_MS
#define NUVEM_INTERVALO_MS 1000
#endif
// Depois de uma falha a espera dobra (1, 2, 4, 8... s) até este teto, e volta
// ao NUVEM_INTERVALO_MS no primeiro envio que der certo.
constexpr uint32_t NUVEM_ESPERA_MAX_MS = 60000;
// Timeouts curtos: o alarme local não espera por eles (a nuvem tem tarefa
// própria), mas um envio preso atrasa os seguintes (os ciclos perdidos são pulados).
constexpr uint32_t NUVEM_TIMEOUT_MS          = 4000;   // abrir a conexão TCP (e, sem verificação, o aperto de mão TLS)
constexpr uint32_t NUVEM_TIMEOUT_RESPOSTA_MS = 2000;   // esperar a resposta numa conexão já aberta
// Aperto de mão TLS (vale com e sem verificação): o que pesa é CPU, não rede, e a
// tarefa da nuvem divide o núcleo com a tarefa ociosa (prioridade 0, ver abaixo),
// então o tempo de relógio chega a ser o DOBRO do tempo de CPU. O limite da
// biblioteca só é conferido entre duas etapas do aperto de mão: se uma etapa lenta
// passar dele, a conexão é abandonada mesmo estando prestes a dar certo. 20 s dá
// folga. Só muda quanto a nuvem espera um servidor que travou no meio do aperto de
// mão (para o alarme não muda nada: a nuvem tem tarefa própria). Servidor que nem
// atende a conexão TCP continua sendo abandonado em NUVEM_TIMEOUT_MS.
constexpr uint32_t NUVEM_TIMEOUT_TLS_MS = 20000;
static_assert(NUVEM_TIMEOUT_MS <= 5000 && NUVEM_TIMEOUT_RESPOSTA_MS <= 5000,
              "timeouts da nuvem devem ser curtos (<= 5 s)");
static_assert(NUVEM_TIMEOUT_TLS_MS >= NUVEM_TIMEOUT_MS && NUVEM_TIMEOUT_TLS_MS <= 30000,
              "NUVEM_TIMEOUT_TLS_MS fora do razoavel");
static_assert(NUVEM_INTERVALO_MS >= 250 && NUVEM_INTERVALO_MS <= NUVEM_ESPERA_MAX_MS,
              "NUVEM_INTERVALO_MS deve ficar entre 250 ms e NUVEM_ESPERA_MAX_MS (60 s)");

// Tarefa da nuvem: núcleo 0, junto do Wi-Fi; a tarefa de áudio fica sozinha no
// núcleo 1. A pilha é grande porque o HTTPS (mbedTLS) gasta bastante.
//
// PRIORIDADE 0 (a mesma da tarefa ociosa) É DE PROPÓSITO. O watchdog de tarefas
// (5 s, reinicia a placa) vigia a tarefa ociosa do núcleo 0. O aperto de mão TLS
// verificado faz contas de vários segundos numa chamada só, sem ceder a CPU. Com
// prioridade 1, a tarefa da nuvem monopolizava o núcleo 0, a ociosa não rodava
// por mais de 5 s e a placa reiniciava ("Task watchdog got triggered ... IDLE0").
// Com prioridade 0, o escalonador reveza a nuvem e a ociosa a cada 1 ms
// (fatiamento de tempo): a ociosa sempre roda e o watchdog fica inteiro, ligado,
// vigiando o núcleo 0 como antes. O Wi-Fi não sente: as tarefas dele têm
// prioridade bem maior e interrompem a nuvem na hora. O áudio, a regra, o LED e
// o buzzer estão no núcleo 1 e não dividem núcleo com a nuvem.
constexpr uint32_t    PILHA_TAREFA_NUVEM      = 12288;
constexpr UBaseType_t PRIORIDADE_TAREFA_NUVEM = 0;
constexpr BaseType_t  NUCLEO_TAREFA_NUVEM     = 0;
static_assert(PRIORIDADE_TAREFA_NUVEM < PRIORIDADE_TAREFA_AUDIO,
              "a tarefa da nuvem tem que ter prioridade menor que a do audio");
static_assert(NUCLEO_TAREFA_NUVEM != 0 || PRIORIDADE_TAREFA_NUVEM == 0,
              "no nucleo 0 a tarefa da nuvem DEVE ter prioridade 0: com prioridade maior, "
              "o aperto de mao TLS deixa a tarefa ociosa sem rodar e o watchdog reinicia a placa");

// Autoteste da parte pura da nuvem (JSON, resposta, estados, espera crescente).
#ifndef TESTE_NUVEM
#define TESTE_NUVEM 1
#endif
