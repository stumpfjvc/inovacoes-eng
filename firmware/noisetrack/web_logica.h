// NoiseTrack: partes puras da página (Parte 10). Não usam rede, Serial nem
// millis(): recebem números e textos e devolvem números e textos. Assim dá para
// testar tudo no autoteste (teste_web.cpp).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "tarefa_audio.h"

// ---------- JSON do estado (WebSocket e GET /status) ----------
// Só vão identificador, dB, estado, eventos e configuração. Nada de áudio.
constexpr size_t WEB_TAM_JSON = 256;

// Monta o JSON numa linha, por exemplo:
// {"id":"noisetrack-a1b2c3","db":63.2,"a":62.9,"b":63.5,"valido":true,
//  "estado":"contando","acima_ms":1250,"limite":65,"x":5,"evento":"disparou","evento_n":3}
// Nível de canal sem sinal sai como null. Devolve o tamanho, ou 0 se não coube.
size_t web_montar_json(const EstadoCompartilhado& e, const char* id, char* saida, size_t tam);

// ---------- Formulário de configuração ----------
// Converte o texto inteiro em número; texto inválido devolve false (não vira 0).
bool web_ler_inteiro(const char* s, long* v);
// Motivo de um formulário recusado. Parecido com um enum do Java.
enum class ErroConfig : uint8_t {
  NENHUM,              // tudo certo
  LIMITE_NAO_NUMERO,   // limite vazio, com letras ou com casas decimais
  LIMITE_FAIXA,        // limite fora de LIMITE_MIN_DB..LIMITE_MAX_DB
  X_NAO_NUMERO,
  X_FAIXA              // X fora de TEMPO_X_MIN_S..TEMPO_X_MAX_S
};
// Lê e valida limite (30 a 100 dB) e X (1 a 60 s), inteiros, vindos do
// formulário. Só escreve em *limite e *tempoXs se os dois estiverem certos.
ErroConfig web_validar_config(const char* limiteTxt, const char* xTxt, int* limite, int* tempoXs);
// Nome curto do erro para o JSON ("limite", "x" ou "nenhum").
const char* web_campo_erro_config(ErroConfig e);
// Mensagem para a pessoa, por exemplo
// "O limite deve ser um número inteiro entre 30 e 100 dB."
// Devolve o tamanho escrito (0 se não coube).
size_t web_mensagem_erro_config(ErroConfig e, char* saida, size_t tam);
// Compara a senha digitada com a certa, sempre no mesmo tempo (não revela,
// pela demora da resposta, quantas letras do começo estavam certas).
bool web_senha_confere(const char* digitada, const char* certa);

// ---------- Bloqueio por tentativas erradas ----------
// WEB_TENTATIVAS_MAX senhas erradas seguidas bloqueiam por WEB_BLOQUEIO_MS.
// Uma senha certa zera a contagem. O tempo entra como parâmetro (testável).
class BloqueioSenha {
 public:
  bool     bloqueado(uint32_t agora);        // também libera quando o tempo acaba
  uint32_t esperaMs(uint32_t agora) const;   // quanto falta para liberar (0 se livre)
  void     registrarErro(uint32_t agora);
  void     registrarAcerto() { erros_ = 0; }
  int      restantes() const;                // tentativas até bloquear

 private:
  int      erros_      = 0;
  bool     bloqueado_  = false;
  uint32_t tBloqueio_  = 0;
};

// ---------- WebSocket (RFC 6455, só o necessário) ----------
// Se a linha do pedido for "Sec-WebSocket-Key: xxxx" (maiúsculas ou minúsculas),
// copia o xxxx para "chave" e devolve true.
bool ws_extrair_chave(const char* linha, char* chave, size_t tam);
// Resposta do aperto de mão: Base64 de SHA-1 de (chave + código fixo do protocolo).
// "saida" precisa de pelo menos 29 posições. Devolve false se não couber.
bool ws_calcular_aceite(const char* chave, char* saida, size_t tam);
// Cabeçalho de um quadro de texto do servidor (sem máscara). Escreve 2 bytes
// (até 125 de dados) ou 4 bytes (até 65535) em "cab" e devolve quantos foram.
size_t ws_cabecalho_texto(size_t tamDados, uint8_t* cab);
