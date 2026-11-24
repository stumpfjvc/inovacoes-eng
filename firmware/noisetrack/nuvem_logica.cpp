// NoiseTrack: partes puras da nuvem. Sem rede, sem Serial, sem millis().
#include "nuvem_logica.h"
#include "config.h"
#include "web_logica.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "mbedtls/base64.h"
#include "mbedtls/x509.h"
#include "mbedtls/ssl.h"

const char* nuvem_nome_resultado(ResultadoNuvem r) {
  switch (r) {
    case ResultadoNuvem::NENHUM:            return "ainda nao tentou";
    case ResultadoNuvem::OK:                return "ok";
    case ResultadoNuvem::SEM_CONEXAO:       return "sem conexao";
    case ResultadoNuvem::SEM_RESPOSTA:      return "sem resposta completa";
    case ResultadoNuvem::APARELHO_INVALIDO: return "aparelho_invalido";
    case ResultadoNuvem::ERRO_SERVIDOR:     return "erro do servidor";
    case ResultadoNuvem::RESPOSTA_INVALIDA: return "resposta invalida";
    case ResultadoNuvem::FORA_DA_FAIXA:     return "limite/X fora da faixa";
    case ResultadoNuvem::ERRO_CERTIFICADO:  return "certificado recusado";
  }
  return "?";
}

const char* nuvem_nome_estado(Estado e) {
  switch (e) {
    case Estado::CALMO:     return "calmo";
    case Estado::CONTANDO:  return "contando";
    case Estado::ALERTA:    return "alerta";
    case Estado::SEM_SINAL: return "sem_sinal";   // inclui o aquecimento do microfone
  }
  return "sem_sinal";
}

// ---------- Configuração ----------
bool nuvem_eh_placeholder(const char* s) {
  if (s == nullptr || s[0] == '\0') return true;
  return strstr(s, "COLE_AQUI") != nullptr || strstr(s, "SEU-PROJETO") != nullptr;
}

// Chave antiga do Supabase (JWT): o meio, decodificado, diz o papel. Se for
// "service_role", é a chave de ADMINISTRADOR e não pode ir para a placa.
static bool jwtEhServiceRole(const char* k) {
  if (strncmp(k, "eyJ", 3) != 0) return false;
  const char* p1 = strchr(k, '.');
  if (p1 == nullptr) return false;
  const char* p2 = strchr(p1 + 1, '.');
  if (p2 == nullptr) return false;
  const size_t n = (size_t)(p2 - (p1 + 1));
  if (n == 0 || n > 600) return false;

  char b64[608];
  memcpy(b64, p1 + 1, n);
  for (size_t i = 0; i < n; i++) {   // base64url -> base64 comum
    if (b64[i] == '-') b64[i] = '+';
    else if (b64[i] == '_') b64[i] = '/';
  }
  size_t m = n;
  while (m % 4 != 0) b64[m++] = '=';

  unsigned char dec[460];
  size_t olen = 0;
  if (mbedtls_base64_decode(dec, sizeof(dec) - 1, &olen, (const unsigned char*)b64, m) != 0) {
    return false;
  }
  dec[olen] = '\0';
  return strstr((const char*)dec, "service_role") != nullptr;
}

bool nuvem_config_valida(const char* url, const char* chavePublica,
                         const char* chaveAparelho, const char** motivo) {
  char host[101];
  if (nuvem_eh_placeholder(url)) {
    *motivo = "NUVEM_URL ainda e o modelo (preencha no config_local.h)";
  } else if (!nuvem_extrair_host(url, host, sizeof(host))) {
    *motivo = "NUVEM_URL invalida (use https://xxxx.supabase.co, sem nada depois)";
  } else if (nuvem_eh_placeholder(chavePublica)) {
    *motivo = "NUVEM_CHAVE_PUBLICA ainda e o modelo (preencha no config_local.h)";
  } else if (strncmp(chavePublica, "sb_secret_", 10) == 0 || jwtEhServiceRole(chavePublica)) {
    *motivo = "NUVEM_CHAVE_PUBLICA e a chave de ADMINISTRADOR: use a publishable/anon";
  } else if (strlen(chavePublica) > 400) {
    *motivo = "NUVEM_CHAVE_PUBLICA longa demais";
  } else if (nuvem_eh_placeholder(chaveAparelho)) {
    *motivo = "NUVEM_CHAVE_APARELHO ausente ou modelo (coloque no secrets.h)";
  } else if (strlen(chaveAparelho) > 128) {
    *motivo = "NUVEM_CHAVE_APARELHO longa demais";
  } else {
    *motivo = "";
    return true;
  }
  return false;
}

bool nuvem_extrair_host(const char* url, char* host, size_t tam) {
  static const char PREFIXO[] = "https://";
  if (url == nullptr || strncmp(url, PREFIXO, sizeof(PREFIXO) - 1) != 0) return false;
  const char* p = url + sizeof(PREFIXO) - 1;
  size_t n = 0;
  while (p[n] != '\0' && p[n] != '/') {
    const char c = p[n];
    if (!(isalnum((unsigned char)c) || c == '.' || c == '-')) return false;   // sem porta, ? ou @
    n++;
  }
  if (n == 0 || n >= tam || n > 100) return false;
  if (p[n] == '/' && p[n + 1] != '\0') return false;   // só aceita "/" no fim, sem caminho
  memcpy(host, p, n);
  host[n] = '\0';
  return true;
}

void nuvem_mascarar_chave(const char* chave, char* saida, size_t tam) {
  const size_t n = (chave == nullptr) ? 0 : strlen(chave);
  if (n <= 12) snprintf(saida, tam, "***");
  else         snprintf(saida, tam, "%.12s...(%u caracteres)", chave, (unsigned)n);
}

// ---------- Pedido ----------
bool nuvem_escapar_json(const char* s, char* saida, size_t tam) {
  if (s == nullptr || tam == 0) return false;
  size_t j = 0;
  for (size_t i = 0; s[i] != '\0'; i++) {
    const unsigned char c = (unsigned char)s[i];
    char tmp[8];
    const char* parte;
    if (c == '"')       parte = "\\\"";
    else if (c == '\\') parte = "\\\\";
    else if (c < 0x20) {   // controle (\n, \t...) vira \u00XX
      snprintf(tmp, sizeof(tmp), "\\u%04x", c);
      parte = tmp;
    } else {
      tmp[0] = (char)c;
      tmp[1] = '\0';
      parte = tmp;
    }
    const size_t k = strlen(parte);
    if (j + k >= tam) return false;
    memcpy(saida + j, parte, k);
    j += k;
  }
  saida[j] = '\0';
  return true;
}

size_t nuvem_montar_json(const char* id, const char* chave, float nivelDb, bool nivelValido,
                         Estado estado, char* saida, size_t tam) {
  char idEsc[96], chaveEsc[300], nivel[16];
  if (!nuvem_escapar_json(id, idEsc, sizeof(idEsc))) return 0;
  if (!nuvem_escapar_json(chave, chaveEsc, sizeof(chaveEsc))) return 0;
  if (nivelValido && isfinite(nivelDb)) snprintf(nivel, sizeof(nivel), "%.1f", nivelDb);
  else                                  snprintf(nivel, sizeof(nivel), "null");
  const int n = snprintf(saida, tam,
                         "{\"p_id\":\"%s\",\"p_chave\":\"%s\",\"p_nivel\":%s,\"p_estado\":\"%s\"}",
                         idEsc, chaveEsc, nivel, nuvem_nome_estado(estado));
  return (n > 0 && (size_t)n < tam) ? (size_t)n : 0;
}

// ---------- Resposta HTTP ----------
// Procura "agulha" nos n primeiros bytes de "h" (que pode não terminar em '\0').
static const char* buscar(const char* h, size_t n, const char* agulha) {
  const size_t k = strlen(agulha);
  if (k == 0 || k > n) return nullptr;
  for (size_t i = 0; i + k <= n; i++) {
    if (memcmp(h + i, agulha, k) == 0) return h + i;
  }
  return nullptr;
}

// A linha [ini, fim) começa com "nome" (sem ligar para maiúsculas)?
static bool linhaComeca(const char* ini, const char* fim, const char* nome) {
  const size_t k = strlen(nome);
  if ((size_t)(fim - ini) < k) return false;
  for (size_t i = 0; i < k; i++) {
    if (tolower((unsigned char)ini[i]) != nome[i]) return false;
  }
  return true;
}

static bool linhaContem(const char* ini, const char* fim, const char* palavra) {
  const size_t k = strlen(palavra);
  for (const char* p = ini; p + k <= fim; p++) {
    size_t i = 0;
    while (i < k && tolower((unsigned char)p[i]) == palavra[i]) i++;
    if (i == k) return true;
  }
  return false;
}

// Linha de status: "HTTP/1.1 200 OK" -> 200.
static bool lerStatus(const char* bruto, size_t n, int* status) {
  if (n < 12 || strncmp(bruto, "HTTP/", 5) != 0) return false;
  const char* espaco = (const char*)memchr(bruto, ' ', n);
  if (espaco == nullptr || (size_t)(espaco - bruto) + 4 > n) return false;
  if (!isdigit((unsigned char)espaco[1]) || !isdigit((unsigned char)espaco[2]) ||
      !isdigit((unsigned char)espaco[3])) {
    return false;
  }
  *status = (espaco[1] - '0') * 100 + (espaco[2] - '0') * 10 + (espaco[3] - '0');
  return true;
}

// O que interessa dos cabeçalhos de uma resposta.
struct CabecalhosHttp {
  bool          chunked    = false;   // Transfer-Encoding: chunked
  bool          fecha      = false;   // Connection: close
  bool          temTamanho = false;   // veio Content-Length
  unsigned long tamanho    = 0;
  bool          invalido   = false;   // Content-Length que não é um número
};

// "fimCab" aponta para o "\r\n\r\n" que termina os cabeçalhos.
static CabecalhosHttp lerCabecalhos(const char* bruto, size_t n, const char* fimCab) {
  CabecalhosHttp h;
  const char* primeiraQuebra = (const char*)memchr(bruto, '\n', n);
  if (primeiraQuebra == nullptr) {
    h.invalido = true;
    return h;
  }
  const char* linha = primeiraQuebra + 1;   // pula a linha de status
  while (linha < fimCab + 2) {
    const char* fimLinha = buscar(linha, (size_t)(fimCab + 2 - linha), "\r\n");
    if (fimLinha == nullptr) break;
    if (linhaComeca(linha, fimLinha, "transfer-encoding:")) {
      if (linhaContem(linha, fimLinha, "chunked")) h.chunked = true;
    } else if (linhaComeca(linha, fimLinha, "connection:")) {
      if (linhaContem(linha, fimLinha, "close")) h.fecha = true;
    } else if (linhaComeca(linha, fimLinha, "content-length:")) {
      const char* p = linha + 15;
      while (p < fimLinha && (*p == ' ' || *p == '\t')) p++;
      if (p >= fimLinha || !isdigit((unsigned char)*p)) {
        h.invalido = true;   // "Content-Length: abc" não vira 0
      } else {
        unsigned long v = 0;
        while (p < fimLinha && isdigit((unsigned char)*p)) {
          if (v > 100000000UL) { h.invalido = true; break; }   // absurdo para uma resposta
          v = v * 10 + (unsigned long)(*p - '0');
          p++;
        }
        h.temTamanho = true;
        h.tamanho    = v;
      }
    }
    linha = fimLinha + 2;
  }
  return h;
}

bool nuvem_ler_resposta_http(const char* bruto, size_t n, int* status,
                             char* corpo, size_t tamCorpo) {
  if (bruto == nullptr || corpo == nullptr || tamCorpo == 0) return false;
  corpo[0] = '\0';

  // 1) Linha de status: "HTTP/1.1 200 OK"
  if (!lerStatus(bruto, n, status)) return false;

  // 2) Cabeçalhos: até a linha em branco.
  const char* fimCab = buscar(bruto, n, "\r\n\r\n");
  if (fimCab == nullptr) return false;
  const CabecalhosHttp h = lerCabecalhos(bruto, n, fimCab);
  if (h.invalido) return false;
  const bool chunked = h.chunked;

  // 3) Corpo
  const char* p   = fimCab + 4;
  const char* fim = bruto + n;
  size_t j = 0;
  if (chunked) {
    for (;;) {   // cada pedaço: tamanho em hexadecimal, \r\n, dados, \r\n
      char* depois = nullptr;
      const char* fimTam = buscar(p, (size_t)(fim - p), "\r\n");
      if (fimTam == nullptr) return false;
      const unsigned long tamPedaco = strtoul(p, &depois, 16);
      if (depois == p) return false;
      p = fimTam + 2;
      if (tamPedaco == 0) break;   // último pedaço
      if ((size_t)(fim - p) < tamPedaco + 2) return false;   // incompleto
      if (j + tamPedaco >= tamCorpo) return false;
      memcpy(corpo + j, p, tamPedaco);
      j += tamPedaco;
      p += tamPedaco;
      if (p[0] != '\r' || p[1] != '\n') return false;
      p += 2;
    }
  } else {
    size_t tamCorpoHttp = (size_t)(fim - p);
    if (h.temTamanho) {
      if ((size_t)h.tamanho > tamCorpoHttp) return false;   // incompleto
      tamCorpoHttp = (size_t)h.tamanho;
    }
    if (tamCorpoHttp >= tamCorpo) return false;
    memcpy(corpo, p, tamCorpoHttp);
    j = tamCorpoHttp;
  }
  corpo[j] = '\0';
  return true;
}

FimHttp nuvem_fim_da_resposta(const char* bruto, size_t n, size_t* tamTotal, bool* servidorFecha) {
  *tamTotal      = 0;
  *servidorFecha = false;
  if (bruto == nullptr) return FimHttp::INVALIDA;

  // Os primeiros bytes já têm de combinar com "HTTP/" (mesmo antes de chegar tudo).
  static const char PREFIXO[] = "HTTP/";
  const size_t k = n < 5 ? n : 5;
  if (memcmp(bruto, PREFIXO, k) != 0) return FimHttp::INVALIDA;

  const char* fimCab = buscar(bruto, n, "\r\n\r\n");
  if (fimCab == nullptr) return FimHttp::INCOMPLETA;   // cabeçalhos ainda chegando
  int status = 0;
  if (!lerStatus(bruto, (size_t)(fimCab - bruto), &status)) return FimHttp::INVALIDA;

  const CabecalhosHttp h = lerCabecalhos(bruto, n, fimCab);
  if (h.invalido) return FimHttp::INVALIDA;
  *servidorFecha = h.fecha;

  const char*  corpoIni    = fimCab + 4;
  const char*  fim         = bruto + n;
  const size_t disponivel  = (size_t)(fim - corpoIni);
  const size_t deslocCorpo = (size_t)(corpoIni - bruto);

  if (status == 204 || status == 304) {   // por definição, não têm corpo
    *tamTotal = deslocCorpo;
    return FimHttp::COMPLETA;
  }

  if (h.chunked) {
    const char* p = corpoIni;
    for (;;) {   // cada pedaço: tamanho em hexadecimal, \r\n, dados, \r\n
      const char* fimTam = buscar(p, (size_t)(fim - p), "\r\n");
      if (fimTam == nullptr) return FimHttp::INCOMPLETA;
      char* depois = nullptr;
      const unsigned long tam = strtoul(p, &depois, 16);
      if (depois == p) return FimHttp::INVALIDA;
      if (tam == 0) {   // último pedaço; depois dele só pode haver "trailers" e a linha em branco
        const char* fimFinal = buscar(fimTam, (size_t)(fim - fimTam), "\r\n\r\n");
        if (fimFinal == nullptr) return FimHttp::INCOMPLETA;
        *tamTotal = (size_t)(fimFinal + 4 - bruto);
        return FimHttp::COMPLETA;
      }
      p = fimTam + 2;
      const size_t resta = (size_t)(fim - p);
      if (tam > resta || resta - tam < 2) return FimHttp::INCOMPLETA;   // dados + \r\n ainda faltam
      if (p[tam] != '\r' || p[tam + 1] != '\n') return FimHttp::INVALIDA;
      p += tam + 2;
    }
  }

  if (h.temTamanho) {
    if (disponivel < h.tamanho) return FimHttp::INCOMPLETA;
    *tamTotal = deslocCorpo + (size_t)h.tamanho;
    return FimHttp::COMPLETA;
  }

  // Sem tamanho e sem pedaços: a resposta só termina quando o servidor fecha.
  *servidorFecha = true;
  return FimHttp::INCOMPLETA;
}

// Copia o valor cru de "chave": no JSON (ex.: "65") para "saida".
// Devolve false se a chave não existir ou o valor estiver vazio.
static bool extrairValor(const char* corpo, const char* chave, char* saida, size_t tam) {
  char agulha[32];
  snprintf(agulha, sizeof(agulha), "\"%s\"", chave);
  const char* p = strstr(corpo, agulha);
  if (p == nullptr) return false;
  p += strlen(agulha);
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  if (*p != ':') return false;
  p++;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  size_t n = 0;
  while (p[n] != '\0' && strchr(",}] \t\r\n", p[n]) == nullptr) {
    if (n + 1 >= tam) return false;
    saida[n] = p[n];
    n++;
  }
  saida[n] = '\0';
  return n > 0;
}

ResultadoNuvem nuvem_interpretar_resposta(int status, const char* corpo,
                                          int* limite, int* segundos) {
  if (corpo == nullptr) return ResultadoNuvem::RESPOSTA_INVALIDA;
  if (status >= 400 && strstr(corpo, "aparelho_invalido") != nullptr) {
    return ResultadoNuvem::APARELHO_INVALIDO;
  }
  if (status != 200) return ResultadoNuvem::ERRO_SERVIDOR;

  char limiteTxt[16], segundosTxt[16];
  if (!extrairValor(corpo, "limite_db", limiteTxt, sizeof(limiteTxt)) ||
      !extrairValor(corpo, "segundos", segundosTxt, sizeof(segundosTxt))) {
    return ResultadoNuvem::RESPOSTA_INVALIDA;
  }
  // A MESMA validação do formulário da página (30–100 dB e 1–60 s, inteiros).
  int l = 0, s = 0;
  switch (web_validar_config(limiteTxt, segundosTxt, &l, &s)) {
    case ErroConfig::NENHUM:
      *limite = l;
      *segundos = s;
      return ResultadoNuvem::OK;
    case ErroConfig::LIMITE_FAIXA:
    case ErroConfig::X_FAIXA:
      return ResultadoNuvem::FORA_DA_FAIXA;
    default:
      return ResultadoNuvem::RESPOSTA_INVALIDA;   // não é número inteiro
  }
}

// ---------- Certificado ----------
int nuvem_comparar_data(const DataHora& a, const DataHora& b) {
  const int va[6] = {a.ano, a.mes, a.dia, a.hora, a.min, a.seg};
  const int vb[6] = {b.ano, b.mes, b.dia, b.hora, b.min, b.seg};
  for (int i = 0; i < 6; i++) {   // compara do ano para o segundo, como num dicionário
    if (va[i] < vb[i]) return -1;
    if (va[i] > vb[i]) return 1;
  }
  return 0;
}

ValidadeCert nuvem_validade(const DataHora& inicio, const DataHora& fim, const DataHora& agora) {
  if (nuvem_comparar_data(agora, inicio) < 0) return ValidadeCert::AINDA_NAO_VALIDO;
  if (nuvem_comparar_data(agora, fim) > 0)    return ValidadeCert::VENCIDO;
  return ValidadeCert::OK;
}

const char* nuvem_nome_validade(ValidadeCert v) {
  switch (v) {
    case ValidadeCert::OK:               return "dentro da validade";
    case ValidadeCert::AINDA_NAO_VALIDO: return "ainda nao valido (relogio errado?)";
    case ValidadeCert::VENCIDO:          return "vencido";
  }
  return "?";
}

bool nuvem_erro_eh_certificado(int codigo) {
  // Erros do módulo X.509 do mbedTLS ficam entre -0x2080 e -0x3000 (o principal é
  // MBEDTLS_ERR_X509_CERT_VERIFY_FAILED: raiz desconhecida, assinatura ou nome).
  if (codigo <= MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE && codigo >= MBEDTLS_ERR_X509_FATAL_ERROR) {
    return true;
  }
  return codigo == MBEDTLS_ERR_SSL_PEER_VERIFY_FAILED ||
         codigo == MBEDTLS_ERR_SSL_CA_CHAIN_REQUIRED ||
         codigo == MBEDTLS_ERR_SSL_BAD_HS_CERTIFICATE;
}

// ---------- Espera ----------
uint32_t nuvem_proxima_espera_com(uint32_t esperaAtualMs, bool sucesso,
                                  uint32_t intervaloMs, uint32_t maximoMs) {
  if (sucesso) return intervaloMs;
  const uint32_t base = esperaAtualMs < intervaloMs ? intervaloMs : esperaAtualMs;
  // "base > maximo / 2" evita estourar o uint32 antes de comparar com o teto.
  return base > maximoMs / 2 ? maximoMs : base * 2;
}

uint32_t nuvem_proxima_espera(uint32_t esperaAtualMs, bool sucesso) {
  return nuvem_proxima_espera_com(esperaAtualMs, sucesso, NUVEM_INTERVALO_MS, NUVEM_ESPERA_MAX_MS);
}

uint32_t nuvem_proximo_envio(uint32_t inicioMs, uint32_t fimMs, uint32_t esperaMs,
                             uint32_t* pulados) {
  if (pulados != nullptr) *pulados = 0;
  if (esperaMs == 0) return fimMs;
  // A subtração sem sinal continua certa quando millis() dá a volta.
  const uint32_t duracao = (uint32_t)(fimMs - inicioMs);
  const uint32_t ciclosPerdidos = duracao / esperaMs;   // ciclos que passaram durante a tentativa
  if (pulados != nullptr) *pulados = ciclosPerdidos;
  return (uint32_t)(inicioMs + esperaMs * (ciclosPerdidos + 1));
}
