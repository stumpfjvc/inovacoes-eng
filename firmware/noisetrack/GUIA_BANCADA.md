# NoiseTrack: guia de bancada (Parte 11)

Guia para montar, testar e calibrar o NoiseTrack na placa de verdade. Foi
escrito para quem programa (Java, por exemplo) mas não tem prática com
eletrônica. Siga na ordem e **monte uma coisa de cada vez**: se algo falhar,
você sabe exatamente qual foi a última peça ligada.

Cada passo diz:

- **Fazer**: o que montar ou mudar.
- **Serial**: o que deve aparecer no monitor serial. Os textos são os do código;
  onde aparece `…` ou um valor de exemplo, o número muda de placa para placa.
- **Se não aparecer**: o que provavelmente é, e qual constante do `config.h`
  ajustar.

Toda mudança em `config.h` só vale depois de **compilar e gravar de novo**.

---

## 1. Material e antes de ir

| Item | Observação |
|---|---|
| ESP32 DevKit (30 ou 38 pinos) | Os dois servem: todos os pinos usados (4, 25, 26, 27, 33) existem nos dois. |
| 2 microfones INMP441 + 1 de reserva | O de reserva serve para descobrir se um microfone veio com defeito. |
| Módulo WS2812 com 4 LEDs | Pinos costumam se chamar `VCC`/`5V`, `GND` e `DIN`/`IN`. |
| Módulo de buzzer ativo de 3 pinos | Pinos `VCC`, `I/O` (ou `S`/`SIG`) e `GND`. |
| Protoboard e jumpers macho-fêmea e macho-macho | |
| Resistor de 220 a 470 Ω (opcional) | Para o sinal do WS2812. |
| Resistor de 100 kΩ (opcional) | Para a linha SD dos microfones, se o canal sem microfone ler lixo (ver seção 7). |
| Cabo micro USB **com dados** | Muitos cabos de celular só carregam. Se o PC não reconhece a placa, troque o cabo primeiro. |
| Decibelímetro ou aplicativo de celular | Para a calibração (seção 5). |

**Confirmar que o PC reconhece a placa (Windows):**

1. Ligue a placa no USB e abra o **Gerenciador de Dispositivos**.
2. Em **Portas (COM e LPT)** deve aparecer algo como
   `Silicon Labs CP210x USB to UART Bridge (COM5)` ou `USB-SERIAL CH340 (COM5)`.
3. Se aparecer um "dispositivo desconhecido" ou nada:
   - troque o cabo (o mais comum: cabo só de carga);
   - instale o driver do chip da placa: **CP2102** (driver "CP210x VCP" da
     Silicon Labs) ou **CH340** (driver "CH341SER" da WCH). O nome do chip está
     escrito no componente pequeno perto do conector USB;
   - tente outra porta USB do PC.

---

## 2. Preparar o código para a placa

**Fazer:**

1. Na pasta `firmware/noisetrack/`, copie `secrets.example.h` para `secrets.h`
   e troque o texto de `WIFI_AP_SEGREDO` por um texto longo e aleatório.
   - Use o **mesmo segredo em todas as placas do grupo** e guarde-o fora do Git
     (por exemplo, num gerenciador de senhas). As senhas de cada placa continuam
     diferentes, porque o MAC muda.
   - O `secrets.h` está no `.gitignore`. Antes de qualquer commit, confira que
     ele **não** aparece no `git status`.
2. No `config.h`, para a configuração final da placa:
   - `#define USANDO_WOKWI    0`
   - `#define FONTE_SIMULADA  0`
   - deixe todos os `TESTE_*` em `1` na primeira montagem.
   - Veja na seção 4 a sugestão de começar com `FONTE_SIMULADA 1` nas primeiras
     etapas.
3. Compile e grave com o PlatformIO no VS Code:
   - botão **✓ (Build)** só compila; botão **→ (Upload)** compila e grava;
   - botão da **tomada (Serial Monitor)** abre o monitor. A velocidade já é
     115200 (está no `platformio.ini`, em `monitor_speed`).
   - Pela linha de comando, na pasta do projeto:
     `C:\.platformio\penv\Scripts\platformio.exe run -t upload` e depois
     `C:\.platformio\penv\Scripts\platformio.exe device monitor -b 115200`.
   - Se a gravação parar em `Connecting........_____`, segure o botão **BOOT**
     da placa até começar a gravar.
   - Para ver a partida desde o começo, aperte o botão **EN** (reinicia a placa)
     com o monitor aberto.

> **Atenção: com `FONTE_SIMULADA = 0` não existe nenhum comando no Serial.**
> `80`, `auto`, `limite N`, `x N`, `wifi`, `wifi apagar` e `web` só existem com
> `FONTE_SIMULADA = 1`. Na configuração final, o limite e o X mudam pela página,
> e a senha Config só aparece na linha da etiqueta, na partida.

---

## 3. Ligações

> **Segurança antes de ligar o USB:**
> - O `VDD` dos INMP441 vai no **3V3**, **nunca no 5 V** (o microfone queima).
> - Confira duas vezes `VDD` e `GND` de cada módulo antes de energizar. Inverter
>   os dois pode queimar o módulo.
> - Monte e desmonte com o **USB desligado**.

```
          ESP32                          MIC 1 = canal A          MIC 2 = canal B
                                         (INMP441)                (INMP441)

    3V3 ----------+--------------------- VDD
                  +------------------------------------------------ VDD
                  +------------------------------------------------ L/R  (mic 2: L/R no 3V3)

    GND ----------+--------------------- GND
                  +--------------------- L/R  (mic 1: L/R no GND)
                  +------------------------------------------------ GND

    GPIO26 -------+--------------------- SCK
                  +------------------------------------------------ SCK
    GPIO25 -------+--------------------- WS
                  +------------------------------------------------ WS
    GPIO33 -------+--------------------- SD
                  +------------------------------------------------ SD


    VIN (5 V) ---------------------------- VCC   (módulo WS2812, 4 LEDs)
    GND ---------------------------------- GND
    GPIO4 ------[ 220 a 470 ohm ]--------- DIN   (resistor opcional)


    3V3 ---------------------------------- VCC   (módulo buzzer, 3 pinos)
    GND ---------------------------------- GND
    GPIO27 ------------------------------- I/O
```

| Módulo | Pino do módulo | Pino do ESP32 |
|---|---|---|
| Mic 1 e mic 2 (os dois juntos) | VDD | 3V3 |
| | GND | GND |
| | SCK | GPIO26 |
| | WS | GPIO25 |
| | SD | GPIO33 |
| Só o mic 1 | L/R | GND (canal esquerdo = A) |
| Só o mic 2 | L/R | 3V3 (canal direito = B) |
| WS2812 | VCC | VIN (5 V do USB) |
| | GND | GND |
| | DIN | GPIO4 (com resistor de 220 a 470 Ω, opcional) |
| Buzzer | VCC | 3V3 |
| | GND | GND |
| | I/O | GPIO27 |

**Por que o L/R é diferente:** os dois microfones dividem os mesmos três fios de
dados. O pino L/R diz a cada um em qual "metade" do sinal falar: no GND, ele
fala na metade esquerda (canal A); no 3V3, na direita (canal B). O L/R nunca
deve ficar solto.

**WS2812 em 5 V com sinal de 3,3 V:** o ESP32 manda o sinal em 3,3 V, e alguns
módulos alimentados com 5 V não entendem 3,3 V como "1". Se os LEDs falharem
(seção 7): ligue o `VCC` do módulo no **3V3** (os LEDs ficam um pouco mais
fracos e as cores podem mudar um pouco) ou use um conversor de nível lógico.

Os pinos estão em `config.h`: `PINO_MIC_SCK`, `PINO_MIC_WS`, `PINO_MIC_SD`,
`PINO_LEDS` e `PINO_BUZZER`.

---

## 4. Montagem em etapas

**Sugestão para as etapas (a), (b) e (c):** grave com `USANDO_WOKWI 0` e
`FONTE_SIMULADA 1`. A placa é a de verdade, mas o som vem da simulação. Assim
os comandos do Serial existem: digite `80` para forçar barulho, `auto` para
voltar, `limite 70`, `x 3`, `wifi` e `web`. Sem microfone e com
`FONTE_SIMULADA 0`, não há como forçar cores nem alerta, e o pino SD solto pode
dar leituras sem sentido. A partir da etapa (d), use `FONTE_SIMULADA 0`.

### (a) Só o ESP32

**Fazer:** nada ligado além do USB. Grave e abra o monitor.

**Serial** (na ordem, resumido):

```
=== NoiseTrack: firmware em desenvolvimento ===
Limite padrao: 65 dB | Tempo X padrao: 5 s
[teste] === autoteste do nivel em dB ===
…
[teste] === TUDO PASS (0 falha(s)) ===
[teste-regra] === autoteste da regra dos X segundos ===
…
[teste-regra] === TUDO PASS (0 falha(s)) ===
[audio] INMP441 por I2S, 2 microfone(s), 16000 Hz        <- com FONTE_SIMULADA 0
[audio] fonte SIMULADA, 2 microfone(s), canal B +1.7 dB   <- com FONTE_SIMULADA 1
[prefs] limite = 65 dB (padrao) | X = 5 s (padrao)
[teste-prefs] === autoteste da memoria ===
…
[teste-prefs] === TUDO PASS (0 falha(s)) ===
[teste-wifi] === autoteste da logica de rede ===
…
[teste-wifi] (senha de exemplo: …)
…
[teste-wifi] === TUDO PASS (0 falha(s)) ===
[teste-web] === autoteste da pagina ===
…
[teste-web] === TUDO PASS (0 falha(s)) ===
[rede] Rede: NoiseTrack-Setup | Senha: XXXXXXXX | Config: YYYYYYYY
[rede] nenhuma rede salva: o AP de setup vai ligar
[serial] comandos: …                                        <- só com FONTE_SIMULADA 1
[rede] estado=ap_setup
[rede] AP de setup ligado: 'NoiseTrack-Setup', portal em http://192.168.4.1/
[nivel] A = … | B = … | comb = …                            <- cerca de 8 por segundo
[vivo] 1000 ms                                              <- a cada segundo
[teste-tarefa] 24 janelas em 3000 ms (esperado 20 a 28): PASS
[teste-tarefa] pilha livre minima: … de 8192 bytes
```

- **Anote a linha `[rede] Rede: … | Senha: … | Config: …`**. É a etiqueta da
  placa (seção 6). A `(senha de exemplo: …)` do `[teste-wifi]` **não** é a da
  placa: é de um MAC de teste.
- Sem `secrets.h` aparece também
  `[rede] AVISO: sem secrets.h, as senhas (AP e config) usam o segredo padrao e sao previsiveis`.
  Na bancada tudo bem; para instalar, crie o `secrets.h` (seção 2).
- Anote o número da `pilha livre minima` (no Wokwi foi 6720 de 8192).

**Pronto quando:** todas as linhas `=== TUDO PASS ===` e o `[teste-tarefa] … PASS` aparecem.

**Se não aparecer:**
- Nada no monitor: velocidade diferente de 115200, porta COM errada, ou o
  monitor abriu depois da partida (aperte **EN**).
- Algum `HOUVE FAIL`: anote a linha com `FAIL` (ela diz o caso) e avise o grupo.
  Não é problema de montagem: os autotestes não usam os módulos.
- `[audio] ERRO i2s_driver_install: …` ou `[audio] ERRO i2s_set_pin: …`, seguido
  de `[audio] ERRO: fonte de audio nao iniciou`: o I2S não ligou. Confira os
  `PINO_MIC_*`.
- `[tarefa] ERRO: nao foi possivel criar a tarefa de audio`: falta memória;
  avise o grupo (`PILHA_TAREFA_AUDIO`).
- `[teste-tarefa] … FAIL`: a tarefa de áudio não está no ritmo. Com
  `FONTE_SIMULADA 0` e sem microfone, o I2S ainda gera as janelas; se der FAIL,
  avise o grupo.

### (b) Módulo WS2812

**Fazer:** USB desligado, ligue o WS2812 (VIN, GND, GPIO4). Religue.

**Serial:** não existe mensagem própria dos LEDs. Olhe a cor e as linhas `[regra]`.

**O que ver** (com `FONTE_SIMULADA 1`):

| Estado | Cor | Como provocar |
|---|---|---|
| calmo | verde | partida, ou `auto` |
| contando | amarelo | digite `80` |
| alerta | vermelho | `80` e espere 5 s |
| sem sinal | azul | só com microfone de verdade (etapa f) |

- Ao digitar `80`: `[sim] nivel forcado = 80 dB`, depois
  `[regra] estado=contando acima=0 ms` e, uns 5 s depois,
  `[regra] estado=alerta acima=5000 ms (disparou)` (o número pode passar um
  pouco de 5000).
- Ao digitar `auto`: `[sim] nivel automatico` e, uns 2 s depois,
  `[regra] estado=calmo acima=0 ms (normalizou)`.

**Pronto quando:** os 4 LEDs acendem juntos, com a cor certa, e o brilho não ofusca.

**Se não aparecer:**
- Nenhum LED: confira VIN e GND, o fio no `DIN` (não no `DOUT`) e o GPIO4.
- Cores erradas, piscando ou só o primeiro LED: veja "WS2812 piscando errado" na seção 7.
- Brilho forte ou fraco demais: `BRILHO_LEDS` (0 a 255; hoje 77, cerca de 30%).
- Verde e vermelho trocados: o módulo usa outra ordem de cores. A ordem está no
  código (`NEO_GRB` em `leds.cpp`), não no `config.h`: avise o grupo.

### (c) Buzzer

**Fazer:** USB desligado, ligue o buzzer (VCC no 3V3, GND, I/O no GPIO27). Religue.

**Serial:** não existe mensagem própria do buzzer. O buzzer acompanha o estado alerta.

**O que ver:** em silêncio no calmo e no contando. No alerta (`80` e esperar
5 s), bipes de 0,5 s ligado e 0,5 s desligado.

**Pronto quando:** silêncio na partida, bipe só no alerta.

**Se não aparecer:**
- **Apita sempre, e para justamente no alerta** (lógica invertida): troque
  `BUZZER_ATIVO_EM_BAIXO` de `0` para `1`.
- Apita sempre e não muda nada: confira se o fio está no GPIO27 e não no VCC.
- Não apita nunca: confira o VCC do módulo no 3V3 e o GND. Alguns módulos são
  mais fracos em 3,3 V.
- Ritmo do bipe: `BUZZER_BIPE_MS` (hoje 500).

### (d) Um microfone

**Fazer:**
1. No `config.h`: `FONTE_SIMULADA 0` e, **só nesta etapa**, `NUM_MICROFONES 1`.
   Com só um microfone ligado, o canal B fica com a linha solta e pode ler lixo.
2. USB desligado, ligue o **mic 1**: VDD no **3V3**, GND, L/R no GND, SCK 26,
   WS 25 e SD 33. Grave e abra o monitor.

**Serial:**
```
[audio] INMP441 por I2S, 1 microfone(s), 16000 Hz
[nivel] A = 42.3 dB | B = -- | comb = 42.3 dB
```
- Em silêncio: um valor estável (o número depende da sala e da calibração).
- Falando perto: sobe 15 a 30 dB. Batendo palmas: picos bem mais altos.

**Pronto quando:** o nível de A responde na hora a fala e palmas e volta ao normal.

**Se não aparecer:**
- `A = --` sempre e `[regra] estado=sem_sinal …`: o microfone não manda dados.
  Confira VDD (3V3), GND, SD no GPIO33 e o L/R no GND. Teste o microfone reserva.
- A sempre preso em `20.0 dB` (o piso, `DB_PISO`): o sinal é quase zero. Mesmas
  conferências. Se só funcionar com `MIC_CANAIS_TROCADOS true`, o ESP32 está
  lendo o canal do outro lado: deixe assim e avise o grupo.
- Números que pulam sem sentido (90, 110 dB em silêncio): fio solto, sobretudo
  SCK ou WS, ou falta o GND comum.
- `[audio] falha no i2s_read: …, … de … bytes`: o I2S parou; veja a seção 7.

### (e) Segundo microfone e ordem dos canais

**Fazer:** USB desligado, ligue o **mic 2** nos mesmos fios (VDD, GND, SCK, WS e
SD) e o **L/R dele no 3V3**. No `config.h`: `NUM_MICROFONES 2`. Grave.

**Serial:**
```
[audio] INMP441 por I2S, 2 microfone(s), 16000 Hz
[nivel] A = 42.3 dB | B = 43.1 dB | comb = 42.7 dB
```

**Teste da ordem:** bata o dedo de leve perto do **mic 1** (não no outro). O
**A** deve subir bem mais que o B. Depois faça o mesmo no mic 2: o B deve subir.

**Pronto quando:** cada microfone mexe no canal certo, e em silêncio A e B ficam
a poucos dB um do outro.

**Se não aparecer:**
- Ao bater no mic 1 sobe o **B**: troque `MIC_CANAIS_TROCADOS` para `true`.
- B sempre `--`: o mic 2 não fala. Confira o L/R dele no 3V3 e o VDD.
- A e B iguais o tempo todo, mesmo batendo num só: os dois L/R estão no mesmo
  lado (os dois no GND ou os dois no 3V3).
- A e B diferentes em 5 dB ou mais, em silêncio: é normal entre microfones;
  a calibração por microfone corrige (seção 5).

### (f) Sem sinal

**Fazer** (sempre com o USB desligado para tirar e pôr fios):

Tire o microfone **inteiro** (os 6 fios dele), não só o VDD. Um microfone sem
alimentação mas com os fios de dados ligados pode ser alimentado "pelas costas"
pelos pinos de dados e atrapalhar o outro.

1. Tire **só o mic 2** e ligue o USB. Esperado:
   `[nivel] A = 42.3 dB | B = -- | comb = 42.3 dB (canal sem sinal fora da conta)`.
   O LED continua seguindo o mic 1. **Um microfone sem sinal não leva a
   SEM_SINAL**: o canal só sai da conta.
2. Tire **os dois** e ligue o USB. Esperado:
   `[nivel] A = -- | B = -- | comb = --`,
   `[regra] estado=sem_sinal acima=0 ms (perdeu_sinal)` e o **LED azul**, sem buzzer.
3. Recoloque os fios e ligue de novo. Esperado: `[regra] estado=calmo acima=0 ms`
   e LED verde.

Se aparecer também ` [I2S sem resposta]` no fim da linha `[regra]`, o problema
não é o microfone e sim a leitura do I2S, que falhou `FALHAS_ATE_SEM_SINAL`
vezes seguidas (5). Veja a seção 7.

**Pronto quando:** sem os dois microfones o LED fica azul e não há alerta; com
eles de volta, verde.

**Se não aparecer:** se sem microfone os números continuam pulando em vez de
`--`, a linha SD solta está lendo lixo. Ponha um resistor de 100 kΩ entre o SD
(GPIO33) e o GND, como recomenda o datasheet do INMP441.

### (g) Wi-Fi pelo portal

**Fazer:**
1. Na partida sem rede salva, aparece
   `[rede] AP de setup ligado: 'NoiseTrack-Setup', portal em http://192.168.4.1/`.
2. No celular, conecte na rede **NoiseTrack-Setup** com a **Senha** da etiqueta.
   O celular deve abrir a página sozinho ("Fazer login na rede"). Se não abrir,
   digite `http://192.168.4.1/` no navegador.
3. Digite o nome e a senha do Wi-Fi do local e toque **Salvar e conectar**.

**Serial:**
```
[rede] portal salvou a rede 'MinhaRede' (gravada)
[rede] estado=conectando
[rede] tentando conectar a 'MinhaRede'
[rede] estado=conectado
[rede] AP de setup desligado
[rede] conectado a 'MinhaRede' | IP 192.168.0.42 | http://noisetrack.local
[ntp] pedindo o horario (sem internet, segue sem horario)
[web] pagina em http://192.168.0.42/ e http://noisetrack.local/ (ao vivo na porta 81)
[ntp] horario acertado: 19/11/2026 14:03:22 (UTC-3)
```

- Anote o **IP**. `noisetrack.local` funciona no iPhone, no Mac e no Windows; em
  muitos Android **só o IP** funciona.
- Na próxima partida, com a rede salva, aparecem `[rede] estado=conectando`,
  `[rede] tentando conectar a 'MinhaRede'` e, em seguida, o `conectado`.

**Pronto quando:** aparece a linha `[rede] conectado a …` com o IP, e a rede
NoiseTrack-Setup some.

**Se não aparecer:**
- Senha do Wi-Fi errada: depois de `WIFI_TIMEOUT_MS` (15 s) volta
  `[rede] estado=ap_setup`, e a rede NoiseTrack-Setup reaparece para tentar de novo.
- A página recusa: o nome deve ter de 1 a 32 caracteres, e a senha deve ser
  vazia (rede aberta) ou ter de 8 a 63.
- Rede de 5 GHz: o ESP32 só conecta em **2,4 GHz**.
- Sem internet no local: o horário não acerta (o `[ntp] horario acertado` não
  aparece), mas o resto funciona.

### (h) Página no celular

**Fazer:** com o celular **no mesmo Wi-Fi** do aparelho, abra `http://IP/` (ou
`http://noisetrack.local/`).

**O que ver e testar:**
- Ponteiro e número acompanham o som. O indicador mostra "ao vivo".
- Toque **Ativar som** e provoque um alerta: bipe a cada 1 s. O navegador só
  deixa tocar depois desse toque. No iPhone, o som para quando a tela trava.
- Configuração: limite, X e a **Config** da etiqueta. Salvar mostra "Salvo.".
- Erre a senha 5 vezes seguidas: aparece "Muitas tentativas erradas…", e a
  configuração fica bloqueada por `WEB_BLOQUEIO_MS` (60 s).

**Serial:**
```
[web] cliente ao vivo conectado (1 de 3)
[web] configuracao pela pagina: limite = 70 dB, X = 8 s (gravado)
[web] senha de configuracao errada (restam 4)
[web] configuracao bloqueada por 60 s
```
- Não existe mensagem de cliente desconectado.
- Um 4º celular ao mesmo tempo gera
  `[web] cliente ao vivo recusado: limite de clientes (usa o polling)`. A página
  dele continua funcionando, pela consulta de reserva (`WEB_MAX_CLIENTES_WS`).

**Pronto quando:** ponteiro ao vivo, bipe no alerta, configuração salva com a
senha certa e bloqueio depois de 5 erros.

### (i) Queda do roteador

**Fazer:** com a página aberta, desligue o roteador por uns 30 s e religue.

**Serial:**
```
[rede] estado=caiu
[web] pagina desligada (Wi-Fi do local fora)
[rede] tentando conectar a 'MinhaRede'        <- a cada WIFI_RETRY_MS (5 s)
…
[rede] estado=conectado
[rede] conectado a 'MinhaRede' | IP 192.168.0.42 | http://noisetrack.local
[web] pagina em http://192.168.0.42/ e http://noisetrack.local/ (ao vivo na porta 81)
```
O ESP32 pode levar alguns segundos para perceber que o roteador sumiu.

**Durante a queda, provoque barulho:** o LED e o buzzer devem funcionar igual,
porque o alerta local não depende da rede. A página mostra "sem conexão" e volta
sozinha.

**Pronto quando:** volta sozinho e o alerta local funcionou sem rede.

### (j) Teste do alerta de verdade

**Fazer:** limite 65 dB e X 5 s (os padrões; confira na página).
1. **Ruído contínuo** acima de 65 dB por uns 10 s (música alta ou um tom no
   celular, perto do aparelho).
2. **Ruído curto**: palmas ou uma fala alta de 2 s.

**Serial:**
- Contínuo: `[regra] estado=contando acima=0 ms`, uns 5 s depois
  `[regra] estado=alerta acima=5000 ms (disparou)` (LED vermelho e bipe), e uns
  2 s depois do silêncio `[regra] estado=calmo acima=0 ms (normalizou)`.
- Curto: `[regra] estado=contando acima=0 ms` e logo `[regra] estado=calmo acima=0 ms`, sem alerta.

**Pronto quando:** o contínuo dispara perto de 5 s e o curto não dispara.

**Se não aparecer:** confira o nível na linha `[nivel]`. Se o ruído "alto" não
passa de 65 dB, o aparelho pode estar medindo baixo: faça a calibração antes.
Oscilações entre 62 e 65 dB não zeram a contagem (`HISTERESE_DB` = 3), e o
alerta só acaba depois de `HOLD_MS` (2 s) abaixo de 62 dB.

---

## 5. Calibração

**A conta que o aparelho faz** (para cada microfone):

```
dB SPL = dBFS + OFFSET_BASE_DB + OFFSET_CAL_MIC_x_DB
       = dBFS + 120            + ajuste do microfone
```

- **dBFS** mede o quanto o sinal é forte em relação ao maior valor que o
  microfone consegue entregar. O dBFS é sempre negativo, e quanto mais perto de
  0, mais alto o som.
- **120** vem do datasheet do INMP441: um som de **94 dB SPL dá −26 dBFS**.
  Então `dB SPL = dBFS + 94 − (−26) = dBFS + 120`.
- Esse 120 pode estar **uns 3 dB fora**. O datasheet mede os −26 dBFS em
  relação a uma senoide no máximo, e o nosso código usa outra convenção, em que
  essa senoide dá −3 dBFS. Pela conta do datasheet, o certo poderia ser **123**.
  Na prática: não estranhe se o aparelho medir uns 3 dB **abaixo** do
  decibelímetro. A calibração corrige isso junto com o resto.
- **Ponderação:** apps de celular e muitos decibelímetros medem em **dB(A)**,
  que dá menos peso aos graves, como o ouvido. O NoiseTrack mede **sem
  ponderação**. Em fala e ruído comum, a diferença é pequena (1 a 3 dB). Com
  graves fortes (música com batida, motor), o NoiseTrack mede mais alto que o
  dB(A). Se o decibelímetro tiver modo **C** ou **Z**, use-o: fica mais perto do
  aparelho. Use também o modo **FAST**: ele é de 125 ms, como a janela do
  aparelho.
- **A calibração é por microfone:** cada um tem seu ajuste
  (`OFFSET_CAL_MIC_A_DB` e `OFFSET_CAL_MIC_B_DB`).

**Como fazer:**

1. Decibelímetro e aparelho lado a lado, à mesma distância da fonte de som.
2. Em cada nível, espere uns 5 s parado e anote o valor do decibelímetro e os
   valores **A** e **B** da linha `[nivel]` (valores típicos, não um pico).
3. Evite níveis abaixo de uns 35 dB: ali o INMP441 não mede bem (o ruído
   próprio dele é dessa ordem).

| Nível | Decibelímetro (dB) | A do aparelho (dB) | B do aparelho (dB) | Diferença A = medidor − A | Diferença B = medidor − B |
|---|---|---|---|---|---|
| Silêncio do ambiente | | | | | |
| Fala normal (1 m) | | | | | |
| Música | | | | | |
| Palmas contínuas | | | | | |
| **Média** | | | | | |

4. **Ajuste:** `OFFSET_CAL_MIC_A_DB = média das diferenças de A` e o mesmo para B.
   - Exemplo: o medidor marca 70 e o A marca 66,5, então diferença = 70 − 66,5 =
     +3,5. Se a média de A for +3,5, use `constexpr float OFFSET_CAL_MIC_A_DB = 3.5f;`.
   - Diferença positiva: o aparelho mede baixo e o ajuste soma. Negativa: ele
     mede alto e o ajuste subtrai.
   - Fique entre **−20 e +20**. O código não confere esse limite para os ajustes
     do `config.h`, mas um ajuste maior que isso indica fiação errada,
     microfone com defeito ou medição mal feita.
   - Se as diferenças variam muito de um nível para outro (5 dB ou mais), o
     problema não é só de ajuste: confira a ponderação (A × C/Z), a distância e
     se o microfone está tapado.
5. Grave de novo e repita uma ou duas medições para conferir: a diferença deve
   ficar perto de 0 (±2 dB).

> As chaves `offset_cal_a` e `offset_cal_b` que existem na memória da placa
> **ainda não são usadas** na conta. Hoje a calibração é só pelo `config.h`.

---

## 6. Antes de instalar

- [ ] `TESTE_PREFS 0`: esse autoteste grava na flash a cada partida.
- [ ] Os outros `TESTE_*` em `0`, se quiser uma partida mais rápida
      (`TESTE_NIVEL_DB`, `TESTE_REGRA`, `TESTE_TAREFA`, `TESTE_WIFI`, `TESTE_WEB`).
- [ ] `USANDO_WOKWI 0`, `FONTE_SIMULADA 0`, `NUM_MICROFONES 2`, e os ajustes de
      calibração preenchidos.
- [ ] `secrets.h` criado, com o segredo do grupo, e **fora** do `git status`.
- [ ] **Etiqueta:** a linha `[rede] Rede: NoiseTrack-Setup | Senha: XXXXXXXX | Config: YYYYYYYY`
      só aparece no Serial **na partida** (não existe comando para mostrá-la
      com `FONTE_SIMULADA 0`). Anote na primeira vez, imprima e cole no aparelho
      (ou entregue a Config só ao síndico). Ela muda se o `secrets.h` mudar.
- [ ] **Trocar a rede salva depois de instalado:**
  - Com `FONTE_SIMULADA 0` não existe `wifi apagar`, e a placa não tem botão
    para isso (o botão **BOOT** não é lido pelo firmware; o **EN** só reinicia).
  - Sem cabo: reinicie o aparelho **sem** a rede salva disponível (roteador
    desligado ou longe). Ele tenta por 15 s (`WIFI_TIMEOUT_MS`) e liga o portal
    NoiseTrack-Setup. Uma queda no meio do uso **não** abre o portal: só a
    partida sem conseguir conectar.
  - Com cabo: grave com `FONTE_SIMULADA 1` e use `wifi apagar`, ou use
    "Erase Flash" no PlatformIO (apaga tudo, inclusive limite e X, e exige
    gravar de novo).
  - **Sugestão para o case:** um furo para alcançar o botão **EN** (reiniciar).
    Um furo para o **BOOT** só serve se, no futuro, o firmware passar a usá-lo
    (por exemplo, "segurar BOOT na partida apaga a rede").

---

## 7. Problemas comuns

| Sintoma | Causa provável | O que fazer |
|---|---|---|
| Placa não aparece na COM | Cabo só de carga; falta driver | Trocar o cabo; instalar driver CP210x ou CH340; outra porta USB. |
| Gravação para em `Connecting........_____` | A placa não entrou em modo de gravação | Segurar **BOOT** durante o `Connecting`. |
| `[audio] falha no i2s_read: …, … de … bytes` | I2S sem resposta (fiação ou pino errado) | Conferir SCK 26, WS 25, SD 33 e o GND comum. Depois de 5 falhas seguidas (`FALHAS_ATE_SEM_SINAL`) aparece ` [I2S sem resposta]` na linha `[regra]`, e o estado vira sem sinal. |
| "RMS sempre perto de zero" | Não existe mais mensagem de RMS bruto (saiu na Parte 5). O sintoma equivalente é o nível preso em `20.0 dB` (`DB_PISO`) ou `--` | Conferir VDD no 3V3, L/R ligado (nunca solto), SD no GPIO33; testar o microfone reserva; testar `MIC_CANAIS_TROCADOS true`. |
| Canal sem microfone lê valores aleatórios | Linha SD solta enquanto o microfone não fala | Com um microfone só, usar `NUM_MICROFONES 1`; ou resistor de 100 kΩ entre SD e GND. |
| Níveis 10 a 20 dB fora do decibelímetro | Convenção do 120/123, ponderação dB(A), distância, microfone tapado ou com defeito | Calibrar (seção 5). Se passar de 20 dB, conferir fiação e L/R, e trocar pelo reserva. |
| A e B muito diferentes | Microfones de lotes diferentes, ou um tapado | Calibrar cada um; conferir se nada encosta no furo do microfone. |
| WS2812 piscando errado, cores trocadas, só o 1º LED | Sinal de 3,3 V fraco para 5 V; falta GND comum; fio no `DOUT` | GND comum; fio no `DIN`; resistor de 220 a 470 Ω; ligar o VCC do módulo no 3V3 ou usar conversor de nível. Verde e vermelho trocados: ordem de cores no código (`NEO_GRB` em `leds.cpp`), avisar o grupo. |
| Buzzer sempre ligado | Módulo de lógica invertida; fio I/O no lugar errado | `BUZZER_ATIVO_EM_BAIXO 1`; conferir I/O no GPIO27 e VCC no 3V3. |
| Portal não abre sozinho no celular | Dados móveis ligados; "DNS privado" do Android | Digitar `http://192.168.4.1/`; desligar os dados móveis durante o setup; desligar o "DNS privado". |
| Página não carrega por `noisetrack.local` | Muitos Android não resolvem `.local`; celular em outra rede | Usar o IP da linha `[rede] conectado a …`; conferir se o celular está no mesmo Wi-Fi (não na rede de convidados). |
| Página abre mas fica em "reserva" | Porta 81 bloqueada pela rede, ou já há 3 clientes ao vivo | Funciona mesmo assim (consulta a cada 0,4 s). Fechar abas antigas. |
| Rede salva não conecta nunca | Rede de 5 GHz, senha errada ou nome com erro | Usar 2,4 GHz; refazer pelo portal (volta sozinho depois de 15 s sem conectar). |

---

## 8. Registro de resultados

| Data | Passo | Resultado | Ajuste feito |
|---|---|---|---|
| | (a) Só o ESP32 | | |
| | (b) WS2812 | | |
| | (c) Buzzer | | |
| | (d) Um microfone | | |
| | (e) Segundo microfone e ordem | | |
| | (f) Sem sinal | | |
| | (g) Wi-Fi pelo portal | | |
| | (h) Página no celular | | |
| | (i) Queda do roteador | | |
| | (j) Teste do alerta | | |
| | Calibração mic A | | `OFFSET_CAL_MIC_A_DB = ` |
| | Calibração mic B | | `OFFSET_CAL_MIC_B_DB = ` |
| | Pilha livre mínima | | |
| | Etiqueta anotada | | |
