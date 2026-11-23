// NoiseTrack: autoteste da regra dos X segundos. Simula o tempo janela a janela.
#include "teste_regra.h"
#include "config.h"

#if TESTE_REGRA
#include "regra.h"

static constexpr float LIMITE = 65.0f;   // limite usado nos testes
static constexpr int   X_S    = 5;       // tempo X usado nos testes
static constexpr float ALTO   = 70.0f;   // acima do limite
static constexpr float BAIXO  = 50.0f;   // abaixo de limite - histerese
static constexpr float FAIXA  = 63.0f;   // entre limite - histerese (62) e limite (65)

// Uma "simulação": a regra, o relógio falso e o que aconteceu até agora.
struct Simulacao {
  Regra    regra;
  uint32_t agora        = 0;
  bool     disparou     = false;
  uint32_t tDisparou    = 0;
  bool     normalizou   = false;
  uint32_t tNormalizou  = 0;
  bool     perdeuSinal  = false;
  bool     voltouACalmo = false;   // passou por CALMO vindo de outro estado
};

// Alimenta a regra com o mesmo nível por "duracaoMs", uma janela por vez,
// avançando o relógio falso JANELA_MS a cada passo.
static void rodar(Simulacao& sim, uint32_t duracaoMs, float nivel, bool valido = true,
                  float limite = LIMITE, int xS = X_S) {
  for (uint32_t t = 0; t < duracaoMs; t += JANELA_MS) {
    const RegraSaida s = sim.regra.atualizar(nivel, valido, sim.agora, limite, xS);
    if (s.evento == EventoRegra::DISPAROU && !sim.disparou) {
      sim.disparou = true;
      sim.tDisparou = s.instanteEvento;
    }
    if (s.evento == EventoRegra::NORMALIZOU && !sim.normalizou) {
      sim.normalizou = true;
      sim.tNormalizou = s.instanteEvento;
    }
    if (s.evento == EventoRegra::PERDEU_SINAL) sim.perdeuSinal = true;
    if (s.mudou && s.estado == Estado::CALMO) sim.voltouACalmo = true;
    sim.agora += JANELA_MS;
  }
}

static int falhas = 0;

static void resultado(const char* nome, bool ok, const char* detalhe) {
  if (!ok) falhas++;
  Serial.printf("[teste-regra] %s: %s (%s)\n", nome, ok ? "PASS" : "FAIL", detalhe);
}

// |a - b| <= tolerancia, para tempos em ms.
static bool perto(uint32_t a, uint32_t b, uint32_t tolerancia) {
  return (a > b ? a - b : b - a) <= tolerancia;
}

// Caso b, a partir de um instante inicial qualquer (usado também pelo caso g).
static bool casoDisparo(uint32_t inicio, char* detalhe, size_t tam) {
  Simulacao sim;
  sim.agora = inicio;
  rodar(sim, 6000, ALTO);
  const uint32_t atraso = (uint32_t)(sim.tDisparou - inicio);
  snprintf(detalhe, tam, "disparou=%s apos %lu ms, estado=%s", sim.disparou ? "sim" : "nao",
           (unsigned long)atraso, nomeEstado(sim.regra.estado()));
  return sim.disparou && perto(atraso, X_S * 1000, JANELA_MS) &&
         sim.regra.estado() == Estado::ALERTA;
}

// Caso e, a partir de um instante inicial qualquer (usado também pelo caso g).
static bool casoHold(uint32_t inicio, char* detalhe, size_t tam) {
  Simulacao sim;
  sim.agora = inicio;
  rodar(sim, 6000, ALTO);              // entra em ALERTA
  rodar(sim, 1000, BAIXO);             // 1 s calmo...
  rodar(sim, JANELA_MS, ALTO);         // ...uma janela alta cancela o HOLD
  const uint32_t tQuieto = sim.agora;
  rodar(sim, HOLD_MS - JANELA_MS, BAIXO);
  const bool aindaAlerta = sim.regra.estado() == Estado::ALERTA;   // 1,875 s: ainda não
  rodar(sim, 2 * JANELA_MS, BAIXO);
  const uint32_t espera = (uint32_t)(sim.tNormalizou - tQuieto);
  snprintf(detalhe, tam, "alerta aos %lu ms=%s, normalizou apos %lu ms, estado=%s",
           (unsigned long)(HOLD_MS - JANELA_MS), aindaAlerta ? "sim" : "nao",
           (unsigned long)espera, nomeEstado(sim.regra.estado()));
  return aindaAlerta && sim.normalizou && perto(espera, HOLD_MS, JANELA_MS) &&
         sim.regra.estado() == Estado::CALMO;
}

bool teste_regra_rodar() {
  falhas = 0;
  char d[112];
  Serial.println("[teste-regra] === autoteste da regra dos X segundos ===");

  // a) Rajada de 2 s com X = 5 s: não dispara.
  {
    Simulacao sim;
    rodar(sim, 2000, ALTO);
    rodar(sim, 3000, BAIXO);
    snprintf(d, sizeof(d), "disparou=%s, estado=%s", sim.disparou ? "sim" : "nao",
             nomeEstado(sim.regra.estado()));
    resultado("a) rajada de 2 s nao dispara", !sim.disparou &&
              sim.regra.estado() == Estado::CALMO, d);
  }

  // b) 6 s acima: dispara perto de 5 s.
  resultado("b) 6 s acima dispara perto de 5 s", casoDisparo(0, d, sizeof(d)), d);

  // c) Oscilação entre limite - histerese e limite: não zera, então dispara.
  {
    Simulacao sim;
    for (int i = 0; i < 48; i++) rodar(sim, JANELA_MS, (i % 2 == 0) ? ALTO : FAIXA);   // 6 s
    snprintf(d, sizeof(d), "voltou a calmo=%s, disparou=%s",
             sim.voltouACalmo ? "sim" : "nao", sim.disparou ? "sim" : "nao");
    resultado("c) oscilacao na histerese nao zera", !sim.voltouACalmo && sim.disparou, d);
  }

  // d) Queda abaixo de limite - histerese: zera, e precisa de mais X s para disparar.
  {
    Simulacao sim;
    rodar(sim, 3000, ALTO);
    rodar(sim, JANELA_MS, BAIXO);
    const bool zerou = sim.regra.estado() == Estado::CALMO && sim.regra.acimaMs() == 0;
    rodar(sim, 3000, ALTO);
    snprintf(d, sizeof(d), "zerou=%s, disparou=%s, estado=%s", zerou ? "sim" : "nao",
             sim.disparou ? "sim" : "nao", nomeEstado(sim.regra.estado()));
    resultado("d) queda abaixo da histerese zera", zerou && !sim.disparou &&
              sim.regra.estado() == Estado::CONTANDO, d);
  }

  // e) Alerta só volta a calmo depois de HOLD_MS seguidos abaixo.
  resultado("e) alerta volta so depois do HOLD_MS", casoHold(0, d, sizeof(d)), d);

  // f1) Sem sinal durante a contagem: não dispara, vai a SEM_SINAL, volta a CALMO.
  {
    Simulacao sim;
    rodar(sim, 3000, ALTO);
    rodar(sim, 5000, 0.0f, false);
    const bool semSinal = sim.regra.estado() == Estado::SEM_SINAL;
    rodar(sim, JANELA_MS, BAIXO);
    snprintf(d, sizeof(d), "disparou=%s, sem_sinal=%s, depois=%s", sim.disparou ? "sim" : "nao",
             semSinal ? "sim" : "nao", nomeEstado(sim.regra.estado()));
    resultado("f) sem sinal nao dispara e volta a calmo", !sim.disparou && semSinal &&
              sim.perdeuSinal && sim.regra.estado() == Estado::CALMO, d);
  }

  // f2) Sem sinal durante o alerta: mantém o alerta por HOLD_MS, depois SEM_SINAL.
  {
    Simulacao sim;
    rodar(sim, 6000, ALTO);
    rodar(sim, HOLD_MS - JANELA_MS, 0.0f, false);
    const bool aindaAlerta = sim.regra.estado() == Estado::ALERTA;
    rodar(sim, 2 * JANELA_MS, 0.0f, false);
    const bool semSinal = sim.regra.estado() == Estado::SEM_SINAL;
    rodar(sim, JANELA_MS, BAIXO);
    snprintf(d, sizeof(d), "alerta mantido=%s, depois sem_sinal=%s, por fim=%s",
             aindaAlerta ? "sim" : "nao", semSinal ? "sim" : "nao",
             nomeEstado(sim.regra.estado()));
    resultado("f) sem sinal no alerta segura HOLD_MS", aindaAlerta && semSinal &&
              sim.regra.estado() == Estado::CALMO, d);
  }

  // g) Os casos b e e começando 2 s antes de millis() dar a volta.
  const uint32_t pertoDaVolta = 4294967295UL - 2000;
  resultado("g) volta do millis() no disparo", casoDisparo(pertoDaVolta, d, sizeof(d)), d);
  resultado("g) volta do millis() no HOLD_MS", casoHold(pertoDaVolta, d, sizeof(d)), d);

  // Extra) Limite e X fora da faixa usam os padrões (65 dB e 5 s).
  {
    Simulacao sim;
    rodar(sim, 1000, BAIXO, true, 10.0f, 0);     // limite 10 é inválido: vale 65
    const bool calmo = sim.regra.estado() == Estado::CALMO;
    rodar(sim, 6000, ALTO, true, 500.0f, 9999);  // X = 9999 é inválido: vale 5 s
    snprintf(d, sizeof(d), "calmo com 50 dB=%s, disparou=%s", calmo ? "sim" : "nao",
             sim.disparou ? "sim" : "nao");
    resultado("extra) limite e X invalidos usam o padrao", calmo && sim.disparou, d);
  }

  // h) Valores vindos da página, nos extremos: limite 100 dB e X = 60 s valem.
  {
    Simulacao sim;
    rodar(sim, 6000, 95.0f, true, 100.0f, 60);    // 95 dB fica abaixo do limite 100
    const bool calmo95 = sim.regra.estado() == Estado::CALMO;
    const uint32_t inicio = sim.agora;
    rodar(sim, 59000, 105.0f, true, 100.0f, 60);  // 59 s acima: ainda não (X = 60, não 5)
    const bool naoDisparou59 = !sim.disparou;
    rodar(sim, 2000, 105.0f, true, 100.0f, 60);
    const uint32_t atraso = (uint32_t)(sim.tDisparou - inicio);
    snprintf(d, sizeof(d), "calmo com 95=%s, nao aos 59 s=%s, disparou apos %lu ms",
             calmo95 ? "sim" : "nao", naoDisparou59 ? "sim" : "nao", (unsigned long)atraso);
    resultado("h) limite 100 dB e X 60 s respeitados", calmo95 && naoDisparou59 && sim.disparou &&
              perto(atraso, 60000, JANELA_MS), d);
  }

  // i) Logo acima do máximo (101 dB e 61 s) é inválido: usa 65 dB e 5 s.
  {
    Simulacao sim;
    rodar(sim, 6000, ALTO, true, 101.0f, 61);     // 70 dB passa do padrão 65; X vale 5
    const uint32_t atraso = sim.tDisparou;
    snprintf(d, sizeof(d), "disparou=%s apos %lu ms", sim.disparou ? "sim" : "nao",
             (unsigned long)atraso);
    resultado("i) limite 101 e X 61 usam o padrao", sim.disparou &&
              perto(atraso, X_S * 1000, JANELA_MS), d);
  }

  // j) Mudança de configuração no meio da contagem vale na hora (sem reiniciar).
  {
    Simulacao sim;
    rodar(sim, 2000, ALTO, true, 65.0f, 30);      // contando com X = 30 s
    const bool contando = sim.regra.estado() == Estado::CONTANDO && !sim.disparou;
    rodar(sim, 1000, ALTO, true, 65.0f, 2);       // a página muda X para 2 s: já passou
    snprintf(d, sizeof(d), "contando com X=30=%s, disparou com X=2=%s",
             contando ? "sim" : "nao", sim.disparou ? "sim" : "nao");
    resultado("j) X novo vale na proxima janela", contando && sim.disparou, d);
  }

  Serial.printf("[teste-regra] === %s (%d falha(s)) ===\n",
                falhas == 0 ? "TUDO PASS" : "HOUVE FAIL", falhas);
  return falhas == 0;
}

#else
bool teste_regra_rodar() { return true; }
#endif
