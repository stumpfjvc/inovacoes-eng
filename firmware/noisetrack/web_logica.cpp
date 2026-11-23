// NoiseTrack: partes puras da página. Sem rede, sem Serial, sem millis().
#include "web_logica.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"

// ---------- JSON ----------
// Escreve "62.9" ou "null" em "texto".
static void numeroOuNull(char* texto, size_t tam, float v, bool valido) {
  if (valido) snprintf(texto, tam, "%.1f", v);
  else        snprintf(texto, tam, "null");
}

size_t web_montar_json(const EstadoCompartilhado& e, const char* id, char* saida, size_t tam) {
  char db[12], a[12], b[12];
  numeroOuNull(db, sizeof(db), e.dbCombinado, e.validoCombinado);
  numeroOuNull(a, sizeof(a), e.dbA, e.validoA);
  numeroOuNull(b, sizeof(b), e.dbB, e.validoB);
  const int n = snprintf(saida, tam,
      "{\"id\":\"%s\",\"db\":%s,\"a\":%s,\"b\":%s,\"valido\":%s,\"estado\":\"%s\","
      "\"acima_ms\":%lu,\"limite\":%d,\"x\":%d,\"evento\":\"%s\",\"evento_n\":%lu}",
      id, db, a, b, e.validoCombinado ? "true" : "false", nomeEstado(e.estado),
      (unsigned long)e.acimaMs, (int)e.limiteDb, e.tempoXs, nomeEvento(e.ultimoEvento),
      (unsigned long)e.contadorEventos);
  // snprintf devolve quanto PRECISARIA; se não coube, avisa com 0.
  return (n > 0 && (size_t)n < tam) ? (size_t)n : 0;
}

// ---------- Formulário ----------
bool web_ler_inteiro(const char* s, long* v) {
  if (s == nullptr || *s == '\0') return false;
  char* resto = nullptr;
  *v = strtol(s, &resto, 10);
  return resto != s && *resto == '\0';
}

ErroConfig web_validar_config(const char* limiteTxt, const char* xTxt, int* limite, int* tempoXs) {
  long l = 0, x = 0;
  if (!web_ler_inteiro(limiteTxt, &l))           return ErroConfig::LIMITE_NAO_NUMERO;
  if (l < LIMITE_MIN_DB || l > LIMITE_MAX_DB)    return ErroConfig::LIMITE_FAIXA;
  if (!web_ler_inteiro(xTxt, &x))                return ErroConfig::X_NAO_NUMERO;
  if (x < TEMPO_X_MIN_S || x > TEMPO_X_MAX_S)    return ErroConfig::X_FAIXA;
  *limite  = (int)l;
  *tempoXs = (int)x;
  return ErroConfig::NENHUM;
}

const char* web_campo_erro_config(ErroConfig e) {
  switch (e) {
    case ErroConfig::LIMITE_NAO_NUMERO:
    case ErroConfig::LIMITE_FAIXA:      return "limite";
    case ErroConfig::X_NAO_NUMERO:
    case ErroConfig::X_FAIXA:           return "x";
    case ErroConfig::NENHUM:            break;
  }
  return "nenhum";
}

size_t web_mensagem_erro_config(ErroConfig e, char* saida, size_t tam) {
  int n = 0;
  switch (e) {
    case ErroConfig::LIMITE_NAO_NUMERO:
    case ErroConfig::LIMITE_FAIXA:
      n = snprintf(saida, tam, "O limite deve ser um número inteiro entre %d e %d dB.",
                   LIMITE_MIN_DB, LIMITE_MAX_DB);
      break;
    case ErroConfig::X_NAO_NUMERO:
    case ErroConfig::X_FAIXA:
      n = snprintf(saida, tam, "O tempo deve ser um número inteiro entre %d e %d segundos.",
                   TEMPO_X_MIN_S, TEMPO_X_MAX_S);
      break;
    case ErroConfig::NENHUM:
      n = snprintf(saida, tam, "OK");
      break;
  }
  return (n > 0 && (size_t)n < tam) ? (size_t)n : 0;
}

bool web_senha_confere(const char* digitada, const char* certa) {
  if (digitada == nullptr || certa == nullptr) return false;
  const size_t nCerta    = strlen(certa);
  const size_t nDigitada = strlen(digitada);
  if (nCerta == 0) return false;
  // Junta as diferenças de todas as letras com OU, sem parar na primeira errada.
  unsigned char diferenca = (nDigitada == nCerta) ? 0 : 1;
  for (size_t i = 0; i < nCerta; i++) {
    const unsigned char c = (i < nDigitada) ? (unsigned char)digitada[i] : 0;
    diferenca |= c ^ (unsigned char)certa[i];
  }
  return diferenca == 0;
}

// ---------- Bloqueio ----------
static uint32_t decorrido(uint32_t agora, uint32_t t0) { return (uint32_t)(agora - t0); }

bool BloqueioSenha::bloqueado(uint32_t agora) {
  if (bloqueado_ && decorrido(agora, tBloqueio_) >= WEB_BLOQUEIO_MS) {
    bloqueado_ = false;   // o tempo acabou: libera e recomeça a contagem
    erros_ = 0;
  }
  return bloqueado_;
}

uint32_t BloqueioSenha::esperaMs(uint32_t agora) const {
  if (!bloqueado_) return 0;
  const uint32_t passou = decorrido(agora, tBloqueio_);
  return passou >= WEB_BLOQUEIO_MS ? 0 : WEB_BLOQUEIO_MS - passou;
}

int BloqueioSenha::restantes() const {
  return erros_ >= WEB_TENTATIVAS_MAX ? 0 : WEB_TENTATIVAS_MAX - erros_;
}

void BloqueioSenha::registrarErro(uint32_t agora) {
  if (bloqueado_) return;
  erros_++;
  if (erros_ >= WEB_TENTATIVAS_MAX) {
    bloqueado_ = true;
    tBloqueio_ = agora;
  }
}

// ---------- WebSocket ----------
bool ws_extrair_chave(const char* linha, char* chave, size_t tam) {
  static const char NOME[] = "sec-websocket-key:";
  const size_t n = sizeof(NOME) - 1;
  for (size_t i = 0; i < n; i++) {   // compara sem ligar para maiúsculas
    if (linha[i] == '\0' || tolower((unsigned char)linha[i]) != NOME[i]) return false;
  }
  const char* v = linha + n;
  while (*v == ' ' || *v == '\t') v++;
  size_t len = strlen(v);
  while (len > 0 && (v[len - 1] == ' ' || v[len - 1] == '\t' || v[len - 1] == '\r')) len--;
  if (len == 0 || len >= tam) return false;
  memcpy(chave, v, len);
  chave[len] = '\0';
  return true;
}

bool ws_calcular_aceite(const char* chave, char* saida, size_t tam) {
  // Código fixo definido pelo protocolo WebSocket (RFC 6455).
  static const char GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  char junto[96];
  const int n = snprintf(junto, sizeof(junto), "%s%s", chave, GUID);
  if (n <= 0 || (size_t)n >= sizeof(junto)) return false;

  unsigned char resumo[20];
  if (mbedtls_sha1_ret((const unsigned char*)junto, (size_t)n, resumo) != 0) return false;

  size_t escrito = 0;
  if (mbedtls_base64_encode((unsigned char*)saida, tam, &escrito, resumo, sizeof(resumo)) != 0) {
    return false;
  }
  return true;   // mbedtls já coloca o fim do texto
}

size_t ws_cabecalho_texto(size_t tamDados, uint8_t* cab) {
  cab[0] = 0x81;   // FIN (quadro completo) + tipo 1 (texto)
  if (tamDados <= 125) {
    cab[1] = (uint8_t)tamDados;
    return 2;
  }
  cab[1] = 126;    // o tamanho vem nos 2 bytes seguintes
  cab[2] = (uint8_t)(tamDados >> 8);
  cab[3] = (uint8_t)(tamDados & 0xFF);
  return 4;
}
