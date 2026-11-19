# NoiseTrack: guia de bancada (Parte 11)

Guia para montar, testar e calibrar o NoiseTrack na placa de verdade. Foi
escrito para quem programa (Java, por exemplo) mas não tem prática com
eletrônica. Siga na ordem e **monte uma coisa de cada vez**: se algo falhar,
você sabe exatamente qual foi a última peça ligada.

Cada passo diz:

- **Fazer**: o que montar ou mudar.
- **Serial**: o que deve aparecer no monitor serial. Os textos são os do código;
  onde aparece `…` ou um valor de exemplo, o número muda de placa para placa.
- **Se não aparecer**: o que provavelmente é, e qual chave do `config_local.h`
  ajustar.

**Na bancada, não edite o `config.h`.** Tudo o que muda de uma etapa para outra
fica no `config_local.h` (seção 2), que não vai para o Git. Toda mudança só vale
depois de **compilar e gravar de novo**.

---

## 1. Material

Este é o material que o grupo tem (conferido nas fotos):

| Item | Observação |
|---|---|
| ESP32 **NodeMCU ESP-32S, 38 pinos**, com pinos soldados | Os pinos têm nomes impressos como `P26`, `P25`, `3V3`, `GND`, `5V`. Tem dois botões: **IO0** e **EN**. |
| 2 microfones **INMP441**, com pinos soldados | Pinos: `SCK`, `WS`, `L/R`, `SD`, `VDD`, `GND`. |
| Módulo de **buzzer** ativo de 3 pinos | Pinos: `GND`, `I/O`, `VCC`. Marcado **"低电平触发"** = "acionado em nível BAIXO": ele apita quando o pino `I/O` recebe 0 V. |
| Módulo de **4 LEDs HW-222** (WS2812) **sem pinos** | Dois grupos de furos (pads): **J1** = `GND`, `VCC`, `OUT`, `GND` e **J2** = `GND`, `VCC`, `IN`, `GND`. Usamos **só o J2**, com fios soldados. |
| Cabo micro USB **com dados** | Muitos cabos de celular só carregam. Se o PC não reconhece a placa, troque o cabo primeiro. |
| Protoboard de 830 pontos e uns 15 jumpers macho-macho | |
| Decibelímetro ou aplicativo de celular | Para a calibração (seção 5). |

**Os dois botões da placa:**

- **IO0** é o botão **BOOT**. Só serve para gravar: se a gravação parar em
  `Connecting........_____`, segure o IO0 até começar a gravar. O firmware não
  lê esse botão.
- **EN** é o **reset**: reinicia a placa. Aperte com o monitor aberto para ver a
  partida desde o começo.

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

### 2.1 `secrets.h` (senhas do aparelho)

Na pasta `firmware/noisetrack/`, copie `secrets.example.h` para `secrets.h` e
troque o texto de `WIFI_AP_SEGREDO` por um texto longo e aleatório.

- Use o **mesmo segredo em todas as placas do grupo** e guarde-o fora do Git
  (por exemplo, num gerenciador de senhas). As senhas de cada placa continuam
  diferentes, porque o MAC muda.
- O `secrets.h` está no `.gitignore`. Antes de qualquer commit, confira que ele
  **não** aparece no `git status`.

### 2.2 `config_local.h` (a configuração da sua placa)

O `config.h` do repositório fica com os valores do **Wokwi** (simulador). Para a
placa, copie `config_local.example.h` para **`config_local.h`**, na mesma pasta.

- O `config.h` inclui o `config_local.h` logo no começo, se ele existir. O que
  estiver definido no `config_local.h` vale; o resto usa o padrão do `config.h`.
  É parecido com um arquivo `.properties` do Java que sobrescreve os padrões.
- O `config_local.h` está no `.gitignore`: **não vai para o Git**. Confira no
  `git status` antes de cada commit.
- Sem `config_local.h`, o firmware fica exatamente como no Wokwi.
- Para voltar uma chave ao padrão, apague a linha dela. Para voltar tudo, apague
  o arquivo.
- Se você criar, mudar ou apagar o `config_local.h` e o Serial não mostrar a
  mudança, faça **Clean** no PlatformIO (ícone da lixeira, ou
  `platformio.exe run -t clean`) e compile de novo.

Chaves do `config_local.h` e o valor do exemplo da bancada:

| Chave | Exemplo | Quando trocar |
|---|---|---|
| `USANDO_WOKWI` | `0` | Nunca na placa. |
| `FONTE_SIMULADA` | `1` | Etapas (a), (b), (c) com som simulado; **`0` a partir da etapa (d)**. |
| `NUM_MICROFONES` | `1` | `1` na etapa (d); **`2` a partir da etapa (e)**. |
| `LOCAL_MIC_CANAIS_TROCADOS` | `false` | `true` só se, na etapa (e), bater no mic 1 fizer subir o B. |
| `LOCAL_OFFSET_CAL_MIC_A_DB` / `_B_DB` | `0.0f` | Depois da calibração (seção 5). |
| `BUZZER_ATIVO_EM_BAIXO` | `1` | O nosso buzzer é de nível baixo. Só troque para `0` se ele apitar sempre e calar no alerta. |
| `COMANDOS_SERIAL` | `1` | `0` desliga a leitura de comandos no Serial (opcional, ao instalar). |
| `CALIBRACAO` | `0` | `1` durante a calibração (seção 5). |
| `TESTE_NIVEL_DB`, `TESTE_REGRA`, `TESTE_TAREFA`, `TESTE_PREFS`, `TESTE_WIFI`, `TESTE_WEB` | `1` | Ao instalar: `TESTE_PREFS 0` (seção 6). |

> `BUZZER_ATIVO_EM_BAIXO` continua `0` no `config.h` porque o Wokwi depende
> disso. Na bancada, quem diz `1` é o `config_local.h`.

### 2.3 Compilar, gravar e abrir o monitor

- No VS Code (PlatformIO): botão **✓ (Build)** só compila; botão **→ (Upload)**
  compila e grava; botão da **tomada (Serial Monitor)** abre o monitor. A
  velocidade já é 115200 (`monitor_speed` no `platformio.ini`).
- O jeito mais simples é o botão **Build** do VS Code. Pela linha de comando,
  o `pio` não está no PATH: o PlatformIO fica na pasta `.platformio` do usuário,
  e o caminho muda de PC para PC. Exemplos:
  `C:\.platformio\penv\Scripts\platformio.exe` e
  `C:\Users\<usuario>\.platformio\penv\Scripts\platformio.exe`. Na pasta
  `firmware/noisetrack/`, com o caminho do seu PC no lugar de `platformio.exe`:
  `platformio.exe run -t upload` e depois `platformio.exe device monitor -b 115200`.
- Se a gravação parar em `Connecting........_____`, segure o botão **IO0
  (BOOT)** até começar a gravar.
- Para ver a partida desde o começo, aperte o **EN** com o monitor aberto.

### 2.4 Comandos no Serial

Com `COMANDOS_SERIAL 1` (padrão), digite no monitor e aperte Enter:

| Comando | O que faz | Onde existe |
|---|---|---|
| `etiqueta` | Mostra a etiqueta: senha do AP de setup e senha Config | Placa real e simulação |
| `wifi` | Estado da rede, IP e horário | Placa real e simulação |
| `wifi apagar` | Apaga a rede salva (o AP de setup liga) | Placa real e simulação |
| `web` | Endereço da página, clientes e senha de configuração | Placa real e simulação |
| `limite 70` | Muda e grava o limite (30 a 120 dB) | Placa real e simulação |
| `x 8` | Muda e grava o tempo X (1 a 600 s) | Placa real e simulação |
| `80` / `auto` | Força o nível simulado / volta ao automático | **Só com `FONTE_SIMULADA 1`** |

Na partida aparece a lista do que existe na configuração gravada:

```
[serial] comandos: um nivel (20 a 120 dB) ou 'auto' | 'limite N' | 'x N' | 'wifi' | 'wifi apagar' | 'web' | 'etiqueta'   <- FONTE_SIMULADA 1
[serial] comandos: 'limite N' | 'x N' | 'wifi' | 'wifi apagar' | 'web' | 'etiqueta'                                        <- FONTE_SIMULADA 0
[serial] comandos desligados (COMANDOS_SERIAL 0)
```

**Com o microfone de verdade (`FONTE_SIMULADA 0`) o nível forçado não existe.**
Se digitar `80`, aparece
`[serial] '80' ignorado: nivel forcado so existe com FONTE_SIMULADA 1`. Para
provocar alerta, faça barulho de verdade ou baixe o limite (`limite 40`).

Respostas típicas:

```
[rede] Rede: NoiseTrack-Setup | Senha: XXXXXXXX | Config: YYYYYYYY      <- etiqueta
[prefs] limite = 70 aceito, gravado                                       <- limite 70
[prefs] limite 200 recusado (fora da faixa)                               <- limite 200
[prefs] 'limite abc' ignorado: use 'limite 30' a 'limite 120'             <- texto inválido nunca vira 0
[wifi] rede salva apagada                                                 <- wifi apagar
[serial] 'oi' ignorado                                                    <- comando desconhecido
```

---

## 3. Ligações

> **Segurança antes de ligar o USB:**
> - O `VDD` dos INMP441 vai no **3V3**, **nunca no 5V** (o microfone queima).
> - Confira duas vezes `VCC`/`VDD` e `GND` de cada módulo antes de energizar.
>   Inverter os dois pode queimar o módulo.
> - Monte, desmonte e solde sempre com o **USB desligado**.

### 3.1 Tabela de ligações

| Módulo | Pino do módulo | Pino da placa (NodeMCU ESP-32S) |
|---|---|---|
| Mic 1 **e** mic 2 (os dois juntos) | `VDD` | **`3V3`** (nunca `5V`) |
| | `GND` | `GND` |
| | `SCK` | `P26` |
| | `WS` | `P25` |
| | `SD` | `P33` |
| Só o mic 1 (canal A) | `L/R` | `GND` |
| Só o mic 2 (canal B) | `L/R` | `3V3` |
| Buzzer | `VCC` | **`3V3`** |
| | `GND` | `GND` |
| | `I/O` | `P27` |
| LEDs HW-222, lado **J2** | `IN` | `P4` |
| | `VCC` | **`5V`** (se falhar, `3V3`: ver 3.4) |
| | `GND` | `GND` (qualquer um dos dois `GND` do J2) |

```
   NodeMCU ESP-32S                 MIC 1 = canal A        MIC 2 = canal B
                                   (INMP441)              (INMP441)

    3V3 ---------+---------------- VDD
                 +-------------------------------------- VDD
                 +-------------------------------------- L/R   (mic 2: L/R no 3V3)
                 +---------------- VCC do buzzer

    GND ---------+---------------- GND
                 +---------------- L/R                          (mic 1: L/R no GND)
                 +-------------------------------------- GND
                 +---------------- GND do buzzer
                 +---------------- GND do J2 (HW-222)

    P26 ---------+---------------- SCK
                 +-------------------------------------- SCK
    P25 ---------+---------------- WS
                 +-------------------------------------- WS
    P33 ---------+---------------- SD
                 +-------------------------------------- SD

    P27 -------------------------- I/O do buzzer
    P4  -------------------------- IN  do J2 (HW-222)
    5V  -------------------------- VCC do J2 (HW-222)
```

Na protoboard, use as duas linhas compridas das bordas como "barramentos": uma
para o **3V3** e outra para o **GND**. Assim cada módulo pega 3V3 e GND dali,
e todos ficam com o **GND comum** (obrigatório).

Os pinos estão no `config.h` (`PINO_MIC_SCK`, `PINO_MIC_WS`, `PINO_MIC_SD`,
`PINO_LEDS`, `PINO_BUZZER`) e não precisam mudar.

### 3.2 Microfones: por que o L/R é diferente

Os dois microfones dividem os mesmos três fios de dados (`SCK`, `WS`, `SD`). O
pino `L/R` diz a cada um em qual "metade" do sinal falar: no `GND`, ele fala na
metade esquerda (canal **A**); no `3V3`, na direita (canal **B**). O `L/R`
nunca deve ficar solto.

### 3.3 Buzzer de nível baixo

O módulo é "acionado em nível BAIXO": ele apita quando o `I/O` está em 0 V e
fica quieto quando está em 3,3 V. Por isso `BUZZER_ATIVO_EM_BAIXO 1` no
`config_local.h`.

- Ligue o `VCC` do buzzer no **3V3**, e não no 5V: assim o "desligado" do ESP32
  (3,3 V) é igual ao `VCC` do módulo e ele cala de verdade.
- Na hora de ligar o USB, antes do firmware começar, o buzzer pode dar um
  estalo ou bipe curto. Se parar logo, é normal.

### 3.4 LEDs HW-222: soldar no lado J2

O HW-222 vem sem pinos. Ele tem dois lados:

- **J2** = `GND`, `VCC`, `IN`, `GND`: é a **entrada** do sinal. É aqui que a
  placa liga.
- **J1** = `GND`, `VCC`, `OUT`, `GND`: é a **saída**, para encadear outro
  módulo. **Não use.** Ligar o `P4` no `OUT` não acende nada.

Solde três fios no J2 (`IN`, `VCC` e um dos `GND`) e espete a outra ponta na
protoboard. Dicas para quem nunca soldou: ferro bem quente e limpo, encoste o
ferro no pad e no fio juntos por 1 a 2 s, encoste a solda ali (não no ferro) e
tire. A solda deve ficar brilhante e em forma de "montinho". Confira com o
olho (ou um multímetro) que um pad não encostou no vizinho.

**`VCC` em 5V com sinal de 3,3 V:** o ESP32 manda o sinal em 3,3 V, e alguns
módulos alimentados com 5V não entendem 3,3 V como "1". Se os LEDs falharem
(seção 7), passe o `VCC` do J2 para o **3V3**: os LEDs ficam um pouco mais
fracos e as cores podem mudar um pouco.

---

## 4. Montagem em etapas

**Etapas (a), (b) e (c): placa real com som simulado.** Use o `config_local.h`
do exemplo, como está (`USANDO_WOKWI 0`, `FONTE_SIMULADA 1`). A placa é a de
verdade, mas o som vem da simulação: digite `80` para forçar barulho, `auto`
para voltar. A partir da etapa (d), `FONTE_SIMULADA 0`.

### (a) Só o ESP32

**Fazer:** nada ligado além do USB. Grave e abra o monitor.

**Serial** (na ordem, resumido):

```
=== NoiseTrack: firmware em desenvolvimento ===
Limite padrao: 65 dB | Tempo X padrao: 5 s
[teste] === autoteste do nivel em dB ===
…
[teste] 7 calib media de 80 e 70: 77.404 (esperado 77.404) PASS
…
[teste] === TUDO PASS (0 falha(s)) ===
[teste-regra] === autoteste da regra dos X segundos ===
…
[teste-regra] === TUDO PASS (0 falha(s)) ===
[audio] fonte SIMULADA, 1 microfone(s)                    <- com FONTE_SIMULADA 1
[audio] INMP441 por I2S, 1 microfone(s), 16000 Hz         <- com FONTE_SIMULADA 0
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
[serial] comandos: um nivel (20 a 120 dB) ou 'auto' | 'limite N' | 'x N' | 'wifi' | 'wifi apagar' | 'web' | 'etiqueta'
[rede] estado=ap_setup
[rede] AP de setup ligado: 'NoiseTrack-Setup', portal em http://192.168.4.1/
[nivel] A = … | B = -- | comb = …                           <- cerca de 8 por segundo
[vivo] 1000 ms                                              <- a cada segundo
[teste-tarefa] 24 janelas em 3000 ms (esperado 20 a 28): PASS
[teste-tarefa] pilha livre minima: … de 8192 bytes
```

- **Anote a linha `[rede] Rede: … | Senha: … | Config: …`**. É a etiqueta da
  placa (seção 6). Ela também aparece a qualquer momento com o comando
  `etiqueta`. A `(senha de exemplo: …)` do `[teste-wifi]` **não** é a da placa:
  é de um MAC de teste.
- Sem `secrets.h` aparece também
  `[rede] AVISO: sem secrets.h, as senhas (AP e config) usam o segredo padrao e sao previsiveis`.
  Na bancada tudo bem; para instalar, crie o `secrets.h` (seção 2.1).
- Anote o número da `pilha livre minima` (no Wokwi foi 6720 de 8192).
- Com `NUM_MICROFONES 1`, o B aparece sempre como `--`: é esperado.

**Pronto quando:** todas as linhas `=== TUDO PASS ===` e o `[teste-tarefa] … PASS` aparecem.

**Se não aparecer:**
- Nada no monitor: velocidade diferente de 115200, porta COM errada, ou o
  monitor abriu depois da partida (aperte **EN**).
- Algum `HOUVE FAIL`: anote a linha com `FAIL` (ela diz o caso) e avise o grupo.
  Não é problema de montagem: os autotestes não usam os módulos.
- `[audio] ERRO i2s_driver_install: …` ou `[audio] ERRO i2s_set_pin: …`, seguido
  de `[audio] ERRO: fonte de audio nao iniciou`: o I2S não ligou; avise o grupo.
- `[tarefa] ERRO: nao foi possivel criar a tarefa de audio`: falta memória;
  avise o grupo (`PILHA_TAREFA_AUDIO`).
- A linha `[serial] comandos:` não mostra `um nivel … ou 'auto'`: o
  `config_local.h` não foi lido ou está com `FONTE_SIMULADA 0`. Confira o nome
  do arquivo (`config_local.h`, na pasta `firmware/noisetrack/`) e faça Clean.

### (b) LEDs HW-222

**Fazer:** USB desligado, ligue o J2 do HW-222 (`IN` no `P4`, `VCC` no `5V`,
`GND` no `GND`). Religue.

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
- Nenhum LED: confira se o fio está no **`IN` do J2** (e não no `OUT` do J1),
  o `VCC`, o `GND` e o `P4`. Confira as soldas.
- Cores erradas, piscando ou só o primeiro LED: passe o `VCC` do J2 para o
  `3V3` (seção 3.4).
- Brilho forte ou fraco demais: `BRILHO_LEDS` no `config.h` (0 a 255; hoje 77,
  cerca de 30%). Essa chave não está no `config_local.h`: avise o grupo.
- Verde e vermelho trocados: o módulo usa outra ordem de cores. A ordem está no
  código (`NEO_GRB` em `leds.cpp`): avise o grupo.

### (c) Buzzer

**Fazer:** USB desligado, ligue o buzzer (`VCC` no `3V3`, `GND`, `I/O` no `P27`).
Confira `BUZZER_ATIVO_EM_BAIXO 1` no `config_local.h`. Religue.

**Serial:** não existe mensagem própria do buzzer. O buzzer acompanha o estado alerta.

**O que ver:** em silêncio no calmo e no contando. No alerta (`80` e esperar
5 s), bipes de 0,5 s ligado e 0,5 s desligado.

**Pronto quando:** silêncio na partida, bipe só no alerta.

**Se não aparecer:**
- **Apita sempre, e para justamente no alerta** (lógica ao contrário): troque
  `BUZZER_ATIVO_EM_BAIXO` para `0` no `config_local.h`.
- Apita sempre e não muda nada: confira se o fio está no `P27` e não no `VCC`, e
  se o `VCC` do buzzer está no `3V3` (no 5V ele pode não calar).
- Não apita nunca: confira o `VCC` no `3V3` e o `GND`.
- Ritmo do bipe: `BUZZER_PULSO_MS` (250) ligado e `BUZZER_PAUSA_MS` (750)
  desligado; `BUZZER_GUARDA_MS` (80) é o tempo depois do apito em que o nível
  ainda não conta. Para mudar, defina no `config_local.h`.

### (d) Um microfone

**Fazer:**
1. No `config_local.h`: `FONTE_SIMULADA 0` e `NUM_MICROFONES 1`. Com só um
   microfone ligado, o canal B fica com a linha solta e pode ler lixo.
2. USB desligado, ligue o **mic 1**: `VDD` no **3V3**, `GND`, `L/R` no `GND`,
   `SCK` no `P26`, `WS` no `P25` e `SD` no `P33`. Grave e abra o monitor.

**Serial:**
```
[audio] INMP441 por I2S, 1 microfone(s), 16000 Hz
[serial] comandos: 'limite N' | 'x N' | 'wifi' | 'wifi apagar' | 'web' | 'etiqueta'
[nivel] A = 42.3 dB | B = -- | comb = 42.3 dB
```
- Em silêncio: um valor estável (o número depende da sala e da calibração).
- Falando perto: sobe 15 a 30 dB. Batendo palmas: picos bem mais altos.
- O nível forçado não existe mais: `80` responde
  `[serial] '80' ignorado: nivel forcado so existe com FONTE_SIMULADA 1`. Para
  ver o alerta sem fazer muito barulho, use `limite 40` e fale perto do
  microfone por 5 s (depois volte com `limite 65`).

**Pronto quando:** o nível de A responde na hora a fala e palmas e volta ao normal.

**Se não aparecer:**
- `A = --` sempre e `[regra] estado=sem_sinal …`: o microfone não manda dados.
  Confira `VDD` (3V3), `GND`, `SD` no `P33` e o `L/R` no `GND`.
- A sempre preso em `20.0 dB` (o piso, `DB_PISO`): o sinal é quase zero. Mesmas
  conferências. Se só funcionar com `LOCAL_MIC_CANAIS_TROCADOS true`, o ESP32
  está lendo o canal do outro lado: deixe assim e avise o grupo.
- Números que pulam sem sentido (90, 110 dB em silêncio): fio solto, sobretudo
  `SCK` ou `WS`, ou falta o GND comum.
- `[audio] falha no i2s_read: …, … de … bytes`: o I2S parou; veja a seção 7.

### (e) Segundo microfone e ordem dos canais

**Fazer:** USB desligado, ligue o **mic 2** nos mesmos fios (`VDD`, `GND`, `SCK`,
`WS` e `SD`) e o **`L/R` dele no 3V3**. No `config_local.h`: `NUM_MICROFONES 2`. Grave.

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
- Ao bater no mic 1 sobe o **B**: `LOCAL_MIC_CANAIS_TROCADOS true` no `config_local.h`.
- B sempre `--`: o mic 2 não fala. Confira o `L/R` dele no `3V3` e o `VDD`.
- A e B iguais o tempo todo, mesmo batendo num só: os dois `L/R` estão no mesmo
  lado (os dois no `GND` ou os dois no `3V3`).
- A e B diferentes em 5 dB ou mais, em silêncio: é normal entre microfones;
  a calibração por microfone corrige (seção 5).

### (f) Sem sinal

**Fazer** (sempre com o USB desligado para tirar e pôr fios):

Tire o microfone **inteiro** (os 6 fios dele), não só o `VDD`. Um microfone sem
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
`--`, a linha `SD` solta está lendo lixo. Avise o grupo: a solução é um
resistor de 100 kΩ entre o `SD` (`P33`) e o `GND`, como recomenda o datasheet
do INMP441 (não está no material atual).

### (g) Wi-Fi pelo portal

**Fazer:**
1. Na partida sem rede salva, aparece
   `[rede] AP de setup ligado: 'NoiseTrack-Setup', portal em http://192.168.4.1/`.
2. No celular, conecte na rede **NoiseTrack-Setup** com a **Senha** da etiqueta
   (comando `etiqueta`, se não anotou).
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

- Anote o **IP** (o comando `wifi` mostra de novo). `noisetrack.local` funciona
  no iPhone, no Mac e no Windows; em muitos Android **só o IP** funciona.
- Na próxima partida, com a rede salva, aparecem `[rede] estado=conectando`,
  `[rede] tentando conectar a 'MinhaRede'` e, em seguida, o `conectado`.
- Para refazer o setup: `wifi apagar` no Serial.

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
`http://noisetrack.local/`). O comando `web` mostra o endereço e a senha de
configuração.

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

**Fazer:** limite 65 dB e X 5 s (os padrões; confira na página ou digite
`limite 65` e `x 5`).
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
  aparelho.
- **A calibração é por microfone:** cada um tem seu ajuste
  (`LOCAL_OFFSET_CAL_MIC_A_DB` e `LOCAL_OFFSET_CAL_MIC_B_DB` no `config_local.h`).

### 5.1 Modo calibração (`CALIBRACAO 1`)

Ler 8 linhas `[nivel]` por segundo e "chutar" um valor típico é difícil. Com
`CALIBRACAO 1` no `config_local.h` (e `FONTE_SIMULADA 0`), o aparelho faz a
média para você:

```
[calib] modo calibracao: media em energia a cada 5 s (sem linhas [nivel])
[calib] 5 s: A = 61.8 dB | B = 63.0 dB | comb = 62.4 dB (min 58.9, max 66.1)
[calib] 5 s: A = 62.1 dB | B = 63.2 dB | comb = 62.7 dB (min 60.2, max 65.0)
```

- As linhas `[nivel]` param; uma linha `[calib]` sai a cada 5 s com a média de
  A, B e do combinado nesses 5 s, mais o mínimo e o máximo do combinado.
- **A média é em energia, não a média dos números em dB.** Exemplo: 80 dB e
  70 dB dão **77,4 dB**, e não 75, porque 80 dB tem 10 vezes a energia de
  70 dB. É assim que um decibelímetro faz a média (Leq), então os dois ficam
  comparáveis. O autoteste confere essa conta (`[teste] 7 calib …`).
- Só entram janelas válidas. Um canal sem nenhuma janela válida nos 5 s mostra
  `sem sinal` (por exemplo `B = sem sinal` com `NUM_MICROFONES 1`).
- Se `min` e `max` estiverem muito longe um do outro (mais de uns 10 dB), o som
  variou demais nesses 5 s: espere a próxima linha.
- O alerta, os LEDs, o buzzer e a página continuam funcionando normalmente:
  só muda o que sai no Serial.

### 5.2 Como fazer

1. Decibelímetro e aparelho lado a lado, à mesma distância da fonte de som.
   No decibelímetro, use o modo **SLOW** ou o **Leq** (média), se tiver, porque
   a linha `[calib]` é uma média de 5 s.
2. Em cada nível, deixe o som estável, espere duas linhas `[calib]` e anote a
   segunda junto com o valor do decibelímetro.
3. Evite níveis abaixo de uns 35 dB: ali o INMP441 não mede bem (o ruído
   próprio dele é dessa ordem).

| Nível | Decibelímetro (dB) | A do aparelho (dB) | B do aparelho (dB) | Diferença A = medidor − A | Diferença B = medidor − B |
|---|---|---|---|---|---|
| Silêncio do ambiente | | | | | |
| Fala normal (1 m) | | | | | |
| Música | | | | | |
| Palmas contínuas | | | | | |
| **Média** | | | | | |

4. **Ajuste**, no `config_local.h`:
   `LOCAL_OFFSET_CAL_MIC_A_DB` = média das diferenças de A, e o mesmo para B.
   - Exemplo: o medidor marca 70 e o A marca 66,5, então diferença = 70 − 66,5 =
     +3,5. Se a média de A for +3,5, use `#define LOCAL_OFFSET_CAL_MIC_A_DB 3.5f`.
   - Diferença positiva: o aparelho mede baixo e o ajuste soma. Negativa: ele
     mede alto e o ajuste subtrai.
   - Fique entre **−20 e +20**. O código não confere esse limite, mas um ajuste
     maior que isso indica fiação errada, microfone com defeito ou medição mal
     feita.
   - Se as diferenças variam muito de um nível para outro (5 dB ou mais), o
     problema não é só de ajuste: confira a ponderação (A × C/Z), a distância e
     se o microfone está tapado.
5. Grave de novo e repita uma ou duas medições para conferir: a diferença deve
   ficar perto de 0 (±2 dB).
6. No fim, volte `CALIBRACAO 0`.
7. **Passe os valores para o grupo:** o `config_local.h` não vai para o Git.
   Anote os ajustes na tabela da seção 8 e avise o grupo, para decidir se entram
   no `config.h`.

> As chaves `offset_cal_a` e `offset_cal_b` que existem na memória da placa
> **ainda não são usadas** na conta. Hoje a calibração é só pelo
> `config_local.h` (ou pelo padrão do `config.h`).

---

## 6. Antes de instalar

No `config_local.h`:

- [ ] `TESTE_PREFS 0`: esse autoteste grava na flash a cada partida.
- [ ] Os outros `TESTE_*` em `0`, se quiser uma partida mais rápida
      (`TESTE_NIVEL_DB`, `TESTE_REGRA`, `TESTE_TAREFA`, `TESTE_WIFI`, `TESTE_WEB`).
- [ ] `USANDO_WOKWI 0`, `FONTE_SIMULADA 0`, `NUM_MICROFONES 2`,
      `BUZZER_ATIVO_EM_BAIXO 1`, `CALIBRACAO 0` e os ajustes de calibração preenchidos.
- [ ] `COMANDOS_SERIAL`: pode ficar `1` (só funciona com um PC no USB) ou `0`.

E também:

- [ ] `secrets.h` criado, com o segredo do grupo. `secrets.h` e `config_local.h`
      **fora** do `git status`.
- [ ] **Etiqueta:** a linha `[rede] Rede: NoiseTrack-Setup | Senha: XXXXXXXX | Config: YYYYYYYY`
      aparece na partida e com o comando `etiqueta`. Anote, imprima e cole no
      aparelho (ou entregue a Config só ao síndico). Ela muda se o `secrets.h` mudar.
- [ ] **Trocar a rede salva depois de instalado:**
  - Com um PC no USB: `wifi apagar` no Serial (funciona na placa real com
    `COMANDOS_SERIAL 1`).
  - Sem cabo: reinicie o aparelho (botão **EN**) **sem** a rede salva disponível
    (roteador desligado ou longe). Ele tenta por 15 s (`WIFI_TIMEOUT_MS`) e liga
    o portal NoiseTrack-Setup. Uma queda no meio do uso **não** abre o portal:
    só a partida sem conseguir conectar.
  - Último recurso: "Erase Flash" no PlatformIO (apaga tudo, inclusive limite e
    X, e exige gravar de novo).
  - O botão **IO0 (BOOT)** não é lido pelo firmware; o **EN** só reinicia.
  - **Sugestão para o case:** um furo para alcançar o botão **EN**.

---

## 7. Problemas comuns

| Sintoma | Causa provável | O que fazer |
|---|---|---|
| Placa não aparece na COM | Cabo só de carga; falta driver | Trocar o cabo; instalar driver CP210x ou CH340; outra porta USB. |
| Gravação para em `Connecting........_____` | A placa não entrou em modo de gravação | Segurar **IO0 (BOOT)** durante o `Connecting`. |
| Mudei o `config_local.h` e nada mudou | Arquivo com outro nome/pasta, ou build antigo | Nome exato `config_local.h` em `firmware/noisetrack/`; **Clean** e compilar. A linha `[serial] comandos:` e a `[audio] …` mostram a configuração em uso. |
| `80` responde `nivel forcado so existe com FONTE_SIMULADA 1` | Placa com microfone real | Normal. Use barulho de verdade ou `limite 40`. |
| `[audio] falha no i2s_read: …, … de … bytes` | I2S sem resposta (fiação ou pino errado) | Conferir `SCK` no `P26`, `WS` no `P25`, `SD` no `P33` e o GND comum. Depois de 5 falhas seguidas (`FALHAS_ATE_SEM_SINAL`) aparece ` [I2S sem resposta]` na linha `[regra]`, e o estado vira sem sinal. |
| Nível preso em `20.0 dB` (`DB_PISO`) ou `--` | Microfone sem alimentação ou sem dados | Conferir `VDD` no `3V3`, `L/R` ligado (nunca solto), `SD` no `P33`; trocar os microfones de lugar; testar `LOCAL_MIC_CANAIS_TROCADOS true`. |
| Canal sem microfone lê valores aleatórios | Linha `SD` solta enquanto o microfone não fala | Com um microfone só, usar `NUM_MICROFONES 1`. |
| Níveis 10 a 20 dB fora do decibelímetro | Convenção do 120/123, ponderação dB(A), distância, microfone tapado ou com defeito | Calibrar (seção 5). Se passar de 20 dB, conferir fiação e `L/R`. |
| A e B muito diferentes | Microfones de lotes diferentes, ou um tapado | Calibrar cada um; conferir se nada encosta no furo do microfone. |
| LEDs apagados | Fio no `OUT` do J1; solda ruim; falta GND comum | Fio no **`IN` do J2**; refazer a solda; GND comum. |
| LEDs piscando errado, cores estranhas, só o 1º LED | Sinal de 3,3 V fraco para `VCC` em 5V | Passar o `VCC` do J2 para o `3V3`. Verde e vermelho trocados: ordem de cores no código (`NEO_GRB` em `leds.cpp`), avisar o grupo. |
| Buzzer apita sempre e cala no alerta | Lógica ao contrário | Trocar `BUZZER_ATIVO_EM_BAIXO` no `config_local.h` (a nossa placa usa `1`). |
| Buzzer apita sempre e não muda | `VCC` do buzzer em 5V; fio `I/O` no lugar errado | `VCC` no `3V3`; `I/O` no `P27`. |
| Portal não abre sozinho no celular | Dados móveis ligados; "DNS privado" do Android | Digitar `http://192.168.4.1/`; desligar os dados móveis durante o setup; desligar o "DNS privado". |
| Página não carrega por `noisetrack.local` | Muitos Android não resolvem `.local`; celular em outra rede | Usar o IP (comando `wifi` ou `web`); conferir se o celular está no mesmo Wi-Fi (não na rede de convidados). |
| Página abre mas fica em "reserva" | Porta 81 bloqueada pela rede, ou já há 3 clientes ao vivo | Funciona mesmo assim (consulta a cada 0,4 s). Fechar abas antigas. |
| Rede salva não conecta nunca | Rede de 5 GHz, senha errada ou nome com erro | Usar 2,4 GHz; `wifi apagar` e refazer pelo portal. |

---

## 8. Registro de resultados

| Data | Passo | Resultado | Ajuste feito |
|---|---|---|---|
| | (a) Só o ESP32 | | |
| | (b) LEDs HW-222 | | |
| | (c) Buzzer | | |
| | (d) Um microfone | | |
| | (e) Segundo microfone e ordem | | |
| | (f) Sem sinal | | |
| | (g) Wi-Fi pelo portal | | |
| | (h) Página no celular | | |
| | (i) Queda do roteador | | |
| | (j) Teste do alerta | | |
| | Calibração mic A | | `LOCAL_OFFSET_CAL_MIC_A_DB = ` |
| | Calibração mic B | | `LOCAL_OFFSET_CAL_MIC_B_DB = ` |
| | Pilha livre mínima | | |
| | Etiqueta anotada | | |
