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
- Para a **nuvem**, o `secrets.h` também guarda a chave secreta desta placa,
  `NUVEM_CHAVE_APARELHO`. Ela é diferente em cada placa: veja a etapa (k).

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
| `NUVEM_URL` / `NUVEM_CHAVE_PUBLICA` | modelos `COLE_AQUI_...` | Para usar a nuvem: a URL e a chave **pública** do projeto (etapa k). |
| `NUVEM_INTERVALO_MS` | `1000` (não precisa estar no arquivo) | Tempo entre envios à nuvem. `1000` = quase em tempo real (demonstração); **`10000` para uso normal**, com um décimo do tráfego (etapa k). |
| `TESTE_NIVEL_DB`, `TESTE_REGRA`, `TESTE_TAREFA`, `TESTE_PREFS`, `TESTE_WIFI`, `TESTE_WEB`, `TESTE_NUVEM` | `1` | Ao instalar: `TESTE_PREFS 0` (seção 6). |

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
| `etiqueta` | Mostra a etiqueta: senha do AP de setup, senha Config, texto do QR de Wi-Fi, endereços do portal e da página, e o ID | Placa real e simulação |
| `wifi` | Estado da rede, IP e horário | Placa real e simulação |
| `wifi apagar` | Apaga a rede salva (o AP de setup liga) | Placa real e simulação |
| `web` | Endereço da página, clientes e senha de configuração | Placa real e simulação |
| `nuvem` | Se a nuvem está ligada, o último resultado, há quanto tempo foi o último sucesso e o limite/X recebido (etapa k) | Placa real e simulação |
| `limite 70` | Muda e grava o limite (30 a 100 dB) | Placa real e simulação |
| `x 8` | Muda e grava o tempo X (1 a 60 s) | Placa real e simulação |
| `80` / `auto` | Força o nível simulado / volta ao automático | **Só com `FONTE_SIMULADA 1`** |

Na partida aparece a lista do que existe na configuração gravada:

```
[serial] comandos: um nivel (20 a 120 dB) ou 'auto' | 'limite N' | 'x N' | 'wifi' | 'wifi apagar' | 'web' | 'etiqueta' | 'nuvem'   <- FONTE_SIMULADA 1
[serial] comandos: 'limite N' | 'x N' | 'wifi' | 'wifi apagar' | 'web' | 'etiqueta' | 'nuvem'                                        <- FONTE_SIMULADA 0
[serial] comandos desligados (COMANDOS_SERIAL 0)
```

**Com o microfone de verdade (`FONTE_SIMULADA 0`) o nível forçado não existe.**
Se digitar `80`, aparece
`[serial] '80' ignorado: nivel forcado so existe com FONTE_SIMULADA 1`. Para
provocar alerta, faça barulho de verdade ou baixe o limite (`limite 40`).

Respostas típicas:

```
[rede] Rede: NoiseTrack-Setup | Senha: XXXXXXXX | Config: YYYYYYYY      <- etiqueta
[etiqueta] QR Wi-Fi: WIFI:T:WPA;S:NoiseTrack-Setup;P:XXXXXXXX;;          <- (cole num gerador de QR)
[etiqueta] Portal: http://192.168.4.1/
[etiqueta] Pagina: http://noisetrack.local/ (ou o IP que o roteador der)
[etiqueta] ID: noisetrack-a1b2c3
[prefs] limite = 70 aceito, gravado                                       <- limite 70
[prefs] limite 200 recusado (fora da faixa)                               <- limite 200
[prefs] 'limite abc' ignorado: use 'limite 30' a 'limite 100'             <- texto inválido nunca vira 0
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

### (k) Nuvem: mandar o nível para o site

**O que é:** com o Wi-Fi do local conectado, a cada 1 s (`NUVEM_INTERVALO_MS`)
o aparelho manda para o banco no Supabase o **maior nível (dB)** que o microfone
mediu desde o envio anterior e o **estado de agora** (calmo, contando, alerta ou
sem sinal), e o site (`docs/`) mostra. Na volta, o banco devolve o limite e o X.

- **Por que o maior:** o microfone mede a cada 0,125 s e o envio é a cada 1 s.
  Mandar só a última medida perderia um pico curto (uma palma). O apito do
  próprio buzzer não conta como pico: a medição já o descarta.
- **A conexão fica aberta entre os envios** (keep-alive). Só o primeiro envio
  paga o aperto de mão do HTTPS (1 a 3 s); os seguintes levam uma fração de
  segundo. Se a conexão cair, o aparelho reconecta sozinho. Há **um envio de cada
  vez**: se um demorar (timeout curto: 4 s para conectar, 2 s para a resposta),
  os ciclos perdidos são pulados em vez de acumular.
- Com falhas, a espera cresce (1, 2, 4, 8... até 60 s) e volta a 1 s no primeiro
  envio que der certo.

> **A nuvem manda.** Se o limite/X que vier de lá for diferente do que está no
> aparelho, vale o de lá, e ele é gravado na memória. Por isso, com a nuvem
> ligada, uma mudança feita na página local (ou com `limite N` no Serial) é
> desfeita em cerca de 1 s. Para mudar de vez, mude no site.

> **O alarme local não depende da nuvem.** A nuvem roda numa tarefa separada,
> de prioridade mais baixa e em outro núcleo do processador. Sem internet, com
> o servidor fora do ar ou com a chave errada, o microfone, a regra, os LEDs e
> o buzzer funcionam igual.

**Fazer (uma vez por placa):**

1. **Descubra o ID da placa:** grave o firmware, abra o monitor e digite
   `etiqueta`. A linha `[etiqueta] ID: noisetrack-xxxxxx` mostra o ID.
2. **Cadastre a placa no banco** (no Git Bash, na pasta do projeto):
   `bash nuvem/ferramentas/novo_aparelho.sh noisetrack-xxxxxx`.
   O script copia um SQL para a área de transferência: no Supabase, abra o
   **SQL Editor**, cole, clique em **Run** e apague a query do histórico.
   Anote o **código de pareamento** que aparece na tela (vai na etiqueta).
3. **Cole a chave do aparelho no `secrets.h`:** abra o arquivo
   `C:\Users\<voce>\.noisetrack\noisetrack-xxxxxx.chave` no Bloco de Notas,
   copie a linha e acrescente no `secrets.h`:
   ```c
   #define NUVEM_CHAVE_APARELHO "cole-aqui-a-chave-de-48-caracteres"
   ```
   Essa chave é **secreta**: não mande em mensagem, print nem Git.
4. **Coloque a URL e a chave pública no `config_local.h`** (os mesmos valores
   do `docs/config.js` do site; painel do Supabase → Project Settings → API):
   ```c
   #define NUVEM_URL           "https://SEU-PROJETO.supabase.co"
   #define NUVEM_CHAVE_PUBLICA "sb_publishable_..."
   ```
   Use a chave **publishable** (ou "anon"). **Nunca** a `service_role`/secret:
   o firmware recusa e avisa.
5. Compile e grave. O Wi-Fi do local precisa estar configurado (etapa g).

> **Padrão: `NUVEM_TLS_VALIDAR 1`** (TLS verificado). Ele já reiniciou a placa
> (watchdog); foi corrigido e validado na placa (sem reinício, 49 de 49 envios,
> maior tempo sem a ociosa de 8 ms). **A primeira conexão leva cerca de 10 s**:
> a nuvem espera o relógio (NTP) e depois faz o aperto de mão verificado, e o
> site só mostra a placa **Online depois disso**. Daí em diante a conexão é
> reaproveitada. Detalhes em "Reinício pelo watchdog com TLS verificado", abaixo.

**Serial na partida:**
```
[nuvem] ligada: SEU-PROJETO.supabase.co | ID noisetrack-xxxxxx | chave publica sb_publishab...(46 caracteres) | a cada 1000 ms com o Wi-Fi conectado
[nuvem] tarefa: nucleo 0, prioridade 0 (a mesma da tarefa ociosa: elas se revezam e o watchdog de 5 s continua ligado) | limite do aperto de mao TLS: 20000 ms
[nuvem] TLS verificado: cadeia ate uma das 4 raizes do nuvem_ca.h, nome do servidor e data (so depois do NTP). Se nao conferir, nao envia.
```
(Com `#define NUVEM_TLS_VALIDAR 0` no `config_local.h`, só para diagnóstico, no
lugar da última linha aparece `[nuvem] AVISO: HTTPS SEM verificacao do
certificado`.) A nuvem **espera o relógio** (o certificado depende da data)
antes de conectar:
```
[nuvem] aguardando o horario do NTP para conferir o certificado (o alarme local funciona normalmente)
[ntp] horario acertado: 24/11/2026 14:03:22 (UTC-3)
[nuvem] horario acertado: conectando com TLS verificado
[nuvem] ler as 4 raizes do nuvem_ca.h (a biblioteca refaz isso a cada conexao nova): NN ms
[nuvem] conexao nova (ok): DNS NN ms | TCP+TLS NNNN ms (a tarefa ociosa rodou ~NNNN ms, NN%) | datas N ms | maior tempo sem a tarefa ociosa rodar: N ms (watchdog: 5000) | pilha livre minima NNNN bytes
[nuvem] ok: enviado e recebido limite 65 dB, X 5 s | este envio levou NNNN ms (com a conexao, se foi nova) | de novo a cada 1000 ms
```
A linha `conexao nova` sai **a cada conexão aberta** (a primeira e as seguintes, se
o Wi-Fi ou o servidor derrubarem). Os envios seguintes que dão certo **não**
imprimem nada (para não encher o Serial). Só aparecem as falhas e a volta ao
normal.

**Comando `nuvem`** (digite no monitor):
```
[nuvem] ligada | ID noisetrack-xxxxxx | servidor SEU-PROJETO.supabase.co | TLS verificado (cadeia + nome + data)
[nuvem] horario (NTP): acertado | ultimo erro de certificado: nenhum
[nuvem] Wi-Fi do local: conectado | ultimo resultado: ok (HTTP 200) ha 3 s
[nuvem] ultimo sucesso: ha 0 s | espera atual: 1.0 s | tentativas 120, sucessos 120
[nuvem] conexoes TLS abertas: 1 (com keep-alive fica perto de 1) | ultimo envio levou 240 ms | ciclos pulados: 0 | intervalo 1000 ms
[nuvem] primeira conexao (ok): DNS NN ms | TCP+TLS NNNN ms (a tarefa ociosa rodou ~NNNN ms, NN%) | datas N ms | maior tempo sem a tarefa ociosa rodar: N ms (watchdog: 5000) | pilha livre minima NNNN bytes
[nuvem] primeiro envio que deu certo: NNNN ms | pior tempo sem a tarefa ociosa rodar: N ms de 5000 (watchdog) | tarefa da nuvem: nucleo 0, prioridade 0
[nuvem] configuracao recebida: limite 65 dB, X 5 s | em uso: limite 65 dB, X 5 s
[nuvem] pilha livre minima da tarefa: ... de 12288 bytes | memoria livre ... (minima ...)
```
(Com `NUVEM_TLS_VALIDAR 0` os números de TCP+TLS são menores e a linha
`ler as N raizes` não aparece. Se houve mais de uma conexão, aparece também a
linha `ultima conexao`.)

Como ler as linhas de medida:
- **TCP+TLS:** o tempo total do aperto de mão, em ms. É o número a comparar entre
  `NUVEM_TLS_VALIDAR 0` e `1`.
- **a tarefa ociosa rodou ~X ms (Y%):** a tarefa da nuvem e a ociosa dividem o
  núcleo 0. Perto de **metade** significa que o revezamento está funcionando.
  **Perto de 0%** significa que a ociosa não está recebendo CPU: o watchdog corre
  risco (confira a prioridade da tarefa da nuvem, que tem que ser 0).
- **maior tempo sem a tarefa ociosa rodar:** é a folga do watchdog. O reinício
  acontece em **5000 ms**. O esperado, com a prioridade 0, é poucos ms. Acima de
  2500 ms o firmware imprime um `AVISO`.

Como ler a linha de conexões:
- **conexoes TLS abertas:** com tudo certo fica em **1** (ou poucas, se o Wi-Fi
  ou o servidor caíram). Se **subir uma por segundo**, a conexão não está sendo
  reaproveitada: o servidor está fechando a cada resposta.
- **ultimo envio levou:** algumas centenas de ms. Perto de 2000 ms é a resposta
  demorando (o limite é 2 s).
- **ciclos pulados:** deve ficar em 0 ou crescer devagar. Se crescer rápido, a
  internet está lenta demais para 1 s: use 10 s (abaixo).
- **memoria livre (minima):** a conexão aberta segura uma boa parte da memória
  o tempo todo. Anote a **mínima** depois de alguns minutos (com a página aberta
  em algum celular também) e compare se ela se mantém estável.

**Conferir no site:**
1. Abra o site (a página do `docs/`), entre com seu e-mail e senha.
2. Digite o **código de pareamento** do passo 2. A placa aparece na lista.
3. O nível e o estado mudam a cada ~1 s (bata palmas ou fale perto). O selo
   fica **Online** enquanto o último envio tiver menos de 10 s.
4. Mude o limite no site (por exemplo, 70). Em cerca de 1 s o Serial mostra
   `[nuvem] configuracao da nuvem aplicada: limite 65 -> 70 dB, X 5 -> 5 s (gravada)`
   e o comando `nuvem` mostra `em uso: limite 70 dB`.

**Pronto quando:** o comando `nuvem` mostra `ultimo resultado: ok (HTTP 200)` e o
site mostra o nível da placa mudando.

**Se não aparecer:**
- `[nuvem] desligada: NUVEM_URL ainda e o modelo ...` (ou `NUVEM_CHAVE_PUBLICA`,
  ou `NUVEM_CHAVE_APARELHO ausente ...`): falta preencher o passo 3 ou 4. Se já
  preencheu, faça **Clean** e compile de novo.
- `[nuvem] desligada: NUVEM_CHAVE_PUBLICA e a chave de ADMINISTRADOR ...`: você
  colou a `service_role`/secret. Troque pela publishable e, por segurança, gere
  uma nova secreta no painel.
- `[nuvem] falha: aparelho_invalido (HTTP 400)`: o banco não reconhece a chave.
  Confira se a linha do `secrets.h` é a mesma do arquivo `.chave` e se o SQL do
  passo 2 foi rodado. Se rodou o `novo_aparelho.sh` de novo, a chave mudou: cole
  a nova no `secrets.h` e grave.
- `[nuvem] falha: sem conexao | proxima tentativa em 2.0 s`: sem internet no
  local, ou a URL está errada. As tentativas ficam mais espaçadas (1, 2, 4, 8,
  16, 32, até 60 s) e voltam a cada 1 s no primeiro sucesso.
- `ultimo resultado: ainda nao tentou`: o Wi-Fi do local ainda não conectou
  (no portal do AP de setup a nuvem não tenta), ou o relógio ainda não acertou.
- `[nuvem] aguardando o horario do NTP ...` sem nunca aparecer
  `[ntp] horario acertado`, e depois `[nuvem] ainda sem horario do NTP ha 60 s`:
  a rede do local não deixa sair para a internet (ou bloqueia o NTP, porta UDP
  123). A nuvem fica esperando e conecta sozinha quando o relógio acertar.
- `[nuvem] certificado recusado: cadeia ou nome do servidor nao conferem (X509 - ...)`
  e `falha: certificado recusado`: o certificado do servidor não chega em
  nenhuma das raízes do `nuvem_ca.h`, ou o nome não bate com o `NUVEM_URL`.
  Confira a URL; se estiver certa, o provedor pode ter trocado de autoridade:
  veja "Atualizar o certificado raiz" abaixo. **A nuvem não envia** nesse caso
  (não cai para o modo sem verificação), mas o alarme local segue normal.
- A placa **reinicia** com `Task watchdog got triggered ... IDLE0 (CPU 0) ...
  CPU 0: nuvem`: a tarefa da nuvem monopolizou o núcleo 0 (aperto de mão TLS
  verificado). Confira se a prioridade da tarefa da nuvem é **0** (a linha
  `[nuvem] tarefa: nucleo 0, prioridade 0` na partida) e leia "Reinício pelo
  watchdog com TLS verificado". Para voltar a uma placa estável na hora, apague o
  `#define NUVEM_TLS_VALIDAR 1` do `config_local.h`.
- `[nuvem] certificado recusado: CN=...: vencido (...)` ou `ainda nao valido`:
  a data do certificado não confere com o relógio. "Vencido" é raro (o servidor
  troca o certificado antes); "ainda não válido" quase sempre é relógio errado.

#### Tempo real (1 s): custo de tráfego e como voltar para 10 s

O padrão de **1 s** existe para **demonstração com um aparelho só**. Mandar dez
vezes mais seguido custa dez vezes mais tráfego. Estimativa pelo tamanho dos
pacotes (**calculada, não medida**; o tráfego real depende do Supabase e do
HTTPS):

- **Cada envio da placa** (pedido + resposta, já com o HTTPS): cerca de
  **1 a 1,5 KB**. Os cabeçalhos da resposta do Supabase são a maior parte; o
  dado em si tem pouco mais de 100 bytes.
- **Cada aba do site aberta** (atualiza o cartão a cada 1 s e o gráfico a cada
  10 s): cerca de **2 KB por segundo** recebidos. Aba em segundo plano pausa.

| Quem | A cada 1 s | A cada 10 s |
|---|---|---|
| Placa ligada o dia todo | ~1,2 KB/s: **~4 MB por hora, ~100 MB por dia, ~3 GB por mês** | ~0,12 KB/s: ~0,4 MB por hora, ~10 MB por dia, ~0,3 GB por mês |
| Cada aba do site aberta | ~7 MB por hora (com o site também em 1 s) | (ver o passo 2 abaixo) |

O plano gratuito do Supabase tem um **limite mensal de tráfego de saída**.
Confira o número atual em **Settings → Usage**, no painel: **uma placa em 1 s o
mês inteiro já usa uma parte grande dele**, e abas deixadas abertas somam por
cima. Por isso: **use 1 s só durante a demonstração e volte para 10 s depois.**

**Como voltar para 10 s** (dois lugares, os dois precisam mudar):

1. **Firmware.** No `config_local.h` da placa, acrescente
   `#define NUVEM_INTERVALO_MS 10000` e grave de novo. Não mude o `config.h`.
2. **Site.** No `docs/app.js`, troque `ONLINE_MS` para `30 * 1000` (senão a
   placa aparece **Offline** entre um envio e outro, já que envia a cada 10 s)
   e, se quiser, `ATUALIZAR_MS` para `3 * 1000`. Depois publique o site de novo.

O que **não** muda entre 1 s e 10 s:
- **O histórico** (o gráfico dos últimos 30 minutos) continua com no máximo
  **1 ponto a cada 10 s**: essa regra é do banco (`aparelho_sync`), e o
  `schema.sql` não foi alterado.
- **O alarme local** (LED e buzzer) nunca depende da nuvem.

O que **muda** de significado com 10 s: o nível enviado é o **maior das janelas
dos últimos 10 s**, não o do instante. O cartão e o histórico passam a mostrar o
pico de cada 10 s (um som curto e alto aparece, mas o valor fica um pouco acima
da média do período).

Dois detalhes do site com 10 s de "Online": ele compara a hora do último envio
(do servidor) com a **hora do aparelho em que o site está aberto**. Se o relógio
do computador ou celular estiver **adiantado em mais de uns 10 s**, a placa pode
aparecer Offline mesmo enviando. Relógios acertados pela internet (o normal) não
dão problema.

#### TLS verificado: o que protege e como manter

**O que protege.** O HTTPS faz duas coisas: **embaralha** a conversa (ninguém
na rede lê a chave do aparelho) e, com a verificação ligada, **confere quem
está do outro lado**. Sem a conferência, alguém que controle a rede do local
(um roteador mal-intencionado, por exemplo) poderia se passar pelo Supabase,
ver a chave do aparelho e mandar um limite/X falso. Com `NUVEM_TLS_VALIDAR 1`
(o padrão), o aparelho confere três coisas antes de mandar qualquer dado:
1. **A cadeia:** o certificado do servidor foi assinado (direto ou por um
   intermediário) por uma das **raízes** guardadas no `nuvem_ca.h`.
2. **O nome:** o certificado é mesmo de `SEU-PROJETO.supabase.co`.
3. **A data:** cada certificado está dentro da validade. Por isso a nuvem só
   conecta depois do `[ntp] horario acertado`.

Se qualquer uma falhar, a nuvem **não envia** (nunca troca sozinha para o modo
sem verificação) e tenta de novo mais tarde; o alarme local não muda nada.

**Por que 4 raízes.** O Supabase fica atrás da Cloudflare, que usa
certificados do Let's Encrypt (raízes ISRG X1 e X2) ou do Google Trust Services
(raízes GTS R1 e R4) e pode trocar entre eles na renovação. Com as 4, essa
troca não derruba a nuvem. Custo: cerca de 8 KB de flash (82,7% → 83,3%).

**Atualizar o certificado raiz** (só se aparecer `certificado recusado` com a
URL certa): siga o passo a passo no começo do `nuvem_ca.h`. Resumo: descubra a
raiz atual pelo cadeado do navegador (Certificado → Detalhes → a linha de cima
→ Exportar em "Base64 (PEM)"), cole o texto no bloco do `nuvem_ca.h` junto dos
outros, compile e grave. Faça isso num PC **sem antivírus que inspecione
HTTPS**: o Norton, por exemplo, troca a cadeia pela dele ("Norton Web/Mail
Shield Root"), e essa raiz não serve para a placa.

**E se vencer?** O certificado do **site** vence a cada ~90 dias, mas o
servidor troca sozinho antes; a placa não precisa de nada, porque guarda só as
**raízes**. As raízes duram muito mais (a primeira das 4 vence em 2035). Quando
uma raiz estiver para vencer, o provedor passa a usar outra antes; se a placa
não tiver a nova, aparece `certificado recusado` e é só atualizar como acima.

**Modo sem verificação (`NUVEM_TLS_VALIDAR 0`).** Só para diagnóstico: ponha no
`config_local.h`. A conversa é criptografada, mas o aparelho não confere quem
está do outro lado, e o Serial avisa
`[nuvem] AVISO: HTTPS SEM verificacao do certificado`. Não deixe assim no
aparelho instalado.

#### Reinício pelo watchdog com TLS verificado (corrigido e validado)

**Resultado na placa (com a correção):** sem reinício, **49 de 49 envios**,
**maior tempo sem a tarefa ociosa de 8 ms** (o watchdog reinicia em 5000 ms) e
**primeira conexão de cerca de 10 s**. Por isso o padrão voltou a ser 1. Esses
10 s são o aperto de mão verificado dividindo o núcleo com a tarefa ociosa; **o
site só mostra a placa Online depois da primeira conexão**.

**O que aconteceu.** Com `NUVEM_TLS_VALIDAR 1`, uns segundos depois de
`[nuvem] horario acertado: conectando com TLS verificado`, a placa reiniciava,
com `E task_wdt: Task watchdog got triggered ... IDLE0 (CPU 0) ... CPU 0: nuvem`.
Com `NUVEM_TLS_VALIDAR 0` não acontecia.

**A causa, em palavras simples.**
- O ESP32 tem um "cão de guarda" (watchdog). Se a **tarefa ociosa do núcleo 0**
  (`IDLE0`: a que roda quando não há mais nada para fazer) ficar mais de **5 s**
  sem rodar, ele reinicia a placa. No SDK desta placa está assim: 5 s, reinicia,
  e só o núcleo 0 é vigiado.
- A tarefa da nuvem rodava no núcleo 0 com prioridade 1, **acima** da ociosa
  (prioridade 0). Uma tarefa de prioridade maior que está sempre ocupada **não
  deixa** a ociosa rodar.
- O aperto de mão TLS **com verificação** valida a cadeia de certificados com
  contas de curva elíptica feitas por software. A biblioteca faz isso **dentro de
  uma chamada só**, sem devolver a CPU no meio: ela só dá a vez entre uma etapa e
  outra do aperto de mão, e a conta pesada fica dentro de uma etapa. Sem
  verificação, essa validação não acontece e o aperto de mão é bem mais curto.
- O `CPU 0: nuvem` do log mostra exatamente isso: quem estava rodando no núcleo 0
  quando o watchdog disparou era a tarefa da nuvem.

**O que está confirmado e o que não está.** Confirmado: o log (watchdog da
`IDLE0`, com a nuvem rodando); o `sdkconfig` do SDK da placa (5 s, reinicia, só
o núcleo 0 vigiado, bibliotecas de criptografia compiladas para tamanho e não
para velocidade); o código da biblioteca `WiFiClientSecure` desta versão (relê
as raízes a cada conexão nova e só cede CPU entre etapas do aperto de mão).
**Não confirmado:** quanto tempo o aperto de mão realmente leva. Não havia placa
para medir, então isso é dedução. Por isso o firmware agora **mede e imprime**
(linhas `conexao nova`, lidas acima), para a próxima rodada na bancada dar o
número de verdade.

**As opções consideradas** e o efeito de cada uma:

| Opção | Áudio, regra, LED e buzzer | Wi-Fi | Resultado |
|---|---|---|---|
| **1. Reduzir o custo**: guardar só 1 raiz em vez de 4, ou não reler os certificados | nenhum | nenhum | **Não resolve.** O gasto grande são as assinaturas, iguais com 1 ou 4 raízes; ler as raízes é só interpretar texto (o firmware agora mede isso: `ler as N raizes`). Com 1 raiz só, uma troca de autoridade (a Cloudflare alterna entre Let's Encrypt e Google) derruba a nuvem até regravar a placa. E a biblioteca só aceita o texto das raízes, não dá para entregar já interpretadas. **Não adotada.** |
| **2. Tirar a `IDLE0` do watchdog** (`disableCore0WDT`) | nenhum (ficam no núcleo 1, cuja ociosa nem é vigiada) | funciona igual, mas **perde a rede de segurança**: se o Wi-Fi ou outra tarefa do núcleo 0 travar de verdade, a placa não reinicia sozinha | Funciona, mas é o remédio mais caro. Desligar só durante a conexão (e religar depois) tem uma corrida: ao religar, a ociosa precisa rodar antes da próxima conferência do cronômetro, que pode cair poucos ms depois. Rara, mas possível, e daria o reinício que queremos evitar. **Não adotada.** |
| **3. Aumentar o tempo do watchdog** (por exemplo 30 s) | nenhum | igual | Só empurra o limite: o tempo do aperto de mão depende da CPU e do certificado do servidor, e não há garantia de que caiba. E um travamento de verdade demora 30 s para reiniciar. **Não adotada como solução.** |
| **4. Mover a nuvem para o núcleo 1** | áudio/regra/LED/buzzer têm prioridade 2 e interrompem a nuvem, então funcionam; mas **deixa de existir o núcleo só do alarme** | melhora (sai do núcleo do Wi-Fi) | A ociosa do núcleo 1 não é vigiada, então o watchdog não dispararia. Mas a nuvem passaria a dividir o núcleo 1 com o `loop()` (mesma prioridade, revezam a cada 1 ms, e o `loop()` quase nunca espera): a página, o WebSocket, o Serial e a máquina do Wi-Fi ficariam na metade da velocidade durante os segundos do aperto de mão. **Não adotada.** |
| **5. Prioridade 0 no núcleo 0** | **nenhum efeito**: ficam em outro núcleo; só dividem uma trava, segurada por microssegundos | **nenhum efeito ruim**: as tarefas do Wi-Fi têm prioridade muito maior e interrompem a nuvem na hora | **Adotada.** |

**O que foi feito (opção 5).** A tarefa da nuvem agora tem **prioridade 0**, a
mesma da tarefa ociosa, e continua no núcleo 0. O escalonador reveza tarefas de
mesma prioridade a cada 1 ms, então, mesmo com o aperto de mão ocupado, a ociosa
roda a cada ~2 ms e alimenta o watchdog. **O watchdog continua ligado**,
vigiando o núcleo 0 como antes. O preço: o aperto de mão pode levar **até o
dobro** do tempo de relógio, porque a nuvem passa a dividir o núcleo meio a meio
com a ociosa. Por isso o limite do aperto de mão subiu de 4 s para **20 s**
(`NUVEM_TIMEOUT_TLS_MS`; a biblioteca só confere esse limite entre etapas, então
um limite curto demais derrubaria uma conexão prestes a dar certo). Um
`static_assert` no `config.h` **impede compilar** com a nuvem no núcleo 0 e
prioridade acima de 0, para ninguém desfazer isso sem querer.

**O que dá e o que não dá para garantir.**
- O mecanismo que reiniciou a placa (a ociosa sem rodar por mais de 5 s) deixa de
  existir **por construção**: a nuvem não consegue mais monopolizar o núcleo 0.
- O que **não** dá para garantir sem rodar na placa: que não exista outra causa
  de reinício. Por exemplo, uma queda de tensão (brownout) por causa do pico de
  CPU junto com o rádio Wi-Fi, numa fonte USB fraca, não depende do software. E
  uma falha de programação só aparece rodando.
- Por isso o padrão ficou em 0 até a validação abaixo. Ela passou na placa (ver o
  resultado no começo desta seção) e o padrão agora é **1**. Um reinício a cada
  conexão derrubaria o alarme, que é o coração do aparelho: se um dia isso
  voltar, apague o `NUVEM_TLS_VALIDAR` do `config_local.h` (ou ponha 0) para
  recuperar a placa na hora e mande o log.

**Como validar na placa** (foi o roteiro seguido; repita se mudar a prioridade, o
SDK ou as raízes):
1. No `config_local.h`, acrescente `#define NUVEM_TLS_VALIDAR 1`, grave e abra o
   monitor. Espere `[ntp] horario acertado` e a linha `conexao nova (ok)`.
2. Confira os números dessa linha:
   - **maior tempo sem a tarefa ociosa rodar:** bem abaixo de 5000 ms (o esperado
     são poucos ms). Se aparecer o `AVISO` (2500 ms ou mais), o revezamento não
     está funcionando como previsto: pare e mande o log.
   - **a tarefa ociosa rodou ~X ms (Y%):** perto de metade confirma o revezamento.
   - **TCP+TLS** na primeira conexão e nas seguintes, e **`ler as N raizes`**:
     anote, são os números que faltavam para decidir sobre reduzir as raízes.
3. Deixe rodando por uns 30 minutos: **nenhum reinício** (nenhum `Rebooting...`) e
   `conexoes TLS abertas` estável.
4. **Teste de queda:** desligue o roteador (ou o hotspot do celular) por 1 minuto
   e religue, umas 5 vezes. A nuvem deve reconectar (cada conexão nova imprime a
   linha de medidas). **Faça barulho acima do limite logo depois de religar**, no
   meio do aperto de mão: o LED e o buzzer têm que disparar normalmente.
5. Falhou em algo? Ponha `#define NUVEM_TLS_VALIDAR 0` no `config_local.h` para
   estabilizar a placa e mande o log.

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
