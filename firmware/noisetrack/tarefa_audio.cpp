// NoiseTrack: tarefa FreeRTOS do alerta local (Parte 7).
#include "tarefa_audio.h"
#include "config.h"
#include "audio.h"
#include "nivel_db.h"
#include "leds.h"
#include "buzzer.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// ---------- Estado compartilhado ----------
// A tarefa escreve nesta struct e o loop() lê. Sem proteção, o loop() poderia
// ler no meio de uma escrita (o dB já novo e o estado ainda velho, por exemplo).
// A trava (mutex) faz o papel do "synchronized" do Java: quem pega a trava
// usa a struct sozinho; o outro espera até ela ser devolvida.
static SemaphoreHandle_t   trava = nullptr;
static EstadoCompartilhado compartilhado;
static TaskHandle_t        tarefa = nullptr;

// Equivalente a "synchronized (trava) { ... }".
static void travar()    { xSemaphoreTake(trava, portMAX_DELAY); }
static void destravar() { xSemaphoreGive(trava); }

// ---------- Dados que só a tarefa usa (sem trava) ----------
static int32_t janelaA[AMOSTRAS_POR_JANELA];
#if NUM_MICROFONES == 2
static int32_t  janelaB[AMOSTRAS_POR_JANELA];
static int32_t* const ptrJanelaB = janelaB;
#else
static int32_t* const ptrJanelaB = nullptr;   // com 1 microfone, não há canal B
#endif
static Regra regra;

static bool configValida(float limiteDb, int tempoXs) {
  return limiteDb >= LIMITE_MIN_DB && limiteDb <= LIMITE_MAX_DB &&
         tempoXs >= TEMPO_X_MIN_S && tempoXs <= TEMPO_X_MAX_S;
}

// Nível de uma janela "sem sinal", usado quando o I2S falha várias vezes seguidas.
static NivelDb nivelSemSinal() {
  NivelDb n = {};
  n.dbA = n.dbB = n.dbCombinado = DB_PISO;
  n.validoA = n.validoB = n.validoCombinado = false;
  n.canalDescartado = false;
  return n;
}

static void publicar(const NivelDb& n, const RegraSaida& r, uint32_t falhas) {
  travar();
  compartilhado.dbA             = n.dbA;
  compartilhado.dbB             = n.dbB;
  compartilhado.dbCombinado     = n.dbCombinado;
  compartilhado.validoA         = n.validoA;
  compartilhado.validoB         = n.validoB;
  compartilhado.validoCombinado = n.validoCombinado;
  compartilhado.canalDescartado = n.canalDescartado;
  compartilhado.estado          = r.estado;
  compartilhado.acimaMs         = r.acimaMs;
  if (r.evento != EventoRegra::NENHUM) {
    compartilhado.ultimoEvento         = r.evento;
    compartilhado.instanteUltimoEvento = r.instanteEvento;
    compartilhado.contadorEventos++;
  }
  compartilhado.contadorJanelas++;
  compartilhado.falhasSeguidas = falhas;
  destravar();
}

// ---------- A tarefa ----------
// Como o run() de uma Thread do Java: um laço que nunca termina.
static void tarefaAudio(void*) {
  uint32_t   falhas          = 0;
  TickType_t ultimoDespertar = xTaskGetTickCount();

  for (;;) {
#if FONTE_SIMULADA
    // A fonte simulada responde na hora; este é o relógio dela: acorda a cada
    // JANELA_MS exatos, descontando o tempo gasto nas contas.
    vTaskDelayUntil(&ultimoDespertar, pdMS_TO_TICKS(JANELA_MS));
#endif
    // Fonte real: o relógio é a espera dentro do i2s_read (~125 ms por janela).
    const bool     leu   = audio_ler_janela(janelaA, ptrJanelaB);
    const uint32_t agora = millis();

    NivelDb nivel;
    if (leu) {
      falhas = 0;
      nivel  = nivel_calcular(janelaA, ptrJanelaB, AMOSTRAS_POR_JANELA);
    } else {
      if (falhas < FALHAS_ATE_SEM_SINAL) falhas++;
      if (falhas < FALHAS_ATE_SEM_SINAL) {
        // Falha isolada: mantém o estado como está e só segura o ritmo do bipe.
        buzzer_atualizar(regra.estado() == Estado::ALERTA, agora);
        vTaskDelay(pdMS_TO_TICKS(JANELA_MS));   // não girar em falso se a leitura falhar na hora
        continue;
      }
      nivel = nivelSemSinal();   // N falhas seguidas: trata como sem sinal
    }

    // Limite e X atuais (podem ter sido mudados pela Parte 8 ou pela página).
    travar();
    const float limiteDb = compartilhado.limiteDb;
    const int   tempoXs  = compartilhado.tempoXs;
    destravar();

    const RegraSaida r = regra.atualizar(nivel.dbCombinado, nivel.validoCombinado, agora,
                                         limiteDb, tempoXs);
    // Só esta tarefa mexe nos LEDs e no buzzer, então eles não precisam de trava.
    leds_mostrar(r.estado);
    buzzer_atualizar(r.estado == Estado::ALERTA, agora);
    publicar(nivel, r, falhas);

#if !FONTE_SIMULADA
    if (!leu) vTaskDelay(pdMS_TO_TICKS(JANELA_MS));   // I2S parado: tenta de novo daqui a pouco
#endif
  }
}

// ---------- Funções públicas ----------
bool tarefa_audio_iniciar() {
  compartilhado = {};
  compartilhado.dbA = compartilhado.dbB = compartilhado.dbCombinado = DB_PISO;
  compartilhado.estado       = Estado::CALMO;
  compartilhado.limiteDb     = LIMITE_PADRAO_DB;
  compartilhado.tempoXs      = TEMPO_X_PADRAO_S;
  compartilhado.ultimoEvento = EventoRegra::NENHUM;

  trava = xSemaphoreCreateMutex();
  if (trava == nullptr) return false;

  const BaseType_t ok = xTaskCreatePinnedToCore(
      tarefaAudio, "audio", PILHA_TAREFA_AUDIO, nullptr,
      PRIORIDADE_TAREFA_AUDIO, &tarefa, NUCLEO_TAREFA_AUDIO);
  return ok == pdPASS;
}

EstadoCompartilhado tarefa_audio_copiar() {
  if (trava == nullptr) return compartilhado;   // tarefa ainda não criada
  travar();
  const EstadoCompartilhado copia = compartilhado;   // copia a struct inteira
  destravar();
  return copia;
}

bool tarefa_audio_definir_config(float limiteDb, int tempoXs) {
  if (trava == nullptr || !configValida(limiteDb, tempoXs)) return false;
  travar();
  compartilhado.limiteDb = limiteDb;
  compartilhado.tempoXs  = tempoXs;
  destravar();
  return true;
}

uint32_t tarefa_audio_pilha_livre_min() {
  // No ESP32 este número já vem em bytes (no FreeRTOS original viria em palavras).
  return tarefa == nullptr ? 0 : (uint32_t)uxTaskGetStackHighWaterMark(tarefa);
}
