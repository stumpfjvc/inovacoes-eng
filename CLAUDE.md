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
- Padrão: 65 dB e X = 5 s.
- Nada pago.

## Plano por partes
1. Esqueleto e `config.h` (mesclada na `main`, PR #4)
2. Módulo WS2812 / LEDs (mesclada na `main`, PR #4)
3. Módulo de buzzer (mesclada na `main`, PR #4)
4. Fonte de áudio: 2 INMP441 por I2S via `driver/i2s.h` (a plataforma é
   espressif32@6.9.0 com núcleo 2.x) e fonte simulada
5. Nível em dB: RMS em janelas de 125 ms e calibração
6. Regra dos X segundos: calmo, contando, alerta
7. Alerta local numa tarefa FreeRTOS, sem depender de Wi-Fi, com
   `MODO_DEMO_SAIDAS = 0`
8. Limite e X em `Preferences`, com padrões 65 dB e 5 s
9. Wi-Fi não bloqueante, AP de setup, `noisetrack.local` e NTP
10. Página no ESP32 com WebSocket, polling de reserva e senha
11. Calibração na placa, com `USANDO_WOKWI` e `FONTE_SIMULADA` em 0
