# NoiseTrack

Trabalho em grupo da UFABC. Aparelho com ESP32 que mede o ruído em dB e avisa
com LED e buzzer. O limite (dB) e o tempo X (segundos) são definidos numa página
servida pelo próprio ESP32.

## Regras
- Uma branch por parte, criada a partir da `main`. O firmware fica só em
  `firmware/noisetrack/`.
- A simulação antiga fica em `simulacao-antiga/`; não usar como base do firmware.
- Não alterar `docs/` sem o usuário pedir.
- Um commit por parte. Antes de cada commit, rodar `git status` e `git diff`.
- Sem `git push` sem o usuário pedir.
- Explicar em palavras simples: o usuário conhece Java, não C++.
- `loop()` nunca usa `delay()` nem espera rede; cada tarefa roda pelo `millis()`.
- O projeto fica em `C:\dev\inovacoes-eng`. O caminho é sem acento de propósito;
  não mover.

## Plataforma
- PlatformIO, `espressif32@6.9.0` (núcleo Arduino 2.x), placa `esp32dev`.
- `platformio.ini` tem `src_dir = .`: o código fica na própria pasta do projeto.
- Os acentos do `platformio.ini` estão certos; o PowerShell é que os exibe mal.
  Não regravar o arquivo.
- O comando `pio` não está no PATH. Usar
  `C:\.platformio\penv\Scripts\platformio.exe`.
- Compilar sem `-v`: na 6.9.0 o modo detalhado dá um erro de impressão inofensivo.
- Se voltar o erro "can't create .o", esperar o VS Code terminar de indexar e
  rodar de novo, sem apagar a `.pio`.
- Simulação no Wokwi (`wokwi.toml`); rodar o build antes.

## Decisões do grupo
- Alimentação só por USB.
- Página no ESP32, na rede local, com medidor ao vivo por WebSocket.
- Wi-Fi do local configurado por um AP de setup.
- Acesso por `noisetrack.local` mais o IP.
- Senha só para alterar a configuração.
- Módulo WS2812 com 4 LEDs.
- Módulo de buzzer ativo de 3 pinos (transistor embutido). A lógica pode ser
  invertida: constante `BUZZER_ATIVO_EM_BAIXO`.
- Pinos: SCK 26, WS 25, SD 33, LEDs 4, buzzer 27.
- Dois microfones INMP441. A constante `NUM_MICROFONES` (1 ou 2) em `config.h`
  escolhe quantos são lidos.
- Ligação dos microfones: os dois dividem SCK (GPIO 26), WS (GPIO 25) e
  SD (GPIO 33), além de VDD e GND. Mic 1: L/R no GND (canal esquerdo).
  Mic 2: L/R no 3V3 (canal direito).
- Os dois microfones são combinados por média em energia (não em dB). A
  constante `COMBINACAO_MICS` em `config.h` troca por "o maior dos dois".
- Calibração separada por microfone (`OFFSET_CAL_MIC_A_DB` e
  `OFFSET_CAL_MIC_B_DB` em `config.h`).
- O nível é dB SPL sem ponderação A. A ponderação A fica como melhoria futura.
- Sem sinal nos dois microfones é ERRO (estado próprio `SEM_SINAL`, sem alerta,
  com cor própria no LED), e não "calmo".
- Padrão: 65 dB e X = 5 s.
- Senha do AP de setup gerada a partir do MAC da placa com um segredo
  (`WIFI_AP_SEGREDO` em `secrets.h`, fora do Git; `secrets.example.h` mostra o
  formato).
- Credenciais do Wi-Fi do local em `Preferences` (namespace `wifi`), nunca no
  código nem no Git.
- Nada do aparelho sai pela rede na Parte 9, exceto o NTP.
- Nada pago.

## Plano por partes
1. Esqueleto e `config.h` (mesclada na `main`, PR #4)
2. Módulo WS2812 / LEDs (mesclada na `main`, PR #4)
3. Módulo de buzzer (mesclada na `main`, PR #4)
4. Fonte de áudio: 2 INMP441 por I2S via `driver/i2s.h` (a plataforma é
   espressif32@6.9.0 com núcleo 2.x) e fonte simulada (mesclada na `main`, PR #5)
5. Nível em dB: RMS em janelas de 125 ms e calibração (mesclada na `main`, PR #7)
6. Regra dos X segundos: calmo, contando, alerta (mesclada na `main`, PR #8)
7. Alerta local numa tarefa FreeRTOS, sem depender de Wi-Fi (mesclada na
   `main`, PR #9). A pilha da tarefa usa cerca de 1,5 KB no Wokwi (6720 de
   8192 bytes livres); confirmar na placa antes de reduzir.
8. Limite e X em `Preferences`, com padrões 65 dB e 5 s (mesclada na `main`, PR #10)
9. Wi-Fi não bloqueante, AP de setup, `noisetrack.local` e NTP (mesclada na
   `main`, PR #11). O portal do AP usa HTTP puro. Flash em 66,5% depois desta
   parte.
10. Página no ESP32 com WebSocket, polling de reserva e senha (mesclada na
    `main`, PR #12). Flash em 68,5% (pior caso 69,4% com microfone real);
    `platformio.ini` não foi alterado.
11. Calibração na placa, com `USANDO_WOKWI` e `FONTE_SIMULADA` em 0. Depende
    da placa; o guia está em `firmware/noisetrack/GUIA_BANCADA.md`.
    Ajustes de bancada (branch `feature/bancada-ajustes`):
    - Comandos do Serial também na placa real (`COMANDOS_SERIAL`, padrão 1):
      `wifi`, `wifi apagar`, `web`, `limite N`, `x N` e `etiqueta`. O nível
      forçado (`80`, `auto`) só com `FONTE_SIMULADA` 1. `limite`/`x` não
      dependem mais de `TESTE_PREFS`.
    - `config_local.h` (fora do Git; modelo em `config_local.example.h`)
      sobrescreve as chaves do `config.h`, que viraram `#ifndef`. Os
      `constexpr` leem `LOCAL_MIC_CANAIS_TROCADOS` e
      `LOCAL_OFFSET_CAL_MIC_A_DB`/`_B_DB`.
    - `CALIBRACAO` (padrão 0): troca o `[nivel]` por uma linha `[calib]` a cada
      5 s com a média em energia (`calibracao.cpp`, função pura).

## Bancada
- **Não editar o `config.h` para a bancada.** Ele fica com os valores do Wokwi
  (`USANDO_WOKWI` 1, `FONTE_SIMULADA` 1, `BUZZER_ATIVO_EM_BAIXO` 0). A placa
  usa `config_local.h`, que não vai para o Git.
- Hardware real: ESP32 NodeMCU ESP-32S de 38 pinos (botões EN = reset e
  IO0 = BOOT); 2 INMP441 (SCK, WS, L/R, SD, VDD, GND); buzzer de 3 pinos
  (GND, I/O, VCC) acionado em nível BAIXO, então `BUZZER_ATIVO_EM_BAIXO` 1 na
  bancada; módulo de 4 LEDs HW-222 sem pinos, ligado pelo lado J2 (IN, VCC,
  GND) com fios soldados; protoboard de 830 pontos e jumpers macho-macho.
- Alimentação na bancada: VDD dos microfones e VCC do buzzer em 3V3; VCC dos
  LEDs em 5V (3V3 se falhar).
