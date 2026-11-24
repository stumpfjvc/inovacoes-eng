#!/usr/bin/env bash
# NoiseTrack: cadastra um aparelho novo (ou troca a chave de um que já existe).
#
# Uso:  bash nuvem/ferramentas/novo_aparelho.sh <id>
# Ex.:  bash nuvem/ferramentas/novo_aparelho.sh noisetrack-f6d6d5
#
# O que faz, sem usar a internet:
#   1. gera uma chave secreta (48 caracteres hexadecimais) e um código de
#      pareamento (XXXX-XXXX);
#   2. guarda a chave em ~/.noisetrack/<id>.chave (fora do repositório);
#   3. escreve ~/.noisetrack/<id>.sql com o INSERT para o Supabase;
#   4. copia esse SQL para a área de transferência.
#
# Na tela aparecem só o ID e o código de pareamento. A chave e o SQL
# (que contém a chave) NUNCA são impressos.

set -euo pipefail

# Tudo o que for criado aqui fica legível só pelo dono (arquivos 600, pasta 700).
umask 077

# ---------- Argumento ----------
uso() {
  echo "Uso: bash nuvem/ferramentas/novo_aparelho.sh <id>"
  echo "Ex.: bash nuvem/ferramentas/novo_aparelho.sh noisetrack-f6d6d5"
}

if [ "$#" -ne 1 ] || [ -z "$1" ]; then
  uso
  exit 1
fi
ID="$1"

# O ID vai para um nome de arquivo e para dentro do SQL. Só aceitamos letras,
# números, hífen e sublinhado: isso evita aspas no SQL e "../" no caminho.
if ! [[ "$ID" =~ ^[A-Za-z0-9_-]{1,64}$ ]]; then
  echo "ID inválido: use só letras, números, '-' e '_' (até 64 caracteres)."
  uso
  exit 1
fi

# ---------- Pasta de segredos (fora do repositório) ----------
PASTA="$HOME/.noisetrack"
ARQ_CHAVE="$PASTA/$ID.chave"
ARQ_CODIGO="$PASTA/$ID.codigo"
ARQ_SQL="$PASTA/$ID.sql"

mkdir -p "$PASTA"
chmod 700 "$PASTA"

# ---------- Chave: 48 caracteres hexadecimais ----------
gerar_chave() {
  if command -v openssl >/dev/null 2>&1; then
    openssl rand -hex 24
  else
    # Sem openssl: 24 bytes aleatórios do sistema, escritos em hexadecimal.
    od -An -N24 -tx1 /dev/urandom | tr -d ' \n'
  fi
}

CHAVE="$(gerar_chave)"
if ! [[ "$CHAVE" =~ ^[0-9a-f]{48}$ ]]; then
  echo "Não consegui gerar a chave aleatória (openssl e /dev/urandom falharam)."
  exit 1
fi

# ---------- Código de pareamento: XXXX-XXXX ----------
# Alfabeto sem 0, O, 1, I e L (31 símbolos), para não confundir na etiqueta.
ALFABETO="23456789ABCDEFGHJKMNPQRSTUVWXYZ"

gerar_codigo() {
  local codigo="" n
  while [ "${#codigo}" -lt 8 ]; do
    for n in $(od -An -N16 -tu1 /dev/urandom); do
      # Descarta bytes >= 248 (31 x 8): assim todos os símbolos têm a mesma chance.
      if [ "$n" -lt 248 ] && [ "${#codigo}" -lt 8 ]; then
        codigo+="${ALFABETO:$((n % 31)):1}"
      fi
    done
  done
  echo "${codigo:0:4}-${codigo:4:4}"
}

# Rodar de novo troca só a CHAVE. O código do aparelho já cadastrado continua o
# mesmo (o SQL abaixo não mexe nele), então reaproveitamos o que ficou guardado
# da vez anterior para mostrar o código certo na tela.
JA_EXISTIA=0
if [ -f "$ARQ_CODIGO" ] && [[ "$(cat "$ARQ_CODIGO")" =~ ^[2-9A-HJKMNP-Z]{4}-[2-9A-HJKMNP-Z]{4}$ ]]; then
  CODIGO="$(cat "$ARQ_CODIGO")"
  JA_EXISTIA=1
else
  CODIGO="$(gerar_codigo)"
fi

# ---------- Guardar a chave ----------
# Se já havia uma chave, guarda a anterior (a placa pode ainda estar com ela).
if [ -f "$ARQ_CHAVE" ]; then
  mv -f "$ARQ_CHAVE" "$ARQ_CHAVE.anterior"
  chmod 600 "$ARQ_CHAVE.anterior"
fi
printf '%s\n' "$CHAVE" > "$ARQ_CHAVE"
printf '%s\n' "$CODIGO" > "$ARQ_CODIGO"
chmod 600 "$ARQ_CHAVE" "$ARQ_CODIGO"

# ---------- SQL para o Supabase ----------
# crypt() + gen_salt('bf') guardam só um "embaralhado" (bcrypt) da chave.
# on conflict: se o ID já existe, troca apenas a chave e mantém o resto.
cat > "$ARQ_SQL" <<SQL
insert into public.aparelhos (id, codigo_pareamento, chave_hash)
values (
  '$ID',
  '$CODIGO',
  extensions.crypt('$CHAVE', extensions.gen_salt('bf'))
)
on conflict (id) do update set chave_hash = excluded.chave_hash;
SQL
chmod 600 "$ARQ_SQL"

# ---------- Área de transferência ----------
COPIOU=0
if command -v clip >/dev/null 2>&1; then
  clip < "$ARQ_SQL" && COPIOU=1
elif command -v clip.exe >/dev/null 2>&1; then
  clip.exe < "$ARQ_SQL" && COPIOU=1
fi

# ---------- O que aparece na tela (nada secreto) ----------
echo
echo "Aparelho:             $ID"
echo "Código de pareamento: $CODIGO"
if [ "$JA_EXISTIA" -eq 1 ]; then
  echo "(aparelho já cadastrado neste PC: o código foi mantido e só a chave mudou)"
fi
echo
if [ "$COPIOU" -eq 1 ]; then
  echo "O SQL já está na área de transferência. Agora:"
else
  echo "Não achei o comando 'clip', então o SQL NÃO foi copiado."
  echo "Abra o arquivo $ARQ_SQL, copie o conteúdo e siga os passos:"
fi
echo "  1. No Supabase, abra o SQL Editor (New query)."
echo "  2. Cole (Ctrl+V) e clique em Run."
echo "  3. Apague essa query do histórico do SQL Editor (ela contém a chave)."
echo "  4. Copie qualquer outro texto, para tirar o SQL da área de transferência."
echo "  5. Se o ID já existia, a chave da placa antiga deixa de valer: grave a nova."
echo
echo "A chave ficou em $ARQ_CHAVE (fora do repositório; use-a só na placa)."
