// NoiseTrack: guarda o MAIOR nível entre as janelas de medição, desde a última
// vez em que alguém perguntou. É a conta pura (sem trava, sem rede, sem
// millis()): quem usa é responsável por proteger com trava, se precisar.
//
// Para que serve: a nuvem envia a cada 1 s, mas o microfone mede a cada 125 ms.
// Mandar só a última janela perderia um pico de 0,1 s (uma palma, por exemplo).
// Mandando o maior das ~8 janelas, o pico aparece no site.
#pragma once
#include <stdint.h>
#include <math.h>

struct LeituraMaxima {
  float    db;       // o maior nível com sinal (só vale se "valido")
  bool     valido;   // houve ao menos uma janela com sinal?
  uint32_t janelas;  // quantas janelas entraram (0 = nenhuma desde a última pergunta)
};

// Funciona como uma classe Java: "MaximoJanelas m;" e depois m.somar(...) a cada
// janela e m.pegar() de vez em quando.
class MaximoJanelas {
 public:
  // Chamar uma vez por janela. Janela sem sinal (ou nível que não é número)
  // conta como janela, mas não entra no máximo.
  void somar(float db, bool valido) {
    janelas_++;
    if (valido && isfinite(db) && (!tem_ || db > max_)) {
      max_ = db;
      tem_ = true;
    }
  }

  // Devolve o máximo e COMEÇA DE NOVO: a próxima pergunta cobre só o que vier depois.
  LeituraMaxima pegar() {
    const LeituraMaxima r = {tem_ ? max_ : 0.0f, tem_, janelas_};
    max_     = 0.0f;
    tem_     = false;
    janelas_ = 0;
    return r;
  }

 private:
  float    max_     = 0.0f;
  bool     tem_     = false;
  uint32_t janelas_ = 0;
};
