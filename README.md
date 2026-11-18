# NoiseTrack

Aparelho com ESP32 que mede o nível de ruído (em dB) de um local, como o salão de festas de um condomínio. Se o som passar do limite por um tempo contínuo, ele acende o módulo de LEDs e toca um buzzer no próprio local e avisa uma página web servida pelo próprio aparelho, que mostra o aviso ao porteiro. O limite e o tempo são definidos pelo porteiro nessa página.

**Status:** em desenvolvimento.

## Como funciona

1. O aparelho é instalado no local a monitorar e ligado por USB.
2. O porteiro abre a página de configuração do aparelho, pelo navegador, na rede local.
3. Na página, define o **limite (dB)** e o **tempo X (segundos)**.
4. O aparelho guarda essa configuração na própria memória.
5. O microfone capta o som e o ESP32 calcula o nível em dB a partir de amostras curtas.
6. Se o nível ficar acima do limite por X segundos seguidos, o alerta dispara: módulo de LEDs e buzzer no local, e aviso ao porteiro na página.
7. Quando o som volta ao normal, o aparelho registra o fim do alerta.

Se o som voltar ao normal antes de completar X segundos, nada acontece. Picos rápidos, como uma porta batendo, não disparam o alerta.

## Hardware

| Componente | Função |
|---|---|
| ESP32 WROOM-32 | Processamento, cálculo em dB, Wi-Fi e página de configuração |
| Microfone INMP441 (I2S) | Captura de áudio digital |
| Módulo de LED RGB WS2812 (4 LEDs) | Indicação visual do estado, por cor |
| Módulo de buzzer ativo (3 pinos, transistor embutido) | Alerta sonoro no local |
| Resistor de 330 Ω | Nos dados do WS2812: recomendado, opcional no protótipo |
| Cabo USB / fonte USB | Alimentação (5 V) e gravação do firmware |

A alimentação é somente por USB.

## Ligações

Pinos escolhidos como **ponto de partida, a confirmar na montagem**.

| Peça | Sinal | ESP32 |
|---|---|---|
| INMP441 | VDD | 3,3 V |
| INMP441 | GND | GND |
| INMP441 | L/R | GND (canal esquerdo) |
| INMP441 | SCK | GPIO 26 |
| INMP441 | WS | GPIO 25 |
| INMP441 | SD | GPIO 33 |
| WS2812 | DIN (dados) | GPIO 4, por resistor de 330 Ω (recomendado, opcional no protótipo) |
| WS2812 | VCC | 5 V (VIN) |
| WS2812 | GND | GND comum |
| Módulo de buzzer | VCC | 3V3 (recomendado) |
| Módulo de buzzer | I/O | GPIO 27 |
| Módulo de buzzer | GND | GND |

Diagrama (simplificado):

```
                    ESP32 (WROOM-32)
               +----------------------+
 INMP441       |                      |
  VDD  --------| 3V3                  |
  GND  --------| GND                  |
  L/R  --------| GND                  |
  SCK  --------| GPIO 26              |
  WS   --------| GPIO 25              |
  SD   --------| GPIO 33              |
               |                      |
 WS2812        |                      |
  DIN  --[330R]| GPIO 4               |
  VCC  --------| VIN (5 V)            |
  GND  --------| GND (comum)          |
               |                      |
 Módulo de     |                      |
 buzzer        |                      |
  VCC  --------| 3V3                  |
  I/O  --------| GPIO 27              |
  GND  --------| GND                  |
               +----------------------+
```

O resistor de 330 Ω em `[330R]` é recomendado, mas opcional no protótipo.

### Ponto a testar na bancada

- O módulo WS2812 é de 5 V e o ESP32 envia 3,3 V no sinal de dados. Se o módulo falhar ou piscar de forma errada, usar um conversor de nível ou alimentar o módulo com 3,3 V.
- O módulo de buzzer pode tocar com o sinal em nível baixo (lógica invertida). Se alimentado com 5 V, pode ficar apitando o tempo todo, porque o sinal de 3,3 V não desliga o transistor. Por isso o VCC fica no 3V3; se necessário, inverter o sinal no firmware.

## Configuração pela página do aparelho

A página de configuração é **servida pelo próprio ESP32** e acessada pelo navegador, pela rede local. Nela, o porteiro:

- acessa o aparelho que quer configurar;
- define o **limite em dB**;
- define o **tempo X em segundos**.

O aparelho guarda os valores na memória, para continuar com a última configuração depois de reiniciar ou de ficar sem rede. Se nunca recebeu configuração, usa valores padrão.

O aviso ao porteiro aparece **com a página aberta**, no navegador de um dispositivo conectado na **mesma rede Wi-Fi do aparelho**.

## Alerta local

O alerta no local **não depende de Wi-Fi**. A medição, a contagem dos X segundos, o módulo de LEDs e o buzzer rodam inteiramente no ESP32, com a última configuração salva.

A cor do módulo WS2812 mostra o estado do ambiente:

- **verde:** calmo;
- **amarelo:** atenção;
- **vermelho:** alerta.

O brilho será limitado (cerca de 30%) para reduzir o consumo e o ofuscamento. O buzzer toca quando o alerta dispara.

Se não houver conexão, o alerta continua valendo no local, mas o aviso na página pode não chegar.

## Privacidade

O aparelho **não grava áudio**. As amostras são usadas só para calcular o nível em dB e descartadas em seguida.

Pela rede local trafegam apenas:

- o identificador do aparelho;
- o nível em dB;
- os eventos de alerta (disparou e normalizou, com horário);
- a configuração (limite e tempo X).

Nenhum áudio é gravado, armazenado ou enviado.

## Estado do projeto

| Parte | Estado |
|---|---|
| Compra dos componentes | Pronto |
| Documento de ideia e quadro Kanban (`docs/`) | Pronto |
| Montagem do circuito | Em desenvolvimento |
| Firmware: leitura dos microfones (2 INMP441 por I2S) | Pronto |
| Firmware: cálculo do nível em dB | Em desenvolvimento |
| Firmware: regra do tempo X contínuo | Pendente |
| Firmware: controle do módulo WS2812 (biblioteca NeoPixel ou FastLED) | Pendente |
| Firmware: buzzer e alerta local sem Wi-Fi | Pendente |
| Firmware: receber e guardar a configuração | Pendente |
| Página de configuração servida pelo ESP32 | Pendente |
| Aviso ao porteiro na página aberta | Pendente |
| Calibração com decibelímetro | Pendente |
| Case impresso em 3D | Pendente |
| Testes em campo | Pendente |

A pasta `simulacao-antiga/` (`script.ino`, `diagram.json` e `index.html`) guarda uma **simulação antiga** (ruído aleatório, painel de condomínio de demonstração) e não faz parte do projeto final. O firmware de verdade fica em `firmware/noisetrack/`.

## Como usar

1. Monte o circuito conforme as ligações acima.
2. Grave o firmware no ESP32 pelo cabo USB.
3. Ligue o aparelho pela fonte USB.
4. Com um dispositivo na mesma rede Wi-Fi, abra a página do aparelho e defina o limite e o tempo X.

## Licença

A definir.
