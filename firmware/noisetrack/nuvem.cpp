// NoiseTrack: nuvem (Supabase). As decisões e contas ficam em nuvem_logica.cpp
// (puro e testado); aqui ficam a tarefa, o HTTPS e o que é impresso.
//
// Quem faz o quê (como threads em Java):
//   - tarefa da nuvem (PRIORIDADE 0, núcleo 0): a cada NUVEM_INTERVALO_MS (1 s)
//     monta o pedido, fala com o servidor (pode demorar alguns segundos) e guarda
//     o resultado. A conexão HTTPS fica aberta entre os envios (keep-alive).
//     Um envio de cada vez, sempre: se um demorar, os ciclos perdidos são pulados.
//     Prioridade 0 = a da tarefa ociosa: as duas se revezam a cada 1 ms, então o
//     aperto de mão TLS, que gasta segundos de CPU sem ceder, não impede a ociosa
//     de alimentar o watchdog (ver o porquê em config.h, PRIORIDADE_TAREFA_NUVEM);
//   - loop(): aplica e grava o limite/X recebido (a gravação na flash fica no
//     loop(), como no resto do projeto) e imprime o comando "nuvem";
//   - tarefa de áudio (prioridade 2, núcleo 1): não sabe que a nuvem existe.
#include "nuvem.h"
#include "config.h"

#if NUVEM_ATIVA
#include "nuvem_logica.h"
#include "nuvem_ca.h"
#include "tarefa_audio.h"
#include "armazenamento.h"
#include "rede.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "mbedtls/x509_crt.h"
#include <esp_freertos_hooks.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <string.h>
#include <time.h>

// A chave do aparelho vem do secrets.h (fora do Git). Sem ela, a nuvem desliga.
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef NUVEM_CHAVE_APARELHO
#define NUVEM_CHAVE_APARELHO ""
#endif

static constexpr uint16_t PORTA_HTTPS = 443;
static constexpr uint32_t PASSO_MS    = 100;   // a tarefa confere a hora a cada 0,1 s

// O que foi medido ao abrir uma conexão nova (tempos em ms).
struct MedidaConexao {
  uint32_t dnsMs       = 0;   // resolver o nome do servidor
  uint32_t conectarMs  = 0;   // conexão TCP + aperto de mão TLS (com verificação é a parte pesada)
  uint32_t datasMs     = 0;   // conferir a data dos certificados (só no TLS verificado)
  uint32_t ociosaMs    = 0;   // ~ms em que a tarefa ociosa do núcleo 0 conseguiu rodar durante a conexão
  uint32_t lacunaMs    = 0;   // maior tempo seguido sem a tarefa ociosa rodar (o watchdog reinicia em 5000)
  uint32_t pilhaLivre  = 0;   // pilha livre mínima da tarefa da nuvem, em bytes
  bool     ok          = false;
  bool     tem         = false;
};

// ---------- Estado compartilhado (tarefa da nuvem <-> loop) ----------
// Protegido por trava, como um "synchronized" do Java.
struct StatusNuvem {
  ResultadoNuvem ultimo        = ResultadoNuvem::NENHUM;
  int            ultimoHttp    = 0;
  uint32_t       tUltimaTentativa = 0;
  bool           teveSucesso   = false;
  uint32_t       tUltimoSucesso = 0;
  uint32_t       esperaMs      = NUVEM_INTERVALO_MS;
  uint32_t       tentativas    = 0;
  uint32_t       sucessos      = 0;
  uint32_t       conexoes      = 0;   // conexões TLS abertas (com keep-alive, quase sempre 1)
  uint32_t       ciclosPulados = 0;   // ciclos perdidos porque um envio demorou
  uint32_t       duracaoMs     = 0;   // quanto durou o último envio
  bool           temConfig     = false;   // já recebeu limite/X válidos
  int            limiteRecebido   = 0;
  int            segundosRecebidos = 0;
  bool           aguardandoHorario = false;   // TLS verificado esperando o NTP
  bool           temErroCert   = false;       // houve um certificado recusado
  uint32_t       tErroCert     = 0;
  char           erroCert[120] = "";          // o motivo, em texto (sem chaves)
  MedidaConexao  medidaPrimeira;              // a primeira conexão nova desde a partida
  MedidaConexao  medidaUltima;                // a mais recente
  uint32_t       medidasN      = 0;           // quantas conexões novas foram medidas
  uint32_t       lacunaMaxMs   = 0;           // pior lacuna da tarefa ociosa em qualquer conexão
  bool           temPrimeiroEnvio = false;
  uint32_t       primeiroEnvioMs  = 0;        // o primeiro envio que deu certo, do começo ao fim
  uint32_t       raizesMs      = 0;           // ler as raízes do nuvem_ca.h (TLS verificado)
  int            raizesN       = 0;
};

static SemaphoreHandle_t trava   = nullptr;
static TaskHandle_t      tarefa  = nullptr;
static StatusNuvem       status;
static bool              pendente = false;     // config nova para o loop() aplicar
static int               pendLimite = 0, pendSegundos = 0;
static volatile bool     wifiConectado = false;   // escrito pelo loop(), lido pela tarefa
static const char*       motivoDesligada = "";
static char              host[101];

static void travar()    { xSemaphoreTake(trava, portMAX_DELAY); }
static void destravar() { xSemaphoreGive(trava); }

// ---------- Só a tarefa da nuvem usa ----------
static WiFiClientSecure cliente;
static char             json[384];     // corpo do pedido
static char             pedido[1100];  // cabeçalho + corpo, enviados juntos numa escrita só
static char             bruto[3072];   // resposta HTTP crua (cabeçalhos + corpo)
static uint32_t         contConexoes = 0;
static char             erroCertTexto[120] = "";   // motivo do último certificado recusado
static bool             erroCertNovo = false;      // a tarefa copia para o status depois do envio

static void registrarErroCert(const char* texto) {
  snprintf(erroCertTexto, sizeof(erroCertTexto), "%s", texto);
  erroCertNovo = true;
  Serial.printf("[nuvem] certificado recusado: %s. Sem verificacao a nuvem NAO envia; "
                "o alarme local segue normal (ver nuvem_ca.h)\n", texto);
}

#if NUVEM_TLS_VALIDAR
// O mbedTLS desta plataforma não olha a data dos certificados, então conferimos
// aqui cada um que o servidor mandou, com o relógio do NTP (em UTC).
static bool conferirDatas() {
  const mbedtls_x509_crt* cert = cliente.getPeerCertificate();
  if (cert == nullptr) {
    registrarErroCert("o servidor nao mandou certificado");
    return false;
  }
  const time_t t = time(nullptr);
  struct tm u;
  gmtime_r(&t, &u);
  const DataHora agora = {u.tm_year + 1900, u.tm_mon + 1, u.tm_mday, u.tm_hour, u.tm_min, u.tm_sec};

  for (; cert != nullptr; cert = cert->next) {   // o do site, depois os intermediários
    const DataHora inicio = {cert->valid_from.year, cert->valid_from.mon, cert->valid_from.day,
                             cert->valid_from.hour, cert->valid_from.min, cert->valid_from.sec};
    const DataHora fim    = {cert->valid_to.year, cert->valid_to.mon, cert->valid_to.day,
                             cert->valid_to.hour, cert->valid_to.min, cert->valid_to.sec};
    const ValidadeCert v = nuvem_validade(inicio, fim, agora);
    if (v != ValidadeCert::OK) {
      char nome[64] = "";
      mbedtls_x509_dn_gets(nome, sizeof(nome), &cert->subject);
      char texto[120];
      snprintf(texto, sizeof(texto), "%s: %s (valido ate %04d-%02d-%02d)", nome,
               nuvem_nome_validade(v), fim.ano, fim.mes, fim.dia);
      registrarErroCert(texto);
      return false;
    }
  }
  return true;
}
#endif

// ---------- Medição do aperto de mão (tempo de cada etapa e folga do watchdog) ----------
// Maior tempo seguido sem a tarefa ociosa do núcleo 0 rodar. O watchdog reinicia a
// placa se ele passar de 5000 ms. O gancho só fica ligado durante o connect().
static int64_t           ociosaUltimaUs    = 0;
static volatile uint32_t ociosaLacunaMaxUs = 0;
static volatile uint32_t ociosaChamadas    = 0;

// Roda DENTRO da tarefa ociosa do núcleo 0: não pode bloquear nem demorar. Devolvendo
// true, o FreeRTOS chama este gancho uma vez por tick (1 ms) em que a ociosa roda, então
// o número de chamadas é, em ms, o tempo que ela conseguiu usar a CPU.
// (Este SDK não tem as estatísticas de CPU por tarefa, por isso a medição é assim.)
static bool ganchoOciosa0() {
  const int64_t t = esp_timer_get_time();
  if (ociosaUltimaUs != 0) {
    const int64_t d = t - ociosaUltimaUs;
    if (d > (int64_t)ociosaLacunaMaxUs) {
      ociosaLacunaMaxUs = d > 0xFFFFFFFFLL ? 0xFFFFFFFFu : (uint32_t)d;
    }
  }
  ociosaUltimaUs = t;
  ociosaChamadas = ociosaChamadas + 1;
  return true;   // true = uma vez por tick; false seria "sem parar" e gastaria CPU à toa
}

static MedidaConexao medidaNova;                  // preenchida por abrirConexao()...
static bool          medidaNovaPendente = false;  // ...e copiada para o status pela tarefa
static uint32_t      raizesMs = 0;
static int           raizesN  = 0;
static bool          raizesMedidas = false;

static void imprimirMedida(const char* rotulo, const MedidaConexao& m) {
  const unsigned long pct = m.conectarMs > 0 ? (unsigned long)((100UL * m.ociosaMs) / m.conectarMs) : 0UL;
  Serial.printf("[nuvem] %s (%s): DNS %lu ms | TCP+TLS %lu ms (a tarefa ociosa rodou ~%lu ms, %lu%%) | "
                "datas %lu ms | maior tempo sem a tarefa ociosa rodar: %lu ms (watchdog: 5000) | "
                "pilha livre minima %lu bytes\n",
                rotulo, m.ok ? "ok" : "falhou", (unsigned long)m.dnsMs, (unsigned long)m.conectarMs,
                (unsigned long)m.ociosaMs, pct, (unsigned long)m.datasMs,
                (unsigned long)m.lacunaMs, (unsigned long)m.pilhaLivre);
}

static void registrarMedida(MedidaConexao& m) {
  m.tem = true;
  medidaNova = m;
  medidaNovaPendente = true;
  imprimirMedida("conexao nova", m);
  if (m.lacunaMs >= 2500) {
    Serial.printf("[nuvem] AVISO: a tarefa ociosa ficou %lu ms sem rodar (o watchdog reinicia em 5000). "
                  "Confira a prioridade da tarefa da nuvem (tem que ser 0, ver config.h)\n",
                  (unsigned long)m.lacunaMs);
  }
}

#if NUVEM_TLS_VALIDAR
// Medida única: quanto custa interpretar as raízes do nuvem_ca.h. A biblioteca refaz
// isso a cada conexão nova. Serve para decidir, com número, se vale reduzir as raízes.
static void medirLeituraRaizes() {
  mbedtls_x509_crt c;
  mbedtls_x509_crt_init(&c);
  const int64_t t0 = esp_timer_get_time();
  const int r = mbedtls_x509_crt_parse(&c, (const unsigned char*)NUVEM_CA_RAIZ, strlen(NUVEM_CA_RAIZ) + 1);
  raizesMs = (uint32_t)((esp_timer_get_time() - t0) / 1000);
  raizesN  = 0;
  if (r >= 0) {
    for (const mbedtls_x509_crt* p = &c; p != nullptr && p->raw.p != nullptr; p = p->next) raizesN++;
  }
  mbedtls_x509_crt_free(&c);
  Serial.printf("[nuvem] ler as %d raizes do nuvem_ca.h (a biblioteca refaz isso a cada conexao nova): %lu ms\n",
                raizesN, (unsigned long)raizesMs);
}
#endif

// Abre a conexão HTTPS (aperto de mão TLS incluído). Bloqueia só a tarefa da nuvem.
// Com NUVEM_TLS_VALIDAR 1 nunca usa setInsecure(): se a verificação falhar, devolve
// ERRO_CERTIFICADO e a nuvem não envia.
// Mede cada etapa (DNS, TCP+TLS, datas) e a folga do watchdog: ver imprimirMedida().
static ResultadoNuvem abrirConexao() {
  MedidaConexao m;

#if NUVEM_TLS_VALIDAR
  if (!raizesMedidas) {
    raizesMedidas = true;
    medirLeituraRaizes();
  }
  cliente.setCACert(NUVEM_CA_RAIZ);   // raízes; o nome do servidor é conferido pelo mbedTLS
#else
  cliente.setInsecure();   // criptografa, mas NÃO confere o certificado (ver README)
#endif
  // Tempo máximo do aperto de mão. A biblioteca só confere entre duas etapas dele.
  cliente.setHandshakeTimeout((NUVEM_TIMEOUT_TLS_MS + 999) / 1000);   // em segundos

  // 1) DNS. Resolvido à parte só para medir; o connect() abaixo reaproveita a resposta.
  IPAddress ip;
  const uint32_t tDns = millis();
  const bool dnsOk = WiFi.hostByName(host, ip) == 1;
  m.dnsMs = (uint32_t)(millis() - tDns);
  if (!dnsOk) {
    Serial.printf("[nuvem] conexao falhou: nao achei o endereco de %s (DNS, %lu ms)\n",
                  host, (unsigned long)m.dnsMs);
    return ResultadoNuvem::SEM_CONEXAO;
  }

  // 2) TCP + aperto de mão TLS: onde o TLS verificado gasta CPU. Mede o tempo de relógio,
  // quanto a tarefa ociosa do núcleo 0 conseguiu rodar e o maior tempo em que ela não rodou.
  ociosaUltimaUs    = 0;
  ociosaLacunaMaxUs = 0;
  ociosaChamadas    = 0;
  const bool gancho = esp_register_freertos_idle_hook_for_cpu(ganchoOciosa0, 0) == ESP_OK;
  const int64_t  t0 = esp_timer_get_time();
  const bool conectou = cliente.connect(host, PORTA_HTTPS, (int32_t)NUVEM_TIMEOUT_MS);
  const int64_t  t1 = esp_timer_get_time();
  if (gancho) esp_deregister_freertos_idle_hook_for_cpu(ganchoOciosa0, 0);

  m.conectarMs = (uint32_t)((t1 - t0) / 1000);
  m.ociosaMs   = ociosaChamadas;
  // A lacuna do fim (desde a última passada da ociosa até agora) ainda não foi contada
  // pelo gancho; se ele nem chegou a rodar, a lacuna é a conexão inteira.
  uint32_t lacunaUs = ociosaLacunaMaxUs;
  const uint32_t lacunaFinalUs = (ociosaUltimaUs != 0) ? (uint32_t)(t1 - ociosaUltimaUs)
                                                       : (uint32_t)(t1 - t0);
  if (lacunaFinalUs > lacunaUs) lacunaUs = lacunaFinalUs;
  m.lacunaMs   = lacunaUs / 1000;
  m.pilhaLivre = (uint32_t)uxTaskGetStackHighWaterMark(nullptr);

  if (!conectou) {
    char erro[96] = "";
    const int codigo = cliente.lastError(erro, sizeof(erro));
    cliente.stop();
    m.ok = false;
    registrarMedida(m);
    if (NUVEM_TLS_VALIDAR && nuvem_erro_eh_certificado(codigo)) {
      char texto[120];
      snprintf(texto, sizeof(texto), "cadeia ou nome do servidor nao conferem (%s)", erro);
      registrarErroCert(texto);
      return ResultadoNuvem::ERRO_CERTIFICADO;
    }
    if (erro[0] != '\0') Serial.printf("[nuvem] conexao falhou: %s\n", erro);
    return ResultadoNuvem::SEM_CONEXAO;
  }

  // 3) Datas dos certificados (só no TLS verificado; o mbedTLS daqui não confere sozinho).
#if NUVEM_TLS_VALIDAR
  const uint32_t tDatas = millis();
  const bool datasOk = conferirDatas();
  m.datasMs = (uint32_t)(millis() - tDatas);
  if (!datasOk) {
    cliente.stop();
    m.ok = false;
    registrarMedida(m);
    return ResultadoNuvem::ERRO_CERTIFICADO;
  }
#endif
  contConexoes++;
  m.ok = true;
  registrarMedida(m);
  return ResultadoNuvem::OK;
}

// Joga fora bytes que tenham sobrado de uma resposta anterior (só acontece numa
// conexão reaproveitada). Sem isso, eles seriam lidos como início da próxima.
static void esvaziarEntrada() {
  uint8_t lixo[64];
  int disponivel;
  while ((disponivel = cliente.available()) > 0) {
    const int pedir = disponivel < (int)sizeof(lixo) ? disponivel : (int)sizeof(lixo);
    if (cliente.read(lixo, (size_t)pedir) <= 0) break;
  }
}

// Lê a resposta até ela ficar inteira (pelo Content-Length ou pelo último pedaço:
// o servidor NÃO fecha a conexão), até a conexão cair, o tempo acabar ou o
// buffer encher. Devolve quantos bytes chegaram.
static size_t lerResposta(bool* completa, bool* servidorFecha, size_t* tamTotal) {
  *completa = false;
  *servidorFecha = false;
  *tamTotal = 0;
  size_t n = 0;
  const uint32_t t0 = millis();
  while ((uint32_t)(millis() - t0) < NUVEM_TIMEOUT_RESPOSTA_MS && n < sizeof(bruto) - 1) {
    const int disponivel = cliente.available();
    if (disponivel > 0) {
      const size_t cabe  = sizeof(bruto) - 1 - n;
      const size_t pedir = (size_t)disponivel < cabe ? (size_t)disponivel : cabe;
      const int lidos = cliente.read((uint8_t*)bruto + n, pedir);
      if (lidos > 0) {
        n += (size_t)lidos;
        bruto[n] = '\0';
        const FimHttp f = nuvem_fim_da_resposta(bruto, n, tamTotal, servidorFecha);
        if (f == FimHttp::COMPLETA) { *completa = true; break; }
        if (f == FimHttp::INVALIDA) break;
      }
    } else if (!cliente.connected()) {
      break;
    } else {
      vTaskDelay(pdMS_TO_TICKS(5));   // nada chegou ainda: dá a vez para outras tarefas
    }
  }
  bruto[n] = '\0';
  return n;
}

// Um envio completo. Só existe UM por vez: quem chama é a tarefa da nuvem, e ela
// só começa o próximo depois que este termina (nunca há dois pedidos juntos).
// Bloqueia a TAREFA DA NUVEM por até alguns segundos; o alarme local continua
// rodando na outra tarefa, no outro núcleo.
static ResultadoNuvem sincronizar(int* http, int* limite, int* segundos) {
  *http = 0;

  // O MAIOR nível das janelas desde o envio anterior (a chamada já zera a conta)
  // e o estado de AGORA. Se por algum motivo nenhuma janela chegou, usa a atual.
  const LeituraMaxima pico = tarefa_audio_pegar_maximo();
  const EstadoCompartilhado e = tarefa_audio_copiar();   // cópia, com a trava da tarefa de áudio
  float nivel  = e.dbCombinado;
  bool  valido = e.validoCombinado;
  if (pico.janelas > 0) {
    nivel  = pico.db;
    valido = pico.valido;
  }

  const size_t tamJson = nuvem_montar_json(rede_id(), NUVEM_CHAVE_APARELHO, nivel, valido,
                                           e.estado, json, sizeof(json));
  if (tamJson == 0) return ResultadoNuvem::RESPOSTA_INVALIDA;

  // Pedido HTTP escrito à mão (o HTTPClient ocuparia mais flash), com cabeçalho e
  // corpo no mesmo buffer: uma escrita só, um pacote só.
  // "Connection: keep-alive": o servidor mantém a conexão aberta para o próximo envio.
  const int tamCab = snprintf(pedido, sizeof(pedido),
                              "POST /rest/v1/rpc/aparelho_sync HTTP/1.1\r\n"
                              "Host: %s\r\n"
                              "apikey: %s\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: %u\r\n"
                              "Connection: keep-alive\r\n\r\n",
                              host, NUVEM_CHAVE_PUBLICA, (unsigned)tamJson);
  if (tamCab <= 0 || (size_t)tamCab + tamJson >= sizeof(pedido)) {
    return ResultadoNuvem::RESPOSTA_INVALIDA;
  }
  memcpy(pedido + tamCab, json, tamJson);
  const size_t tamPedido = (size_t)tamCab + tamJson;

  // Até 2 tentativas, e só quando a conexão reaproveitada estava morta (o servidor
  // fechou enquanto esperávamos): aí abre uma nova e repete, sem esperar o próximo ciclo.
  bool   enviou = false, completa = false, servidorFecha = false;
  size_t n = 0, tamTotal = 0;
  for (int tentativa = 0; tentativa < 2; tentativa++) {
    const bool reaproveitada = cliente.connected();   // já verificada quando foi aberta
    if (!reaproveitada) {
      const ResultadoNuvem a = abrirConexao();
      if (a != ResultadoNuvem::OK) return a;   // sem conexão ou certificado recusado
    }
    esvaziarEntrada();

    enviou = cliente.write((const uint8_t*)pedido, tamPedido) == tamPedido;
    n = enviou ? lerResposta(&completa, &servidorFecha, &tamTotal) : 0;

    if ((!enviou || n == 0) && reaproveitada) {   // conexão velha: fecha e tenta com uma nova
      cliente.stop();
      continue;
    }
    break;
  }
  if (!enviou) {
    cliente.stop();
    return ResultadoNuvem::SEM_CONEXAO;
  }

  // Só mantém a conexão aberta se a resposta chegou inteira, sem sobras e sem o
  // servidor ter pedido para fechar. Qualquer dúvida: fecha (o próximo envio reconecta).
  if (!completa || servidorFecha || n > tamTotal) cliente.stop();

  char corpo[512];
  int codigo = 0;
  if (!nuvem_ler_resposta_http(bruto, n, &codigo, corpo, sizeof(corpo))) {
    cliente.stop();   // resposta quebrada: não reaproveita
    return ResultadoNuvem::SEM_RESPOSTA;
  }
  *http = codigo;
  return nuvem_interpretar_resposta(codigo, corpo, limite, segundos);
}

static void tarefaNuvem(void*) {
  uint32_t       espera         = NUVEM_INTERVALO_MS;
  uint32_t       proximo        = 0;       // instante do próximo envio
  bool           primeira       = true;    // manda já, sem esperar o intervalo
  bool           semWifiTratado = false;
  ResultadoNuvem anterior       = ResultadoNuvem::NENHUM;
#if NUVEM_TLS_VALIDAR
  bool           esperandoHora  = false;   // TLS verificado aguardando o NTP
  uint32_t       tInicioEspera  = 0;
  bool           avisouDemora   = false;
#endif

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(PASSO_MS));   // espera DENTRO da tarefa da nuvem (o loop() não espera)
    if (!wifiConectado) {                  // sem Wi-Fi do local (ou no portal): nem tenta
      if (!semWifiTratado) {
        cliente.stop();                    // a conexão velha era de outra rede: não serve mais
        semWifiTratado = true;
      }
      primeira = true;                     // quando o Wi-Fi voltar, manda logo
      continue;
    }
    semWifiTratado = false;

#if NUVEM_TLS_VALIDAR
    // Certificado depende da data: só conecta DEPOIS do "[ntp] horario acertado".
    // O NTP tenta sozinho de novo; aqui só esperamos, sem travar nada.
    if (!rede_horario_acertado()) {
      if (!esperandoHora) {
        esperandoHora = true;
        avisouDemora  = false;
        tInicioEspera = millis();
        travar(); status.aguardandoHorario = true; destravar();
        Serial.println("[nuvem] aguardando o horario do NTP para conferir o certificado "
                       "(o alarme local funciona normalmente)");
      } else if (!avisouDemora && (uint32_t)(millis() - tInicioEspera) >= 60000) {
        avisouDemora = true;
        Serial.println("[nuvem] ainda sem horario do NTP ha 60 s (sem internet?); "
                       "a nuvem continua esperando");
      }
      continue;
    }
    if (esperandoHora) {
      esperandoHora = false;
      travar(); status.aguardandoHorario = false; destravar();
      Serial.println("[nuvem] horario acertado: conectando com TLS verificado");
      primeira = true;
    }
#endif
    const uint32_t inicio = millis();
    if (!primeira && (int32_t)(inicio - proximo) < 0) continue;   // ainda não é a hora
    primeira = false;

    int http = 0, limite = 0, segundos = 0;
    const ResultadoNuvem r = sincronizar(&http, &limite, &segundos);
    const bool ok = (r == ResultadoNuvem::OK);
    const uint32_t fim = millis();

    espera = nuvem_proxima_espera(espera, ok);
    uint32_t pulados = 0;
    proximo = nuvem_proximo_envio(inicio, fim, espera, &pulados);   // pula os ciclos perdidos

    travar();
    status.ultimo           = r;
    status.ultimoHttp       = http;
    status.tUltimaTentativa = inicio;
    status.esperaMs         = espera;
    status.tentativas++;
    status.conexoes         = contConexoes;
    status.ciclosPulados   += pulados;
    status.duracaoMs        = (uint32_t)(fim - inicio);
    if (medidaNovaPendente) {                     // houve conexão nova neste envio
      medidaNovaPendente = false;
      status.medidasN++;
      status.medidaUltima = medidaNova;
      if (!status.medidaPrimeira.tem) status.medidaPrimeira = medidaNova;
      if (medidaNova.lacunaMs > status.lacunaMaxMs) status.lacunaMaxMs = medidaNova.lacunaMs;
    }
    status.raizesMs = raizesMs;
    status.raizesN  = raizesN;
    if (erroCertNovo) {
      erroCertNovo       = false;
      status.temErroCert = true;
      status.tErroCert   = fim;
      memcpy(status.erroCert, erroCertTexto, sizeof(status.erroCert));
    }
    if (ok) {
      if (!status.temPrimeiroEnvio) {             // o primeiro, com a conexão e tudo
        status.temPrimeiroEnvio = true;
        status.primeiroEnvioMs  = (uint32_t)(fim - inicio);
      }
      status.teveSucesso       = true;
      status.tUltimoSucesso    = fim;
      status.sucessos++;
      status.temConfig         = true;
      status.limiteRecebido    = limite;
      status.segundosRecebidos = segundos;
      pendente     = true;   // o loop() compara com o atual e grava se mudou
      pendLimite   = limite;
      pendSegundos = segundos;
    }
    destravar();

    // Imprime só quando muda (o primeiro sucesso e cada falha), para não
    // encher o Serial com um "ok" por segundo.
    if (!ok) {
      Serial.printf("[nuvem] falha: %s", nuvem_nome_resultado(r));
      if (http != 0) Serial.printf(" (HTTP %d)", http);
      Serial.printf(" | proxima tentativa em %.1f s\n", espera / 1000.0f);
      if (r == ResultadoNuvem::APARELHO_INVALIDO) {
        Serial.println("[nuvem] o banco nao reconhece este ID/chave: confira o NUVEM_CHAVE_APARELHO "
                       "do secrets.h e se o SQL do novo_aparelho.sh foi rodado no Supabase");
      }
    } else if (anterior != ResultadoNuvem::OK) {
      Serial.printf("[nuvem] ok: enviado e recebido limite %d dB, X %d s | este envio levou %lu ms "
                    "(com a conexao, se foi nova) | de novo a cada %lu ms\n",
                    limite, segundos, (unsigned long)(fim - inicio), (unsigned long)NUVEM_INTERVALO_MS);
    }
    anterior = r;
  }
}

// ---------- Funções públicas ----------
void nuvem_iniciar() {
  const char* motivo = "";
  if (!nuvem_config_valida(NUVEM_URL, NUVEM_CHAVE_PUBLICA, NUVEM_CHAVE_APARELHO, &motivo)) {
    motivoDesligada = motivo;
    Serial.printf("[nuvem] desligada: %s\n", motivo);
    return;
  }
#if NUVEM_TLS_VALIDAR
  if (strstr(NUVEM_CA_RAIZ, "BEGIN CERTIFICATE") == nullptr) {
    motivoDesligada = "NUVEM_TLS_VALIDAR 1, mas o nuvem_ca.h ainda nao tem o certificado";
    Serial.printf("[nuvem] desligada: %s\n", motivoDesligada);
    return;
  }
#endif
  nuvem_extrair_host(NUVEM_URL, host, sizeof(host));   // já validado acima

  trava = xSemaphoreCreateMutex();
  if (trava == nullptr) {
    motivoDesligada = "sem memoria para a trava";
    Serial.println("[nuvem] desligada: sem memoria para a trava");
    return;
  }
  const BaseType_t ok = xTaskCreatePinnedToCore(tarefaNuvem, "nuvem", PILHA_TAREFA_NUVEM, nullptr,
                                                PRIORIDADE_TAREFA_NUVEM, &tarefa, NUCLEO_TAREFA_NUVEM);
  if (ok != pdPASS) {
    tarefa = nullptr;
    motivoDesligada = "nao foi possivel criar a tarefa";
    Serial.println("[nuvem] desligada: nao foi possivel criar a tarefa");
    return;
  }

  char mascara[48];
  nuvem_mascarar_chave(NUVEM_CHAVE_PUBLICA, mascara, sizeof(mascara));
  Serial.printf("[nuvem] ligada: %s | ID %s | chave publica %s | a cada %lu ms com o Wi-Fi conectado\n",
                host, rede_id(), mascara, (unsigned long)NUVEM_INTERVALO_MS);
  Serial.printf("[nuvem] tarefa: nucleo %d, prioridade %u (a mesma da tarefa ociosa: elas se revezam e o "
                "watchdog de 5 s continua ligado) | limite do aperto de mao TLS: %lu ms\n",
                (int)NUCLEO_TAREFA_NUVEM, (unsigned)PRIORIDADE_TAREFA_NUVEM, (unsigned long)NUVEM_TIMEOUT_TLS_MS);
#if NUVEM_TLS_VALIDAR
  int raizes = 0;
  for (const char* p = NUVEM_CA_RAIZ; (p = strstr(p, "BEGIN CERTIFICATE")) != nullptr; p++) raizes++;
  Serial.printf("[nuvem] TLS verificado: cadeia ate uma das %d raizes do nuvem_ca.h, nome do servidor "
                "e data (so depois do NTP). Se nao conferir, nao envia.\n", raizes);
#else
  Serial.println("[nuvem] AVISO: HTTPS SEM verificacao do certificado (NUVEM_TLS_VALIDAR 0): "
                 "a conexao e criptografada, mas o aparelho nao confere se fala mesmo com o "
                 "Supabase. Ver nuvem_ca.h para ligar a verificacao.");
#endif
}

void nuvem_atualizar(uint32_t agora) {
  (void)agora;
  if (tarefa == nullptr) return;
  // Só como cliente do Wi-Fi do local. No portal (AP de setup) a rede não é CONECTADO.
  wifiConectado = (rede_estado() == EstadoRede::CONECTADO);

  travar();
  const bool temPendente = pendente;
  const int  limite = pendLimite, segundos = pendSegundos;
  pendente = false;
  destravar();
  if (!temPendente) return;

  // A nuvem manda: se for diferente do que está em uso, aplica e grava.
  const EstadoCompartilhado e = tarefa_audio_copiar();
  const int limiteAtual = (int)e.limiteDb, segundosAtual = e.tempoXs;
  if (limite == limiteAtual && segundos == segundosAtual) return;   // igual: nem toca na flash

  const ResultadoSalvar r = armazenamento_salvar(limite, segundos);   // mesma função da página
  Serial.printf("[nuvem] configuracao da nuvem aplicada: limite %d -> %d dB, X %d -> %d s (%s)\n",
                limiteAtual, limite, segundosAtual, segundos,
                !r.aceito ? "recusada" : (r.erroFlash ? "ERRO ao gravar" : "gravada"));
}

void nuvem_imprimir_status() {
  if (tarefa == nullptr) {
    Serial.printf("[nuvem] desligada: %s\n", motivoDesligada[0] != '\0' ? motivoDesligada : "?");
    return;
  }
  travar();
  const StatusNuvem s = status;
  destravar();
  const uint32_t agora = millis();

  Serial.printf("[nuvem] ligada | ID %s | servidor %s | %s\n", rede_id(), host,
                NUVEM_TLS_VALIDAR ? "TLS verificado (cadeia + nome + data)"
                                  : "TLS SEM verificacao (NUVEM_TLS_VALIDAR 0)");
#if NUVEM_TLS_VALIDAR
  Serial.printf("[nuvem] horario (NTP): %s", s.aguardandoHorario ? "aguardando (a nuvem espera)"
                                                                 : (rede_horario_acertado() ? "acertado" : "ainda nao"));
  if (s.temErroCert) {
    Serial.printf(" | ultimo erro de certificado ha %lu s: %s\n",
                  (unsigned long)((uint32_t)(agora - s.tErroCert) / 1000), s.erroCert);
  } else {
    Serial.println(" | ultimo erro de certificado: nenhum");
  }
#endif
  Serial.printf("[nuvem] Wi-Fi do local: %s | ultimo resultado: %s", wifiConectado ? "conectado" : "desconectado",
                nuvem_nome_resultado(s.ultimo));
  if (s.ultimoHttp != 0) Serial.printf(" (HTTP %d)", s.ultimoHttp);
  if (s.ultimo != ResultadoNuvem::NENHUM) {
    Serial.printf(" ha %lu s", (unsigned long)((uint32_t)(agora - s.tUltimaTentativa) / 1000));
  }
  Serial.println();
  if (s.teveSucesso) {
    Serial.printf("[nuvem] ultimo sucesso: ha %lu s", (unsigned long)((uint32_t)(agora - s.tUltimoSucesso) / 1000));
  } else {
    Serial.print("[nuvem] ultimo sucesso: nenhum ainda");
  }
  Serial.printf(" | espera atual: %.1f s | tentativas %lu, sucessos %lu\n",
                s.esperaMs / 1000.0f, (unsigned long)s.tentativas, (unsigned long)s.sucessos);
  Serial.printf("[nuvem] conexoes TLS abertas: %lu (com keep-alive fica perto de 1) | "
                "ultimo envio levou %lu ms | ciclos pulados: %lu | intervalo %lu ms\n",
                (unsigned long)s.conexoes, (unsigned long)s.duracaoMs,
                (unsigned long)s.ciclosPulados, (unsigned long)NUVEM_INTERVALO_MS);
  // Medidas do aperto de mão (para conferir a folga do watchdog: GUIA_BANCADA.md).
  if (s.medidaPrimeira.tem) imprimirMedida("primeira conexao", s.medidaPrimeira);
  if (s.medidaUltima.tem && s.medidasN > 1) imprimirMedida("ultima conexao", s.medidaUltima);
  Serial.printf("[nuvem] primeiro envio que deu certo: ");
  if (s.temPrimeiroEnvio) Serial.printf("%lu ms", (unsigned long)s.primeiroEnvioMs);
  else                    Serial.print("nenhum ainda");
  Serial.printf(" | pior tempo sem a tarefa ociosa rodar: %lu ms de 5000 (watchdog) | "
                "tarefa da nuvem: nucleo %d, prioridade %u\n",
                (unsigned long)s.lacunaMaxMs, (int)NUCLEO_TAREFA_NUVEM, (unsigned)PRIORIDADE_TAREFA_NUVEM);
#if NUVEM_TLS_VALIDAR
  if (s.raizesN > 0) {
    Serial.printf("[nuvem] ler as %d raizes do nuvem_ca.h: %lu ms (a biblioteca refaz a cada conexao nova)\n",
                  s.raizesN, (unsigned long)s.raizesMs);
  }
#endif
  const EstadoCompartilhado e = tarefa_audio_copiar();
  if (s.temConfig) {
    Serial.printf("[nuvem] configuracao recebida: limite %d dB, X %d s | em uso: limite %d dB, X %d s\n",
                  s.limiteRecebido, s.segundosRecebidos, (int)e.limiteDb, e.tempoXs);
  } else {
    Serial.printf("[nuvem] configuracao recebida: nenhuma ainda | em uso: limite %d dB, X %d s\n",
                  (int)e.limiteDb, e.tempoXs);
  }
  Serial.printf("[nuvem] pilha livre minima da tarefa: %lu de %lu bytes | memoria livre %lu (minima %lu)\n",
                (unsigned long)uxTaskGetStackHighWaterMark(tarefa), (unsigned long)PILHA_TAREFA_NUVEM,
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap());
}

#else
// NUVEM_ATIVA 0: nada é compilado (nem o HTTPS).
void nuvem_iniciar() { Serial.println("[nuvem] desligada (NUVEM_ATIVA 0)"); }
void nuvem_atualizar(uint32_t) {}
void nuvem_imprimir_status() { Serial.println("[nuvem] desligada (NUVEM_ATIVA 0)"); }
#endif
