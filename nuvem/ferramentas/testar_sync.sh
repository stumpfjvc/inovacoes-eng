#!/usr/bin/env bash
# NoiseTrack: finge ser a placa e chama a função aparelho_sync do Supabase.
#
# Uso:  bash nuvem/ferramentas/testar_sync.sh <id>
# Ex.:  bash nuvem/ferramentas/testar_sync.sh noisetrack-f6d6d5
#
# Precisa de dois arquivos em ~/.noisetrack/ (fora do repositório):
#   <id>.chave    criado por novo_aparelho.sh
#   projeto.env   criado por você (explicação abaixo, se faltar)
#
# Este script USA A INTERNET quando roda de verdade. Na tela aparece só a
# resposta do servidor: a chave do aparelho nunca é impressa.

set -euo pipefail

# ---------- Argumento ----------
uso() {
  echo "Uso: bash nuvem/ferramentas/testar_sync.sh <id>"
  echo "Ex.: bash nuvem/ferramentas/testar_sync.sh noisetrack-f6d6d5"
}

if [ "$#" -ne 1 ] || [ -z "$1" ]; then
  uso
  exit 1
fi
ID="$1"
if ! [[ "$ID" =~ ^[A-Za-z0-9_-]{1,64}$ ]]; then
  echo "ID inválido: use só letras, números, '-' e '_' (até 64 caracteres)."
  uso
  exit 1
fi

PASTA="$HOME/.noisetrack"
ARQ_CHAVE="$PASTA/$ID.chave"
ARQ_ENV="$PASTA/projeto.env"

# ---------- Chave do aparelho ----------
if [ ! -f "$ARQ_CHAVE" ]; then
  echo "Não achei a chave do aparelho '$ID' em $ARQ_CHAVE."
  echo "Rode primeiro:  bash nuvem/ferramentas/novo_aparelho.sh $ID"
  echo "e cole o SQL no SQL Editor do Supabase."
  exit 1
fi
CHAVE="$(tr -d '\r\n ' < "$ARQ_CHAVE")"
if ! [[ "$CHAVE" =~ ^[0-9a-f]{48}$ ]]; then
  echo "O arquivo $ARQ_CHAVE não tem uma chave válida (48 caracteres hexadecimais)."
  echo "Rode de novo:  bash nuvem/ferramentas/novo_aparelho.sh $ID"
  exit 1
fi

# ---------- Dados do projeto (projeto.env) ----------
if [ ! -f "$ARQ_ENV" ]; then
  echo "Falta o arquivo $ARQ_ENV."
  echo
  echo "Crie esse arquivo (um texto simples, NOME=valor, sem aspas) com 2 linhas:"
  echo
  echo "  SUPABASE_URL=https://SEU-PROJETO.supabase.co"
  echo "  SUPABASE_CHAVE_PUBLICA=sua-chave-anon-ou-publishable"
  echo
  echo "Onde achar, no painel do Supabase: Project Settings > API."
  echo "  - SUPABASE_URL: a 'Project URL'."
  echo "  - SUPABASE_CHAVE_PUBLICA: a chave 'anon public' (ou 'publishable')."
  echo "NUNCA use a chave 'service_role' (ou 'secret'): ela abre o banco inteiro."
  exit 1
fi

# Lê NOME=valor SEM executar o arquivo (nada de "source"). Ignora comentários,
# tira o fim de linha do Windows (\r) e aspas em volta do valor.
ler_env() {
  grep -E "^[[:space:]]*$1[[:space:]]*=" "$ARQ_ENV" | tail -n 1 \
    | sed -E "s/^[^=]*=//; s/\r$//; s/^[[:space:]]+//; s/[[:space:]]+$//; s/^\"(.*)\"$/\1/; s/^'(.*)'\$/\1/" \
    || true
}

URL="$(ler_env SUPABASE_URL)"
CHAVE_PUBLICA="$(ler_env SUPABASE_CHAVE_PUBLICA)"
URL="${URL%/}"   # tira a barra final, se houver

if [ -z "$URL" ] || [ -z "$CHAVE_PUBLICA" ]; then
  echo "O arquivo $ARQ_ENV precisa ter as linhas SUPABASE_URL e SUPABASE_CHAVE_PUBLICA."
  exit 1
fi
if ! [[ "$URL" =~ ^https?://[^[:space:]]+$ ]]; then
  echo "SUPABASE_URL não parece um endereço (deve começar com https://)."
  exit 1
fi

# Proteção: recusa a chave de administrador, caso tenha sido colada por engano.
# A chave nova começa com "sb_secret_"; a antiga é um JWT cujo conteúdo diz
# "role":"service_role".
EH_SECRETA=0
case "$CHAVE_PUBLICA" in
  sb_secret_*) EH_SECRETA=1 ;;
esac
if [[ "$CHAVE_PUBLICA" == *.*.* ]]; then
  PAYLOAD="$(printf '%s' "$CHAVE_PUBLICA" | cut -d. -f2 | tr '_-' '/+')"
  while [ $(( ${#PAYLOAD} % 4 )) -ne 0 ]; do PAYLOAD+="="; done
  if printf '%s' "$PAYLOAD" | base64 -d 2>/dev/null | grep -q 'service_role'; then
    EH_SECRETA=1
  fi
fi
if [ "$EH_SECRETA" -eq 1 ]; then
  echo "A chave em SUPABASE_CHAVE_PUBLICA é a de ADMINISTRADOR (service_role/secret)."
  echo "Não vou usá-la. Troque pela chave 'anon public' (ou 'publishable') e,"
  echo "como ela apareceu num arquivo de texto, considere gerar uma nova no painel."
  exit 1
fi

# ---------- Chamada ----------
# O corpo vai pela entrada padrão (--data-binary @-): assim a chave do aparelho
# não aparece na linha de comando (que outros programas podem ver). Só o
# cabeçalho "apikey" é enviado, como pedido (a chave pública não é segredo).
# Os valores são seguros para JSON: o ID foi validado e a chave é hexadecimal.
CORPO="$(printf '{"p_id":"%s","p_chave":"%s","p_nivel":58.3,"p_estado":"calmo"}' "$ID" "$CHAVE")"

echo "Chamando aparelho_sync para '$ID'..."
set +e
SAIDA="$(printf '%s' "$CORPO" | curl -sS --max-time 20 \
  -X POST "$URL/rest/v1/rpc/aparelho_sync" \
  -H "apikey: $CHAVE_PUBLICA" \
  -H "Content-Type: application/json" \
  --data-binary @- \
  -w $'\n%{http_code}' 2>&1)"
STATUS_CURL=$?
set -e

# A última linha da saída é o código HTTP (por causa do -w); o resto é a resposta.
CODIGO_HTTP="${SAIDA##*$'\n'}"
RESPOSTA="${SAIDA%$'\n'*}"
RESPOSTA="${RESPOSTA//$CHAVE/***}"   # por garantia: a chave nunca aparece na tela

if [ "$STATUS_CURL" -ne 0 ]; then
  echo "Não consegui falar com o servidor:"
  echo "$RESPOSTA"
  echo "Confira a internet e o SUPABASE_URL em $ARQ_ENV."
  exit 1
fi

echo "Resposta do servidor (HTTP $CODIGO_HTTP):"
echo "$RESPOSTA"
echo

# ---------- O que a resposta quer dizer ----------
if [[ "$RESPOSTA" == *aparelho_invalido* ]]; then
  echo "O servidor diz 'aparelho_invalido': a chave do arquivo $ARQ_CHAVE"
  echo "não confere com a que está no banco (ou o ID não está cadastrado)."
  echo "Para resolver, rode de novo:  bash nuvem/ferramentas/novo_aparelho.sh $ID"
  echo "cole o SQL no SQL Editor do Supabase e clique em Run. Depois teste outra vez."
  exit 1
elif [[ "$RESPOSTA" == *limite_db* ]]; then
  echo "Deu certo: o aparelho foi reconhecido. Veja a tabela 'leituras' no Supabase"
  echo "(entra no máximo 1 linha a cada 10 s por aparelho)."
elif [[ "$CODIGO_HTTP" == 401 || "$CODIGO_HTTP" == 403 ]]; then
  echo "O servidor recusou a chave pública. Confira SUPABASE_CHAVE_PUBLICA em $ARQ_ENV."
  exit 1
elif [[ "$CODIGO_HTTP" == 404 ]]; then
  echo "Função não encontrada. Confira o SUPABASE_URL e se o nuvem/schema.sql já foi rodado."
  exit 1
else
  echo "Resposta inesperada. Confira o SUPABASE_URL e se o nuvem/schema.sql já foi rodado."
  exit 1
fi
