// NoiseTrack: autoteste da parte pura da nuvem. Não usa a rede.
#include "teste_nuvem.h"
#include "config.h"

#if TESTE_NUVEM
#include "nuvem_logica.h"
#include "maximo_janelas.h"
#include <string.h>

static int falhas = 0;

static void conferir(const char* nome, bool ok) {
  if (!ok) falhas++;
  Serial.printf("[teste-nuvem] %s: %s\n", nome, ok ? "PASS" : "FAIL");
}

// Interpreta uma resposta HTTP crua inteira (como chegaria do servidor).
static ResultadoNuvem respostaCrua(const char* http, int* limite, int* segundos) {
  int status = 0;
  char corpo[256];
  if (!nuvem_ler_resposta_http(http, strlen(http), &status, corpo, sizeof(corpo))) {
    return ResultadoNuvem::SEM_RESPOSTA;
  }
  return nuvem_interpretar_resposta(status, corpo, limite, segundos);
}

// Fim da resposta de um texto C inteiro (sem contar tamanhos à mão).
static FimHttp fimDe(const char* texto, size_t* total, bool* fecha) {
  return nuvem_fim_da_resposta(texto, strlen(texto), total, fecha);
}

bool teste_nuvem_rodar() {
  falhas = 0;
  Serial.println("[teste-nuvem] === autoteste da nuvem ===");
  char json[256];
  int l = 0, s = 0;

  // ---------- Estados ----------
  conferir("estados no texto do banco",
           strcmp(nuvem_nome_estado(Estado::CALMO), "calmo") == 0 &&
           strcmp(nuvem_nome_estado(Estado::CONTANDO), "contando") == 0 &&
           strcmp(nuvem_nome_estado(Estado::ALERTA), "alerta") == 0 &&
           strcmp(nuvem_nome_estado(Estado::SEM_SINAL), "sem_sinal") == 0);

  // ---------- JSON do pedido ----------
  nuvem_montar_json("noisetrack-a1b2c3", "abc123", 58.3f, true, Estado::CALMO, json, sizeof(json));
  conferir("JSON do pedido",
           strcmp(json, "{\"p_id\":\"noisetrack-a1b2c3\",\"p_chave\":\"abc123\",\"p_nivel\":58.3,"
                        "\"p_estado\":\"calmo\"}") == 0);

  nuvem_montar_json("x", "k", 0.0f, false, Estado::SEM_SINAL, json, sizeof(json));
  conferir("JSON sem sinal: nivel null",
           strcmp(json, "{\"p_id\":\"x\",\"p_chave\":\"k\",\"p_nivel\":null,\"p_estado\":\"sem_sinal\"}") == 0);

  nuvem_montar_json("id", "a\"b\\c\n", 70.0f, true, Estado::ALERTA, json, sizeof(json));
  conferir("JSON com escape (aspas, barra, quebra de linha)",
           strcmp(json, "{\"p_id\":\"id\",\"p_chave\":\"a\\\"b\\\\c\\u000a\",\"p_nivel\":70.0,"
                        "\"p_estado\":\"alerta\"}") == 0);

  char pequeno[20];
  conferir("JSON que nao cabe devolve 0",
           nuvem_montar_json("noisetrack-a1b2c3", "abc123", 58.3f, true, Estado::CALMO,
                             pequeno, sizeof(pequeno)) == 0);

  // ---------- Resposta HTTP crua ----------
  conferir("HTTP com Content-Length",
           respostaCrua("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 29\r\n\r\n"
                        "{\"limite_db\":65,\"segundos\":5}", &l, &s) == ResultadoNuvem::OK &&
           l == 65 && s == 5);
  conferir("HTTP em pedacos (chunked)",
           respostaCrua("HTTP/1.1 200 OK\r\ntransfer-encoding: chunked\r\n\r\n"
                        "f\r\n{\"limite_db\":70\r\nf\r\n,\"segundos\":12}\r\n0\r\n\r\n", &l, &s) ==
               ResultadoNuvem::OK && l == 70 && s == 12);
  conferir("HTTP incompleto",
           respostaCrua("HTTP/1.1 200 OK\r\nContent-Length: 50\r\n\r\n{\"limite_db\":65", &l, &s) ==
           ResultadoNuvem::SEM_RESPOSTA);
  conferir("HTTP lixo",
           respostaCrua("lixo sem sentido", &l, &s) == ResultadoNuvem::SEM_RESPOSTA &&
           respostaCrua("HTTP/1.1 200 OK\r\nsem fim de cabecalho", &l, &s) == ResultadoNuvem::SEM_RESPOSTA);
  conferir("HTTP com Content-Length que nao e numero e recusado (nao vira 0)",
           respostaCrua("HTTP/1.1 200 OK\r\nContent-Length: abc\r\n\r\n{\"limite_db\":65,\"segundos\":5}",
                        &l, &s) == ResultadoNuvem::SEM_RESPOSTA);

  // ---------- Fim da resposta (conexão reaproveitada: o servidor não fecha) ----------
  {
    // Resposta típica de aparelho_sync com keep-alive, e a mesma em pedaços.
    static const char COMPRIMENTO[] =
        "HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: 29\r\n"
        "Connection: keep-alive\r\n\r\n{\"limite_db\":65,\"segundos\":5}";
    static const char PEDACOS[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
        "f\r\n{\"limite_db\":70\r\nf\r\n,\"segundos\":12}\r\n0\r\n\r\n";
    size_t total = 0;
    bool fecha = true;

    // Inteira: completa, com o tamanho certo e sem pedir para fechar.
    const size_t nC = strlen(COMPRIMENTO), nP = strlen(PEDACOS);
    conferir("fim: Content-Length completo (keep-alive, nao fecha)",
             nuvem_fim_da_resposta(COMPRIMENTO, nC, &total, &fecha) == FimHttp::COMPLETA &&
             total == nC && !fecha);
    conferir("fim: pedacos (chunked) completos",
             nuvem_fim_da_resposta(PEDACOS, nP, &total, &fecha) == FimHttp::COMPLETA &&
             total == nP && !fecha);

    // Qualquer pedaço do começo, por menor que seja, é INCOMPLETA: nunca
    // COMPLETA (leria JSON cortado) e nunca INVALIDA (jogaria fora uma resposta boa).
    bool soIncompletas = true;
    for (size_t i = 0; i < nC; i++) {
      if (nuvem_fim_da_resposta(COMPRIMENTO, i, &total, &fecha) != FimHttp::INCOMPLETA) soIncompletas = false;
    }
    for (size_t i = 0; i < nP; i++) {
      if (nuvem_fim_da_resposta(PEDACOS, i, &total, &fecha) != FimHttp::INCOMPLETA) soIncompletas = false;
    }
    conferir("fim: toda resposta cortada em qualquer ponto e INCOMPLETA", soIncompletas);

    // Bytes a mais depois do fim: o tamanho total diz onde ela acabou (n > total).
    char comSobra[400];
    snprintf(comSobra, sizeof(comSobra), "%sHTTP/1.1 200", COMPRIMENTO);
    conferir("fim: sobra depois da resposta e detectada",
             nuvem_fim_da_resposta(comSobra, strlen(comSobra), &total, &fecha) == FimHttp::COMPLETA &&
             total == nC && strlen(comSobra) > total);

    conferir("fim: Connection: close avisa para nao reaproveitar",
             fimDe("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 2\r\n\r\n{}", &total, &fecha) ==
                 FimHttp::COMPLETA && fecha);
    conferir("fim: sem tamanho e sem pedacos so termina fechando",
             fimDe("HTTP/1.1 200 OK\r\n\r\n{\"x\":1}", &total, &fecha) == FimHttp::INCOMPLETA && fecha);
    conferir("fim: 204 nao tem corpo",
             fimDe("HTTP/1.1 204 No Content\r\n\r\n", &total, &fecha) == FimHttp::COMPLETA &&
             total == strlen("HTTP/1.1 204 No Content\r\n\r\n"));
    conferir("fim: lixo, tamanho ruim e pedaco fora do formato sao INVALIDOS",
             fimDe("lixo sem sentido", &total, &fecha) == FimHttp::INVALIDA &&
             fimDe("HTTP/1.1 200 OK\r\nContent-Length: x\r\n\r\n", &total, &fecha) == FimHttp::INVALIDA &&
             fimDe("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n", &total, &fecha) ==
                 FimHttp::INVALIDA &&
             fimDe("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nabXX", &total, &fecha) ==
                 FimHttp::INVALIDA);
  }

  // ---------- Interpretação de aparelho_sync ----------
  l = s = -1;
  conferir("resposta valida (com espacos)",
           nuvem_interpretar_resposta(200, "{ \"limite_db\" : 80 , \"segundos\" : 60 }", &l, &s) ==
           ResultadoNuvem::OK && l == 80 && s == 60);
  l = s = -1;
  const bool fora = nuvem_interpretar_resposta(200, "{\"limite_db\":120,\"segundos\":5}", &l, &s) ==
                        ResultadoNuvem::FORA_DA_FAIXA &&
                    nuvem_interpretar_resposta(200, "{\"limite_db\":29,\"segundos\":5}", &l, &s) ==
                        ResultadoNuvem::FORA_DA_FAIXA &&
                    nuvem_interpretar_resposta(200, "{\"limite_db\":65,\"segundos\":0}", &l, &s) ==
                        ResultadoNuvem::FORA_DA_FAIXA &&
                    nuvem_interpretar_resposta(200, "{\"limite_db\":65,\"segundos\":61}", &l, &s) ==
                        ResultadoNuvem::FORA_DA_FAIXA;
  conferir("fora da faixa (30-100 dB, 1-60 s) e nao aplica", fora && l == -1 && s == -1);
  conferir("campo faltando",
           nuvem_interpretar_resposta(200, "{\"limite_db\":65}", &l, &s) == ResultadoNuvem::RESPOSTA_INVALIDA &&
           nuvem_interpretar_resposta(200, "{\"segundos\":5}", &l, &s) == ResultadoNuvem::RESPOSTA_INVALIDA);
  conferir("valor que nao e inteiro",
           nuvem_interpretar_resposta(200, "{\"limite_db\":65.5,\"segundos\":5}", &l, &s) ==
               ResultadoNuvem::RESPOSTA_INVALIDA &&
           nuvem_interpretar_resposta(200, "{\"limite_db\":\"65\",\"segundos\":5}", &l, &s) ==
               ResultadoNuvem::RESPOSTA_INVALIDA &&
           nuvem_interpretar_resposta(200, "{\"limite_db\":,\"segundos\":5}", &l, &s) ==
               ResultadoNuvem::RESPOSTA_INVALIDA);
  conferir("lixo no corpo",
           nuvem_interpretar_resposta(200, "<html>erro</html>", &l, &s) == ResultadoNuvem::RESPOSTA_INVALIDA &&
           nuvem_interpretar_resposta(200, "null", &l, &s) == ResultadoNuvem::RESPOSTA_INVALIDA &&
           nuvem_interpretar_resposta(200, "", &l, &s) == ResultadoNuvem::RESPOSTA_INVALIDA);
  conferir("aparelho_invalido (HTTP 400)",
           nuvem_interpretar_resposta(400, "{\"code\":\"P0001\",\"message\":\"aparelho_invalido\"}", &l, &s) ==
           ResultadoNuvem::APARELHO_INVALIDO);
  conferir("outro erro do servidor",
           nuvem_interpretar_resposta(500, "{\"message\":\"erro interno\"}", &l, &s) == ResultadoNuvem::ERRO_SERVIDOR &&
           nuvem_interpretar_resposta(401, "{\"message\":\"Invalid API key\"}", &l, &s) == ResultadoNuvem::ERRO_SERVIDOR);

  // ---------- Espera crescente ----------
  // Com o intervalo e o teto como parâmetros, o teste vale para 1 s e para 10 s
  // seja qual for o NUVEM_INTERVALO_MS do config.h.
  {
    uint32_t e = 1000;
    uint32_t seq[9];
    seq[0] = e;
    for (int i = 1; i <= 7; i++) seq[i] = e = nuvem_proxima_espera_com(e, false, 1000, 60000);
    seq[8] = nuvem_proxima_espera_com(e, true, 1000, 60000);
    conferir("espera com 1 s: 1 -> 2 -> 4 -> 8 -> 16 -> 32 -> 60 -> 60 s, e 1 s depois do sucesso",
             seq[0] == 1000 && seq[1] == 2000 && seq[2] == 4000 && seq[3] == 8000 &&
             seq[4] == 16000 && seq[5] == 32000 && seq[6] == 60000 && seq[7] == 60000 &&
             seq[8] == 1000);

    uint32_t d = 10000;
    uint32_t sd[6];
    sd[0] = d;
    for (int i = 1; i <= 4; i++) sd[i] = d = nuvem_proxima_espera_com(d, false, 10000, 60000);
    sd[5] = nuvem_proxima_espera_com(d, true, 10000, 60000);
    conferir("espera com 10 s: 10 -> 20 -> 40 -> 60 -> 60 s, e 10 s depois do sucesso",
             sd[0] == 10000 && sd[1] == 20000 && sd[2] == 40000 && sd[3] == 60000 &&
             sd[4] == 60000 && sd[5] == 10000);

    // Teto sem estourar o número, mesmo partindo de um valor enorme.
    conferir("espera nunca passa do teto (nem com valor enorme)",
             nuvem_proxima_espera_com(4000000000UL, false, 1000, 60000) == 60000 &&
             nuvem_proxima_espera_com(59999, false, 1000, 60000) == 60000);
  }
  {
    // O que o aparelho faz de verdade (usa o NUVEM_INTERVALO_MS do config.h).
    uint32_t e = NUVEM_INTERVALO_MS;
    bool crescente = true, noTeto = true;
    for (int i = 0; i < 12; i++) {
      const uint32_t prox = nuvem_proxima_espera(e, false);
      if (prox < e) crescente = false;
      if (prox > NUVEM_ESPERA_MAX_MS) noTeto = false;
      e = prox;
    }
    conferir("espera real: so cresce, respeita o teto e volta ao intervalo no sucesso",
             crescente && noTeto && e == NUVEM_ESPERA_MAX_MS &&
             nuvem_proxima_espera(e, true) == NUVEM_INTERVALO_MS);
  }

  // ---------- Próximo envio (um de cada vez, pulando ciclos perdidos) ----------
  {
    uint32_t pulados = 99;
    conferir("proximo envio: tentativa rapida, o ciclo seguinte normal",
             nuvem_proximo_envio(5000, 5300, 1000, &pulados) == 6000 && pulados == 0);
    conferir("proximo envio: tentativa que demorou 3,2 s pula 3 ciclos",
             nuvem_proximo_envio(5000, 8200, 1000, &pulados) == 9000 && pulados == 3);
    conferir("proximo envio: tentativa de exatos 1 s ja pula o ciclo que acabou de passar",
             nuvem_proximo_envio(5000, 6000, 1000, &pulados) == 7000 && pulados == 1);
    const uint32_t perto = 4294967295UL - 500;   // 0,5 s antes de o millis() dar a volta
    conferir("proximo envio: funciona na volta do millis()",
             nuvem_proximo_envio(perto, (uint32_t)(perto + 3200), 1000, &pulados) ==
                 (uint32_t)(perto + 4000) && pulados == 3);
    conferir("proximo envio: espera zero nao trava e ponteiro nulo e aceito",
             nuvem_proximo_envio(100, 150, 0, nullptr) == 150 &&
             nuvem_proximo_envio(100, 150, 1000, nullptr) == 1100);
  }

  // ---------- Maior nível desde o último envio ----------
  {
    MaximoJanelas m;
    m.somar(58.0f, true);
    m.somar(71.5f, true);    // o pico (uma palma de 0,1 s)
    m.somar(60.2f, true);
    const LeituraMaxima a = m.pegar();
    conferir("maior nivel: manda o pico das janelas, nao a ultima",
             a.valido && a.db == 71.5f && a.janelas == 3);

    const LeituraMaxima vazia = m.pegar();   // nada entrou depois da pergunta anterior
    conferir("maior nivel: depois de perguntar, recomeca do zero",
             !vazia.valido && vazia.janelas == 0);

    m.somar(90.0f, false);   // janela sem sinal conta como janela, mas nao como nivel
    m.somar(NAN, true);      // nivel que nao e numero tambem nao entra
    const LeituraMaxima semSinal = m.pegar();
    conferir("maior nivel: so janelas sem sinal dao 'sem nivel' (null no JSON)",
             !semSinal.valido && semSinal.janelas == 2);

    m.somar(0.0f, false);
    m.somar(64.0f, true);
    m.somar(0.0f, false);
    const LeituraMaxima mistura = m.pegar();
    conferir("maior nivel: janela sem sinal no meio nao derruba o maximo",
             mistura.valido && mistura.db == 64.0f && mistura.janelas == 3);

    // O estado vai junto do pico: nivel do pico + estado atual, no JSON.
    nuvem_montar_json("id", "k", a.db, a.valido, Estado::CONTANDO, json, sizeof(json));
    conferir("maior nivel: o pico entra no JSON com o estado atual",
             strcmp(json, "{\"p_id\":\"id\",\"p_chave\":\"k\",\"p_nivel\":71.5,\"p_estado\":\"contando\"}") == 0);
  }

  // ---------- Configuração ----------
  const char* motivo = "";
  char host[101];
  conferir("host da URL",
           nuvem_extrair_host("https://abcd.supabase.co", host, sizeof(host)) &&
           strcmp(host, "abcd.supabase.co") == 0 &&
           nuvem_extrair_host("https://abcd.supabase.co/", host, sizeof(host)) &&
           !nuvem_extrair_host("http://abcd.supabase.co", host, sizeof(host)) &&
           !nuvem_extrair_host("https://abcd.supabase.co/rest/v1", host, sizeof(host)) &&
           !nuvem_extrair_host("https://abcd.supabase.co:8443", host, sizeof(host)) &&
           !nuvem_extrair_host("https://", host, sizeof(host)));
  conferir("modelos (placeholders) desligam a nuvem",
           !nuvem_config_valida("COLE_AQUI_A_URL_DO_PROJETO", "sb_publishable_x", "abc", &motivo) &&
           !nuvem_config_valida("https://abcd.supabase.co", "COLE_AQUI_A_CHAVE_PUBLICA", "abc", &motivo) &&
           !nuvem_config_valida("https://abcd.supabase.co", "sb_publishable_x", "", &motivo) &&
           !nuvem_config_valida("https://abcd.supabase.co", "sb_publishable_x",
                                "COLE_AQUI_A_CHAVE_DO_APARELHO", &motivo));
  conferir("chave de administrador recusada",
           !nuvem_config_valida("https://abcd.supabase.co", "sb_secret_abc", "abc", &motivo) &&
           !nuvem_config_valida("https://abcd.supabase.co",
                                "eyJhbGciOiJIUzI1NiJ9.eyJyb2xlIjoic2VydmljZV9yb2xlIn0.assinatura", "abc",
                                &motivo));
  conferir("configuracao certa aceita",
           nuvem_config_valida("https://abcd.supabase.co", "sb_publishable_x", "0123abcd", &motivo) &&
           nuvem_config_valida("https://abcd.supabase.co",
                               "eyJhbGciOiJIUzI1NiJ9.eyJyb2xlIjoiYW5vbiJ9.assinatura", "0123abcd", &motivo));

  char mascara[48];
  nuvem_mascarar_chave("sb_publishable_ABCDEFGHIJKLMNOP", mascara, sizeof(mascara));
  const bool mascaraOk = strncmp(mascara, "sb_publishab...", 15) == 0 && strstr(mascara, "ABCDEF") == nullptr;
  nuvem_mascarar_chave("curta", mascara, sizeof(mascara));
  conferir("chave mascarada no Serial", mascaraOk && strcmp(mascara, "***") == 0);

  Serial.printf("[teste-nuvem] === %s (%d falha(s)) ===\n",
                falhas == 0 ? "TUDO PASS" : "HOUVE FAIL", falhas);
  return falhas == 0;
}

#else
bool teste_nuvem_rodar() { return true; }
#endif
