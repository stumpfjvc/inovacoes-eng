# NoiseTrack

Medidor de ruído em decibéis (dB) com ESP32. Mede o som do ambiente em tempo real e avisa, com LEDs e buzzer, quando o volume passa do limite por um tempo contínuo.

**Status:** em desenvolvimento.

## Como funciona

1. O microfone captura o som do ambiente.
2. O ESP32 calcula o nível sonoro em dB a partir de amostras curtas.
3. Se o nível ficar acima do limite por X segundos contínuos, o alerta dispara.
4. Se o som voltar ao normal antes disso, nada acontece.

Picos rápidos, como uma porta batendo, não disparam o alerta.

## Hardware

| Componente | Função |
|---|---|
| ESP32 WROOM-32 | Processamento e cálculo do nível em dB |
| Microfone INMP441 (I2S) | Captura de áudio digital |
| LEDs | Indicação visual do nível |
| Buzzer | Alerta sonoro |
| Cabo USB | Alimentação e gravação do firmware |
| Bateria 18650 + TP4056 | Alimentação de backup (no-break), a confirmar |

## Ligações do microfone

| INMP441 | ESP32 |
|---|---|
| VDD | 3,3 V |
| GND | GND |
| L/R | GND (canal esquerdo) |
| SD, WS, SCK | a definir |

Os pinos de LEDs e buzzer também serão definidos após a montagem na protoboard.

## Alerta

- **Limite (dB):** nível acima do qual o som é considerado excessivo.
- **Tempo X (s):** quanto tempo seguido o som precisa ficar acima do limite.
- **LEDs:** mostram o estado atual do ambiente.
- **Buzzer:** toca quando o alerta dispara.

Os valores de limite e de X são parâmetros do projeto e podem ser ajustados. A forma de ajuste ainda está em definição.

## Privacidade

O aparelho não grava áudio. As amostras são usadas só para calcular o nível em dB e descartadas em seguida. Nada de som é armazenado ou enviado.

## Como usar

1. Monte o circuito conforme as ligações acima.
2. Grave o firmware no ESP32 pelo cabo USB.
3. Ligue o aparelho na tomada, pela fonte USB.
4. Ajuste o limite e o tempo X (forma de ajuste a definir).

## Licença

A definir.
