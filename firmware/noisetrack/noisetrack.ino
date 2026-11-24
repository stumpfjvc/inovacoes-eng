// NoiseTrack: firmware do medidor de ruído (ESP32)
// Ver README.md e o plano de implementação por partes.
//
// O alerta local (áudio, dB, regra, LEDs e buzzer) roda numa tarefa FreeRTOS
// própria (tarefa_audio.cpp). O loop() só imprime: nunca usa delay() nem
// espera rede, e cada coisa roda quando chega a hora dela, medida com millis().

#include "config.h"
#include "estado.h"
#include "leds.h"
#include "buzzer.h"
#include "audio.h"
#include "tarefa_audio.h"
#include "armazenamento.h"
#include "comandos_serial.h"
#include "rede.h"
#include "web.h"
#include "nuvem.h"
#include "teste_nivel_db.h"
#include "teste_regra.h"
#include "teste_tarefa.h"
#include "teste_prefs.h"
#include "teste_wifi.h"
#include "teste_web.h"
#include "teste_nuvem.h"
// Sempre incluído (só declarações): o PlatformIO cria protótipos das funções do
// .ino sem olhar os #if, e eles precisam conhecer o AcumuladorCalib.
#include "calibracao.h"

// De quanto em quanto tempo o loop() olha o estado publicado pela tarefa.
// 25 ms é bem menos que uma janela (125 ms), então nenhuma janela passa batida.
static constexpr uint32_t CONSULTA_MS = 25;

static uint32_t tUltimoBatimento = 0;
static uint32_t tUltimaConsulta  = 0;
static uint32_t janelasImpressas = 0;
static uint32_t eventosImpressos = 0;
static Estado   estadoImpresso   = Estado::CALMO;

// Escreve "xx.x dB" ou "--" (canal sem sinal ou ausente) em "texto".
static void formatarDb(char* texto, size_t tamanho, float db, bool valido) {
  if (valido) snprintf(texto, tamanho, "%.1f dB", db);
  else        snprintf(texto, tamanho, "--");
}

#if CALIBRACAO
// Somas do período de calibração atual (só o loop() mexe nelas).
static AcumuladorCalib calibA, calibB, calibComb;
static uint32_t        tUltimaCalib = 0;

// Escreve a média do período ("xx.x dB") ou "sem sinal" em "texto".
static void formatarMedia(char* texto, size_t tamanho, const AcumuladorCalib& a) {
  float media = 0.0f;
  if (calib_media(a, &media)) snprintf(texto, tamanho, "%.1f dB", media);
  else                        snprintf(texto, tamanho, "sem sinal");
}

// A cada CALIB_PERIODO_MS imprime as médias em energia e começa outro período.
static void imprimirCalibracao(uint32_t agora) {
  if (agora - tUltimaCalib < CALIB_PERIODO_MS) return;
  tUltimaCalib = agora;

  char textoA[16], textoB[16], textoComb[16];
  formatarMedia(textoA, sizeof(textoA), calibA);
  formatarMedia(textoB, sizeof(textoB), calibB);
  formatarMedia(textoComb, sizeof(textoComb), calibComb);
  Serial.printf("[calib] %lu s: A = %s | B = %s | comb = %s",
                (unsigned long)(CALIB_PERIODO_MS / 1000), textoA, textoB, textoComb);
  if (calibComb.janelas > 0) {
    Serial.printf(" (min %.1f, max %.1f)", calibComb.minDb, calibComb.maxDb);
  }
  Serial.println();

  calib_zerar(&calibA);
  calib_zerar(&calibB);
  calib_zerar(&calibComb);
}
#endif

// Lê uma cópia do estado da tarefa e imprime o que for novo.
static void imprimirEstado(uint32_t agora) {
  if (agora - tUltimaConsulta < CONSULTA_MS) return;
  tUltimaConsulta = agora;

  const EstadoCompartilhado e = tarefa_audio_copiar();

  if (e.contadorJanelas != janelasImpressas) {   // janela nova
    janelasImpressas = e.contadorJanelas;
#if CALIBRACAO
    // Cada janela entra uma vez só na média (o contador mudou = janela nova).
    calib_somar(&calibA, e.dbA, e.validoA);
    calib_somar(&calibB, e.dbB, e.validoB);
    calib_somar(&calibComb, e.dbCombinado, e.validoCombinado);
#else
    char textoA[16], textoB[16], textoComb[16];
    formatarDb(textoA, sizeof(textoA), e.dbA, e.validoA);
    formatarDb(textoB, sizeof(textoB), e.dbB, e.validoB);
    formatarDb(textoComb, sizeof(textoComb), e.dbCombinado, e.validoCombinado);
    Serial.printf("[nivel] A = %s | B = %s | comb = %s%s\n", textoA, textoB, textoComb,
                  e.canalDescartado ? " (canal sem sinal fora da conta)" : "");
#endif
  }

  if (e.estado != estadoImpresso) {   // o estado mudou
    estadoImpresso = e.estado;
    Serial.printf("[regra] estado=%s acima=%lu ms", nomeEstado(e.estado),
                  (unsigned long)e.acimaMs);
    if (e.contadorEventos != eventosImpressos) {
      eventosImpressos = e.contadorEventos;
      Serial.printf(" (%s)", nomeEvento(e.ultimoEvento));
    }
    if (e.falhasSeguidas >= FALHAS_ATE_SEM_SINAL) Serial.print(" [I2S sem resposta]");
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== NoiseTrack: firmware em desenvolvimento ===");
  Serial.printf("Limite padrao: %d dB | Tempo X padrao: %d s\n",
                LIMITE_PADRAO_DB, TEMPO_X_PADRAO_S);

  buzzer_iniciar();   // primeiro, para o buzzer não apitar durante o resto do setup
  leds_iniciar();

#if TESTE_NIVEL_DB
  teste_nivel_db_rodar();   // só imprime PASS/FAIL; não trava se falhar
#endif
#if TESTE_REGRA
  teste_regra_rodar();      // idem
#endif

  if (!audio_iniciar()) {
    // Segue mesmo assim: a tarefa vai falhar ao ler e cair em SEM_SINAL (LED azul).
    Serial.println("[audio] ERRO: fonte de audio nao iniciou");
  }

  // A partir daqui, só a tarefa mexe no áudio, nos LEDs e no buzzer.
  if (!tarefa_audio_iniciar()) {
    Serial.println("[tarefa] ERRO: nao foi possivel criar a tarefa de audio");
  }

  // Limite e X guardados na flash (ou os padrões) vão para a tarefa.
  armazenamento_iniciar();
#if TESTE_PREFS
  teste_prefs_rodar();      // grava valores de teste e restaura os originais
#endif
#if TESTE_WIFI
  teste_wifi_rodar();       // só a lógica (tempo simulado); não usa o Wi-Fi
#endif
#if TESTE_WEB
  teste_web_rodar();        // só a parte pura da página; não usa a rede
#endif
#if TESTE_NUVEM
  teste_nuvem_rodar();      // só a parte pura da nuvem; não usa a rede
#endif

  // Wi-Fi: só prepara. A conexão acontece aos poucos, em rede_atualizar().
  rede_iniciar();
  web_iniciar();            // a página só liga quando o Wi-Fi do local conectar
  nuvem_iniciar();          // depois de rede_iniciar (usa o ID); desliga sozinha se faltar chave

  comandos_serial_iniciar();
#if CALIBRACAO
  calib_zerar(&calibA);
  calib_zerar(&calibB);
  calib_zerar(&calibComb);
  tUltimaCalib = millis();
  Serial.printf("[calib] modo calibracao: media em energia a cada %lu s (sem linhas [nivel])\n",
                (unsigned long)(CALIB_PERIODO_MS / 1000));
#endif
  teste_tarefa_iniciar(millis());
}

void loop() {
  const uint32_t agora = millis();

  // Mensagem de "vivo": mostra que o loop continua rodando sem travar.
  if (agora - tUltimoBatimento >= BATIMENTO_MS) {
    tUltimoBatimento = agora;
    Serial.printf("[vivo] %lu ms\n", (unsigned long)agora);
  }

  imprimirEstado(agora);
#if CALIBRACAO
  imprimirCalibracao(agora);     // média dos últimos 5 s (só com CALIBRACAO 1)
#endif
  comandos_serial_atualizar();   // pode gravar na flash: por isso fica aqui, e não na tarefa
  rede_atualizar(agora);         // Wi-Fi, portal, mDNS e NTP: nunca espera a rede
  web_atualizar(agora);          // página, /status, /config e WebSocket
  nuvem_atualizar(agora);        // aplica o limite/X vindo da nuvem (o envio roda na tarefa dela)
  teste_tarefa_verificar(agora);
}
