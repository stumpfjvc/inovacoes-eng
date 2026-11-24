// NoiseTrack: partes puras da nuvem (Supabase). Não usam rede, Serial nem
// millis(): recebem textos e números e devolvem textos e números. Assim dá para
// testar tudo no autoteste (teste_nuvem.cpp), no mesmo padrão de rede e web.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "estado.h"

// Resultado de uma tentativa de sincronizar. Parecido com um enum do Java.
enum class ResultadoNuvem : uint8_t {
  NENHUM,             // ainda não tentou
  OK,                 // resposta válida; limite e X aceitos
  SEM_CONEXAO,        // não conectou (sem internet, DNS, TLS, servidor fora)
  SEM_RESPOSTA,       // conectou, mas a resposta HTTP não chegou inteira ou veio quebrada
  APARELHO_INVALIDO,  // o banco não reconheceu o ID/chave do aparelho
  ERRO_SERVIDOR,      // outro código HTTP de erro
  RESPOSTA_INVALIDA,  // JSON ruim, campo faltando ou valor que não é número inteiro
  FORA_DA_FAIXA       // limite/X fora de 30–100 dB / 1–60 s
};
const char* nuvem_nome_resultado(ResultadoNuvem r);

// Estado da regra no texto que o banco aceita: calmo, contando, alerta, sem_sinal.
// (O aquecimento do microfone já chega aqui como SEM_SINAL.)
const char* nuvem_nome_estado(Estado e);

// ---------- Configuração ----------
// true se o texto estiver vazio ou ainda for um modelo ("COLE_AQUI...").
bool nuvem_eh_placeholder(const char* s);
// Confere URL, chave pública e chave do aparelho. Se algo estiver errado,
// devolve false e aponta *motivo para uma frase curta (sem mostrar chaves).
bool nuvem_config_valida(const char* url, const char* chavePublica,
                         const char* chaveAparelho, const char** motivo);
// "https://xxxx.supabase.co" (ou com "/" no fim) -> "xxxx.supabase.co".
// Recusa http://, porta, caminho e caracteres estranhos.
bool nuvem_extrair_host(const char* url, char* host, size_t tam);
// Mostra só o começo de uma chave: "sb_publishab...(46 caracteres)".
void nuvem_mascarar_chave(const char* chave, char* saida, size_t tam);

// ---------- Pedido ----------
// Escapa um texto para dentro de aspas no JSON (\" \\ e caracteres de controle).
// Devolve false se não couber.
bool nuvem_escapar_json(const char* s, char* saida, size_t tam);
// {"p_id":"...","p_chave":"...","p_nivel":58.3,"p_estado":"calmo"}
// Nível inválido (sem sinal) vai como null. Devolve o tamanho, ou 0 se não coube.
size_t nuvem_montar_json(const char* id, const char* chave, float nivelDb, bool nivelValido,
                         Estado estado, char* saida, size_t tam);

// ---------- Resposta ----------
// Separa a resposta HTTP crua: código (200, 400...) e corpo. Entende corpo com
// Content-Length, até o fim da conexão, ou em pedaços ("chunked").
// Devolve false se a resposta estiver incompleta ou quebrada.
bool nuvem_ler_resposta_http(const char* bruto, size_t n, int* status,
                             char* corpo, size_t tamCorpo);

// Com a conexão reaproveitada (keep-alive) o servidor NÃO fecha ao terminar, então
// não dá para ler "até acabar". Esta função diz, olhando o que já chegou, se a
// resposta já está inteira: pelo Content-Length ou pelo último pedaço (chunked).
enum class FimHttp : uint8_t {
  INCOMPLETA,   // ainda falta chegar (continue lendo)
  COMPLETA,     // chegou inteira; *tamTotal = bytes dela (cabeçalhos + corpo)
  INVALIDA      // não é HTTP, cabeçalho quebrado ou pedaços fora do formato
};
// *servidorFecha vira true se o servidor avisou "Connection: close" (ou não deu
// tamanho nenhum e só termina fechando): neste caso não reaproveite a conexão.
// Se chegarem bytes além de *tamTotal, também não reaproveite.
FimHttp nuvem_fim_da_resposta(const char* bruto, size_t n, size_t* tamTotal, bool* servidorFecha);

// Interpreta o corpo de aparelho_sync. Valida limite e X com a MESMA função da
// página (web_validar_config). Só escreve *limite e *segundos se der OK.
ResultadoNuvem nuvem_interpretar_resposta(int status, const char* corpo,
                                          int* limite, int* segundos);

// ---------- Espera entre tentativas ----------
// Sucesso: volta a NUVEM_INTERVALO_MS. Falha: dobra, até NUVEM_ESPERA_MAX_MS
// (com 1 s: 1 -> 2 -> 4 -> 8 -> 16 -> 32 -> 60 -> 60 s).
uint32_t nuvem_proxima_espera(uint32_t esperaAtualMs, bool sucesso);
// A mesma conta com o intervalo e o teto como parâmetros (é o que o autoteste usa,
// para conferir 1 s e 10 s sem depender do config.h).
uint32_t nuvem_proxima_espera_com(uint32_t esperaAtualMs, bool sucesso,
                                  uint32_t intervaloMs, uint32_t maximoMs);

// Quando é o próximo envio. Uma tentativa que começou em "inicioMs" e terminou
// em "fimMs" (pode ter demorado vários ciclos, por timeout) NÃO faz o próximo
// envio sair colado nela: os ciclos que passaram enquanto ela rodava são pulados
// e o próximo cai no primeiro ciclo depois de "fimMs". *pulados (opcional) diz
// quantos ciclos foram pulados. Funciona quando millis() dá a volta.
uint32_t nuvem_proximo_envio(uint32_t inicioMs, uint32_t fimMs, uint32_t esperaMs,
                             uint32_t* pulados);
