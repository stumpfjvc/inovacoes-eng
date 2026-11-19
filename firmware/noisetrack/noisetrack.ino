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
#include "teste_nivel_db.h"
#include "teste_regra.h"
#include "teste_tarefa.h"
#include "teste_prefs.h"
#include "teste_wifi.h"

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

// Lê uma cópia do estado da tarefa e imprime o que for novo.
static void imprimirEstado(uint32_t agora) {
  if (agora - tUltimaConsulta < CONSULTA_MS) return;
  tUltimaConsulta = agora;

  const EstadoCompartilhado e = tarefa_audio_copiar();

  if (e.contadorJanelas != janelasImpressas) {   // janela nova
    janelasImpressas = e.contadorJanelas;
    char textoA[16], textoB[16], textoComb[16];
    formatarDb(textoA, sizeof(textoA), e.dbA, e.validoA);
    formatarDb(textoB, sizeof(textoB), e.dbB, e.validoB);
    formatarDb(textoComb, sizeof(textoComb), e.dbCombinado, e.validoCombinado);
    Serial.printf("[nivel] A = %s | B = %s | comb = %s%s\n", textoA, textoB, textoComb,
                  e.canalDescartado ? " (canal sem sinal fora da conta)" : "");
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

  // Wi-Fi: só prepara. A conexão acontece aos poucos, em rede_atualizar().
  rede_iniciar();

  comandos_serial_iniciar();
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
  comandos_serial_atualizar();   // pode gravar na flash: por isso fica aqui, e não na tarefa
  rede_atualizar(agora);         // Wi-Fi, portal, mDNS e NTP: nunca espera a rede
  teste_tarefa_verificar(agora);
}
