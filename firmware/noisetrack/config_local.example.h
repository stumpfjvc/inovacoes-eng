// NoiseTrack: modelo da configuração local (bancada / placa de verdade).
//
// Copie este arquivo para "config_local.h", na mesma pasta, e mude os valores.
// O config_local.h está no .gitignore e NÃO vai para o Git: cada pessoa ajusta
// a sua placa sem mexer no config.h (que continua com os valores do Wokwi).
//
// Como funciona: o config.h inclui o config_local.h logo no começo. Tudo o que
// estiver definido aqui vale; o que não estiver usa o padrão do config.h.
// Para voltar ao padrão de uma chave, apague a linha dela (ou o arquivo todo).
//
// Depois de criar, mudar ou apagar este arquivo: compilar e gravar de novo.
// Se o Serial não mostrar a mudança, faça "Clean" no PlatformIO e compile.
//
// Os valores abaixo são o exemplo da BANCADA, na primeira montagem
// (GUIA_BANCADA.md, etapas a, b e c: placa real com som simulado).
#pragma once

// ---------- Ambiente ----------
// 0 = placa de verdade. (1 só no Wokwi.)
#define USANDO_WOKWI   0

// Etapas (a), (b), (c): 1 = som simulado, para testar LEDs e buzzer com "80"/"auto".
// Etapa (d) em diante: troque para 0 (microfone INMP441 de verdade).
#define FONTE_SIMULADA 1

// ---------- Microfones ----------
// Etapa (d): 1 (só o mic 1 ligado). Etapa (e) em diante: troque para 2.
#define NUM_MICROFONES 1

// Troque para true só se, na etapa (e), bater no mic 1 fizer subir o B.
#define LOCAL_MIC_CANAIS_TROCADOS false

// Calibração (seção 5 do guia): média das diferenças "medidor - aparelho".
// Ex.: o aparelho mede 3,5 dB abaixo do decibelímetro no mic A -> 3.5f.
#define LOCAL_OFFSET_CAL_MIC_A_DB 0.0f
#define LOCAL_OFFSET_CAL_MIC_B_DB 0.0f

// ---------- Buzzer ----------
// O módulo da bancada é marcado "低电平触发" (acionado em nível BAIXO): 1.
// Se mesmo assim apitar sempre e calar no alerta, troque para 0.
#define BUZZER_ATIVO_EM_BAIXO 1

// ---------- Comandos no Serial ----------
// 1 = "limite N", "x N", "wifi", "wifi apagar", "web", "etiqueta"
// (e "80"/"auto" se FONTE_SIMULADA 1). 0 = o Serial não lê nada.
#define COMANDOS_SERIAL 1

// ---------- Calibração ----------
// Seção 5 do guia: troque para 1 (com FONTE_SIMULADA 0). Em vez de 8 linhas
// [nivel] por segundo, sai uma linha [calib] a cada 5 s com a média de A, B e comb.
#define CALIBRACAO 0

// ---------- Autotestes (rodam uma vez na partida) ----------
// Na bancada, deixe todos em 1. Antes de instalar: TESTE_PREFS 0 (ele grava
// na flash a cada partida); os outros podem ir a 0 para a partida ser mais rápida.
#define TESTE_NIVEL_DB 1
#define TESTE_REGRA    1
#define TESTE_TAREFA   1
#define TESTE_PREFS    1
#define TESTE_WIFI     1
#define TESTE_WEB      1
