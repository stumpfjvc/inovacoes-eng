# NoiseTrack na nuvem (Supabase)

Esta pasta tem o banco de dados da parte online do NoiseTrack. O site que mostra
os dados fica em [`../docs/`](../docs/).

> **Regra de ouro:** nenhuma chave, senha ou URL real entra no Git. Neste guia,
> tudo que está em MAIÚSCULAS (`URL_DO_PROJETO`, `CHAVE_ANON`, `CHAVE_DO_APARELHO`...)
> é um **lugar para você trocar pelo valor de verdade**, só na hora de usar.

## Como as peças conversam

```
 placa ESP32 ──(aparelho_sync: ID + chave)──▶  Supabase  ◀──(login + leitura)── site (docs/)
             ◀──── limite e segundos ───────               ──(funções)──▶ parear, salvar config
```

- A **placa** se identifica com o **ID** e uma **chave secreta** dela. No banco
  fica só um "embaralhado" da chave (bcrypt), nunca a chave em texto.
- O **site** usa login por e-mail e senha. Cada pessoa só vê os aparelhos que
  pareou, porque o RLS (Row Level Security) bloqueia o resto.
- Ninguém escreve direto nas tabelas. Tudo passa pelas funções do `schema.sql`.

## 1. Criar o banco

1. Entre no painel do Supabase e abra o seu projeto.
2. Menu da esquerda: **SQL Editor** → **New query**.
3. Abra o arquivo [`schema.sql`](schema.sql), copie **tudo** e cole no editor.
4. Clique em **Run**. Deve aparecer "Success. No rows returned".
5. Confira em **Table Editor**: devem existir as tabelas `aparelhos` e `leituras`,
   as duas com o cadeado de RLS ligado.

Pode rodar o `schema.sql` de novo depois (por exemplo, se ele mudar): ele não
apaga dados.

## 2. Cadastrar um aparelho

Cada placa precisa de três coisas, escolhidas por você:

| O quê | Exemplo de formato | Onde fica |
|---|---|---|
| **ID** | `noisetrack-a1b2c3` (o mesmo que a placa mostra no comando `etiqueta`) | banco e placa |
| **Código de pareamento** | `XXXX-XXXX`, só **maiúsculas** e números | banco e etiqueta |
| **Chave do aparelho** | texto longo e aleatório (48 caracteres, por exemplo) | **só na placa** |

**Para gerar a chave**, use o Git Bash no seu PC (não precisa de internet):

```bash
openssl rand -hex 24
```

Isso imprime 48 letras e números. Essa é a `CHAVE_DO_APARELHO`.

**Para o código de pareamento**, invente 8 caracteres e evite os que se confundem
(0/O, 1/I/L). Exemplo de formato: `K7MP-3QXR`.

Agora, no **SQL Editor**, rode (trocando os três valores):

```sql
insert into public.aparelhos (id, codigo_pareamento, chave_hash)
values (
  'ID_DO_APARELHO',
  'CODIGO-PAREAMENTO',
  extensions.crypt('CHAVE_DO_APARELHO', extensions.gen_salt('bf'))
);
```

- `crypt(..., gen_salt('bf'))` guarda só o "embaralhado" da chave. Nem quem
  abre o banco consegue ver a chave original.
- O código precisa estar em **maiúsculas** no INSERT. O site converte o que a
  pessoa digitar para maiúsculas antes de procurar.

> ⚠️ **A chave do aparelho (em texto) vai SÓ na placa**, num arquivo fora do Git
> (como o `secrets.h` do firmware). **NUNCA** coloque essa chave no Git, no
> `config.js` do site, em print ou em mensagem do grupo. Se ela vazar, gere uma
> chave nova, grave na placa e troque no banco:
>
> ```sql
> update public.aparelhos
>    set chave_hash = extensions.crypt('CHAVE_NOVA', extensions.gen_salt('bf'))
>  where id = 'ID_DO_APARELHO';
> ```

Depois de rodar, **apague a chave do editor** (o SQL Editor guarda o histórico
das consultas).

## 3. Testar a função da placa com curl

Antes de mexer no firmware, dá para fingir que você é a placa. Você vai precisar
de dois valores do painel (**Project Settings** → **API**):

- `URL_DO_PROJETO`: a "Project URL", algo como `https://xxxx.supabase.co`
- `CHAVE_ANON`: a chave **anon public**

> A chave anon é pública de propósito (o site também usa). Quem protege os dados
> é o RLS. **Nunca** use a chave `service_role` na placa nem no site.

No **Git Bash** (no PowerShell, o `curl` é outro programa e as aspas mudam):

```bash
curl -X POST "URL_DO_PROJETO/rest/v1/rpc/aparelho_sync" \
  -H "apikey: CHAVE_ANON" \
  -H "Authorization: Bearer CHAVE_ANON" \
  -H "Content-Type: application/json" \
  -d '{"p_id":"ID_DO_APARELHO","p_chave":"CHAVE_DO_APARELHO","p_nivel":62.5,"p_estado":"calmo"}'
```

O que esperar:

| Situação | Resposta |
|---|---|
| ID e chave certos | `{"limite_db":65,"segundos":5}` |
| Chave ou ID errados | erro com a mensagem `aparelho_invalido` |

Rode algumas vezes, com uns 10 s entre elas, mudando o `p_nivel`. Depois, no
**Table Editor**, veja a tabela `leituras` crescer (no máximo 1 linha a cada
10 s por aparelho).

## Cadastrar e testar um aparelho com os scripts

Os passos 2 e 3 acima têm atalhos em `nuvem/ferramentas/`, para você não copiar
chaves à mão. Rode no **Git Bash**, na pasta do projeto. Tudo o que for secreto
fica em `~/.noisetrack/` (na sua pasta de usuário), **nunca** dentro do repositório.

**1. Cadastrar** (não usa a internet):

```bash
bash nuvem/ferramentas/novo_aparelho.sh noisetrack-f6d6d5
```

- Gera a chave do aparelho e o código de pareamento, e copia o SQL para a área
  de transferência. Na tela aparecem só o ID e o código.
- No Supabase: **SQL Editor** → cole → **Run** → apague a query do histórico.
- Rodar de novo com o mesmo ID **troca só a chave** (o código continua o mesmo).
  A chave anterior fica em `<id>.chave.anterior`; apague quando não precisar mais.

**2. Testar** (usa a internet). Antes, crie o arquivo `~/.noisetrack/projeto.env`
com a URL e a chave **pública** do projeto (nunca a `service_role`):

```
SUPABASE_URL=https://SEU-PROJETO.supabase.co
SUPABASE_CHAVE_PUBLICA=sua-chave-anon-ou-publishable
```

```bash
bash nuvem/ferramentas/testar_sync.sh noisetrack-f6d6d5
```

- Resposta `{"limite_db":65,"segundos":5}`: deu certo.
- `aparelho_invalido`: a chave do arquivo não confere com a do banco. Rode o
  `novo_aparelho.sh` de novo e cole o SQL.

Os scripts nunca imprimem a chave. O `projeto.env` e a pasta `~/.noisetrack/`
não vão para o Git: confira com `git status` antes de qualquer commit.

## 4. O que cada função faz

| Função | Quem chama | O que faz |
|---|---|---|
| `aparelho_sync(id, chave, nivel, estado)` | a placa (anon) | Confere a chave, guarda o nível e o estado, grava no histórico (no máximo 1 a cada 10 s) e devolve o limite e os segundos. Às vezes apaga leituras com mais de 7 dias. |
| `parear_aparelho(codigo)` | o site (logado) | Torna você dono do aparelho, se ele ainda não tiver dono. |
| `atualizar_config(id, limite, segundos)` | o site (logado) | Muda o limite (30 a 100 dB) e os segundos (1 a 60) do seu aparelho. |
| `desparear_aparelho(id)` | o site (logado) | Solta o aparelho e apaga o histórico dele, para o próximo dono não ver. |

Mensagens de erro que as funções podem devolver: `aparelho_invalido`,
`codigo_invalido`, `nao_autenticado`, `limite_invalido`, `segundos_invalido`,
`aparelho_nao_encontrado`. O site traduz cada uma para uma frase amigável.

## O firmware na nuvem

O firmware (`firmware/noisetrack/nuvem.cpp`) chama a `aparelho_sync` a cada 1 s
(`NUVEM_INTERVALO_MS`, para demonstração; use 10 s no dia a dia), com o Wi-Fi do
local conectado. O histórico guarda no máximo 1 leitura a cada 10 s de qualquer
jeito (regra da função, no servidor). O passo a passo para ligar, o custo de
tráfego e como voltar para 10 s estão na etapa (k) do
`firmware/noisetrack/GUIA_BANCADA.md`. Resumo:

- A **URL** e a chave **pública** vão no `config_local.h` (`NUVEM_URL` e
  `NUVEM_CHAVE_PUBLICA`); a **chave do aparelho** vai no `secrets.h`
  (`NUVEM_CHAVE_APARELHO`). Os dois arquivos ficam fora do Git.
- **A nuvem manda:** se o limite/segundos do banco for diferente do aparelho,
  vale o do banco, e o aparelho grava na memória dele.
- O alarme local não depende da nuvem: sem internet, ele funciona igual, e o
  aparelho tenta de novo com espera crescente (com 1 s: 1, 2, 4, 8... até 60 s).

### Conexão verificada (TLS)

> **A verificação está ligada por padrão** (`NUVEM_TLS_VALIDAR 1`). Ela já
> reiniciou a placa pelo watchdog; foi corrigida (a tarefa da nuvem roda em
> prioridade 0) e validada na placa: sem reinício, 49 de 49 envios, maior tempo
> sem a tarefa ociosa de 8 ms. **A primeira conexão leva cerca de 10 s** (o
> aperto de mão verificado é pesado), e o site só mostra a placa **Online depois
> disso**; as seguintes reaproveitam a conexão. O modo sem verificação
> (`#define NUVEM_TLS_VALIDAR 0` no `config_local.h`) é só para diagnóstico.
> Detalhes: etapa (k) do `firmware/noisetrack/GUIA_BANCADA.md`.

O firmware usa HTTPS **com verificação**. Antes de mandar qualquer dado, ele
confere:

1. **A cadeia:** o certificado do servidor chega numa das raízes guardadas em
   `firmware/noisetrack/nuvem_ca.h` (ISRG Root X1 e X2, do Let's Encrypt, e GTS
   Root R1 e R4, do Google Trust Services: as autoridades que a Cloudflare, na
   frente do Supabase, costuma usar).
2. **O nome:** o certificado é do mesmo endereço do `NUVEM_URL`.
3. **A data:** cada certificado está na validade. O mbedTLS do ESP32 não olha a
   data sozinho, então o firmware confere, e só depois do NTP acertar o relógio.

**O que isso protege:** sem a conferência, alguém que controle a rede do local
poderia se passar pelo Supabase, ver a chave do aparelho e mandar um limite
falso. Com ela, a placa só conversa com o servidor certo.

**Se falhar:** a nuvem não envia (nunca troca sozinha para o modo sem
verificação), o Serial explica o motivo (`[nuvem] certificado recusado: ...`) e
o comando `nuvem` mostra o último erro de certificado. O alarme local segue
normal.

**Atualizar a raiz** (se o provedor mudar de autoridade): instruções no começo
do `nuvem_ca.h`. O certificado do site vence a cada ~90 dias, mas a placa só
guarda as raízes, que duram anos (a primeira das quatro vence em 2035).

## Ainda não feito

- A raiz que o servidor usa hoje não foi confirmada de um PC "limpo": no PC do
  teste, o antivírus Norton intercepta o HTTPS e mostra a cadeia dele. As 4
  raízes cobrem as autoridades da Cloudflare; a confirmação é a placa conectar
  (`ultimo resultado: ok`).
- Não há limite de tentativas no pareamento. Com códigos de 8 caracteres
  aleatórios, adivinhar é muito difícil, mas evite códigos óbvios.
