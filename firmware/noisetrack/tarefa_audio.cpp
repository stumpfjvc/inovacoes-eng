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
// Maior nível desde a última pergunta da nuvem. Fica protegido pela mesma trava.
static MaximoJanelas       maximo;

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

// Um passa-alta por canal: o estado de cada um passa de uma janela para a outra.
#if FILTRO_PASSA_ALTA
static FiltroPassaAlta  filtroA;
static FiltroPassaAlta* const ptrFiltroA = &filtroA;
#if NUM_MICROFONES == 2
static FiltroPassaAlta  filtroB;
static FiltroPassaAlta* const ptrFiltroB = &filtroB;
#else
static FiltroPassaAlta* const ptrFiltroB = nullptr;
#endif
#else
static FiltroPassaAlta* const ptrFiltroA = nullptr;   // conta antiga, sem filtro
static FiltroPassaAlta* const ptrFiltroB = nullptr;
#endif

static bool configValida(float limiteDb, int tempoXs) {
  return limiteDb >= LIMITE_MIN_DB && limiteDb <= LIMITE_MAX_DB &&
         tempoXs >= TEMPO_X_MIN_S && tempoXs <= TEMPO_X_MAX_S;
}

// Nível de uma janela "sem sinal", usado quando o I2S falha várias vezes seguidas.
static NivelDb nivelSemSinal() {
  NivelDb n = {};
  n.dbA = n.dbB = n.dbCombinado = DB_PISO;
  n.dbfsA = n.dbfsB = DB_PISO - OFFSET_BASE_DB;   // o piso em dBFS
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
  // Uma comparação, com a trava que já está fechada. Janela do apito (em que se
  // republica a última da pausa) e aquecimento (sem sinal) entram do jeito que
  // foram publicadas: o apito do próprio buzzer nunca vira "pico" no site.
  maximo.somar(n.dbCombinado, n.validoCombinado);
  destravar();
}

#if DIAG_MIC
// Nível das janelas com apito e das janelas da pausa, para ver quanto o buzzer
// soma no microfone. diagRegistrar() roda a cada janela; diagnosticoMic() imprime.
static float    diagApitoMaxDb = 0.0f;   // maior nível com apito desde a última linha
static uint32_t diagApitoN     = 0;      // janelas com apito desde a última linha
static float    diagPausaDb    = 0.0f;   // nível da última janela da pausa
static bool     diagTemPausa   = false;

static void diagRegistrar(const NivelDb& n, bool contaminada) {
  if (!n.validoCombinado) return;
  if (contaminada) {
    if (diagApitoN == 0 || n.dbCombinado > diagApitoMaxDb) diagApitoMaxDb = n.dbCombinado;
    diagApitoN++;
  } else {
    diagPausaDb  = n.dbCombinado;
    diagTemPausa = true;
  }
}

// Diagnóstico temporário do mic A (config.h, DIAG_MIC). Só lê a janela e imprime.
// Usa a última janela lida a cada DIAG_MIC_PERIODO_MS. Os valores brutos já são os
// de 24 bits (audio.cpp deslocou 8 casas). Nenhum dos dBFS tem offset de calibração:
//   rms      = só tira a média da janela (a conta antiga, sem filtro);
//   filtrado = o dBFS que a medição usou (nivel.dbfsA), depois do passa-alta.
// O "rms" é recalculado aqui sem filtro: não mexe no estado do filtro.
static void diagnosticoMic(const int32_t* amostras, size_t n, uint32_t agora,
                           const NivelDb& nivel, bool aquecendo) {
  static uint32_t ultimo = 0;
  if ((uint32_t)(agora - ultimo) < DIAG_MIC_PERIODO_MS) return;
  ultimo = agora;

  int32_t minimo = amostras[0], maximo = amostras[0];
  int64_t soma = 0;
  for (size_t i = 0; i < n; i++) {
    if (amostras[i] < minimo) minimo = amostras[i];
    if (amostras[i] > maximo) maximo = amostras[i];
    soma += amostras[i];
  }
  const float media = (float)((double)soma / (double)n);

  bool valido = false;
  const float dbfs = nivel_dbfs(amostras, n, &valido);

#if FONTE_SIMULADA
  const char* canal = "simulado";
#else
  const char* canal = MIC_CANAIS_TROCADOS ? "direito" : "esquerdo";
#endif
  Serial.printf("[diag] mic A, canal %s: min=%ld max=%ld media=%.1f rms=%.1f dBFS",
                canal, (long)minimo, (long)maximo, media, dbfs);
#if FILTRO_PASSA_ALTA
  Serial.printf(" filtrado(%d Hz)=%.1f dBFS", FILTRO_HZ, nivel.dbfsA);
#else
  (void)nivel;
  Serial.print(" filtrado=desligado");
#endif
  Serial.printf("%s%s\n", valido ? "" : " (SEM VARIACAO)", aquecendo ? " [aquecendo]" : "");

  // Só aparece no último segundo em que o buzzer tocou (durante o alerta).
  if (diagApitoN > 0) {
    Serial.printf("[diag] apito: max %.1f dB (%lu janelas)", diagApitoMaxDb,
                  (unsigned long)diagApitoN);
    if (diagTemPausa) {
      Serial.printf(" | pausa: %.1f dB | diferenca %+.1f dB\n", diagPausaDb,
                    diagApitoMaxDb - diagPausaDb);
    } else {
      Serial.println(" | pausa: --");
    }
    diagApitoN = 0;
  }
}
#endif

// ---------- A tarefa ----------
// Como o run() de uma Thread do Java: um laço que nunca termina.
static void tarefaAudio(void*) {
  uint32_t   falhas          = 0;
  TickType_t ultimoDespertar = xTaskGetTickCount();
  NivelDb    nivelPausa      = nivelSemSinal();   // última janela que a regra pôde usar
  // Aquecimento conta a partir daqui (logo depois do audio_iniciar() no setup).
  // "aquecido" trava em true: quando millis() der a volta (~49 dias), não volta a aquecer.
  const uint32_t tInicioAudio = millis();
  bool           aquecido     = (AQUECIMENTO_MIC_MS == 0);

  for (;;) {
#if FONTE_SIMULADA
    // A fonte simulada responde na hora; este é o relógio dela: acorda a cada
    // JANELA_MS exatos, descontando o tempo gasto nas contas.
    vTaskDelayUntil(&ultimoDespertar, pdMS_TO_TICKS(JANELA_MS));
#endif
    // Fonte real: o relógio é a espera dentro do i2s_read (~125 ms por janela).
    const bool     leu   = audio_ler_janela(janelaA, ptrJanelaB);
    const uint32_t agora = millis();

    if (!aquecido && (uint32_t)(agora - tInicioAudio) >= AQUECIMENTO_MIC_MS) aquecido = true;

    NivelDb nivel;
    bool    contaminada = false;   // esta janela pode ter ouvido o próprio apito?
    if (leu) {
      falhas = 0;
      // O filtro roda em toda janela lida, inclusive no aquecimento (para assentar).
      nivel  = nivel_calcular(janelaA, ptrJanelaB, AMOSTRAS_POR_JANELA, ptrFiltroA, ptrFiltroB);
      // A janela cobre mais ou menos os últimos JANELA_MS antes de "agora".
      // Fica de fora se o buzzer esteve ligado nela ou desligou menos de
      // BUZZER_GUARDA_MS antes de ela começar (o som e o eco ainda no ar).
      // Precisa ser checado ANTES do buzzer_atualizar desta volta.
      contaminada = buzzer_tocou_desde(agora - JANELA_MS - BUZZER_GUARDA_MS);
#if DIAG_MIC
      diagRegistrar(nivel, contaminada);
      diagnosticoMic(janelaA, AMOSTRAS_POR_JANELA, agora, nivel, !aquecido);
#endif
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

    if (!aquecido) {
      // Aquecimento: a janela já passou pelo filtro, mas é descartada. A regra
      // não roda (não há como disparar alerta) e o nível publicado é "sem sinal".
      RegraSaida aquecendo = {};
      aquecendo.estado  = Estado::SEM_SINAL;
      aquecendo.mudou   = false;
      aquecendo.evento  = EventoRegra::NENHUM;
      aquecendo.acimaMs = 0;
      leds_mostrar(aquecendo.estado);
      buzzer_atualizar(false, agora);
      publicar(nivelSemSinal(), aquecendo, falhas);
#if !FONTE_SIMULADA
      if (!leu) vTaskDelay(pdMS_TO_TICKS(JANELA_MS));   // I2S parado: tenta de novo daqui a pouco
#endif
      continue;
    }

    if (contaminada) {
      // Apito ou guarda: a regra não vê esta janela. O estado e o tempo acima
      // ficam parados, e o nível publicado é o da última janela da pausa.
      // Assim a decisão de continuar ou sair do alerta usa só as pausas.
      RegraSaida parada = {};
      parada.estado  = regra.estado();
      parada.mudou   = false;
      parada.evento  = EventoRegra::NENHUM;
      parada.acimaMs = regra.acimaMs();
      leds_mostrar(parada.estado);
      buzzer_atualizar(parada.estado == Estado::ALERTA, agora);   // mantém o ritmo dos pulsos
      publicar(nivelPausa, parada, falhas);
      continue;
    }
    if (leu) nivelPausa = nivel;   // última janela limpa (da pausa ou fora do alerta)

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
  maximo = MaximoJanelas();

  trava = xSemaphoreCreateMutex();
  if (trava == nullptr) return false;

#if FILTRO_PASSA_ALTA
  filtroA.configurar(FILTRO_HZ, FREQ_AMOSTRAGEM_HZ);
#if NUM_MICROFONES == 2
  filtroB.configurar(FILTRO_HZ, FREQ_AMOSTRAGEM_HZ);
#endif
#endif

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

LeituraMaxima tarefa_audio_pegar_maximo() {
  if (trava == nullptr) return LeituraMaxima{0.0f, false, 0};   // tarefa ainda não criada
  travar();
  const LeituraMaxima r = maximo.pegar();
  destravar();
  return r;
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
