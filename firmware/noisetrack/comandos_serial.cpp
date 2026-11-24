// NoiseTrack: comandos digitados no Serial (placa real e fonte simulada).
// Este é o único lugar que lê o Serial, e ele roda no loop(): a tarefa de áudio
// não lê o Serial nem espera a flash.
#include "comandos_serial.h"
#include "config.h"

#if COMANDOS_SERIAL
#include "armazenamento.h"
#include "rede.h"
#include "web.h"
#include "nuvem.h"
#if FONTE_SIMULADA
#include "audio.h"
#endif
#include <string.h>
#include <stdlib.h>

static char    linha[24];
static uint8_t tamLinha   = 0;
static bool    linhaLonga = false;

// Converte o texto inteiro em número. Diferente do parseInt, texto inválido
// ("abc", "7x", "") devolve false em vez de virar 0.
static bool lerInteiro(const char* s, long* v) {
  if (*s == '\0') return false;
  char* resto = nullptr;
  *v = strtol(s, &resto, 10);
  return resto != s && *resto == '\0';
}

static void imprimirResultado(const char* nome, long valor, const ResultadoSalvar& r) {
  if (!r.aceito)        Serial.printf("[prefs] %s %ld recusado (fora da faixa)\n", nome, valor);
  else if (r.erroFlash) Serial.printf("[prefs] %s = %ld aceito, mas ERRO ao gravar\n", nome, valor);
  else                  Serial.printf("[prefs] %s = %ld aceito, %s\n", nome, valor,
                                      r.gravou ? "gravado" : "igual ao guardado (nao regravou)");
}

static void interpretarLinha(char* s) {
  // Tira espaços do começo e do fim.
  while (*s == ' ' || *s == '\t') s++;
  char* fim = s + strlen(s);
  while (fim > s && (fim[-1] == ' ' || fim[-1] == '\t')) *--fim = '\0';
  if (*s == '\0') return;   // linha vazia

  long v = 0;

  // Página (Parte 10): só a linha exata "web".
  if (strcmp(s, "web") == 0) {
    web_imprimir_status();
    return;
  }

  // Nuvem (Supabase): ligada/desligada, último resultado e configuração recebida.
  if (strcmp(s, "nuvem") == 0) {
    nuvem_imprimir_status();
    return;
  }

  // Etiqueta do aparelho: senhas, QR de Wi-Fi, endereços e ID.
  if (strcmp(s, "etiqueta") == 0) {
    rede_imprimir_etiqueta();
    return;
  }

  // Rede (Parte 9): só as linhas exatas "wifi" e "wifi apagar".
  if (strcmp(s, "wifi") == 0) {
    rede_imprimir_status();
    return;
  }
  if (strcmp(s, "wifi apagar") == 0) {
    if (armazenamento_wifi_apagar()) {
      Serial.println("[wifi] rede salva apagada");
#if USANDO_WOKWI
      Serial.println("[wifi] (no Wokwi a rede continua sendo a Wokwi-GUEST)");
#endif
      rede_credenciais_mudaram();
    } else {
      Serial.println("[wifi] ERRO: memoria nao abriu");
    }
    return;
  }

  // "limite N" e "x N": o prefixo diferencia do nível forçado, que é só um número.
  if (strncmp(s, "limite ", 7) == 0) {
    if (lerInteiro(s + 7, &v)) {
      imprimirResultado("limite", v, armazenamento_salvar((int)v, armazenamento_tempo_x_s()));
    } else {
      Serial.printf("[prefs] '%s' ignorado: use 'limite %d' a 'limite %d'\n",
                    s, LIMITE_MIN_DB, LIMITE_MAX_DB);
    }
    return;
  }
  if (strncmp(s, "x ", 2) == 0) {
    if (lerInteiro(s + 2, &v)) {
      imprimirResultado("x", v, armazenamento_salvar(armazenamento_limite_db(), (int)v));
    } else {
      Serial.printf("[prefs] '%s' ignorado: use 'x %d' a 'x %d'\n", s, TEMPO_X_MIN_S, TEMPO_X_MAX_S);
    }
    return;
  }

#if FONTE_SIMULADA
  // Nível forçado da simulação: um número sozinho, ou "auto".
  if (strcmp(s, "auto") == 0) {
    audio_sim_forcar_nivel(-1);
    Serial.println("[sim] nivel automatico");
    return;
  }
  if (lerInteiro(s, &v) && (v == -1 || (v >= SIM_FORCADO_MIN_DB && v <= SIM_FORCADO_MAX_DB))) {
    audio_sim_forcar_nivel((int)v);
    if (v == -1) Serial.println("[sim] nivel automatico");
    else         Serial.printf("[sim] nivel forcado = %ld dB\n", v);
    return;
  }
#else
  // Com o microfone de verdade não há nível forçado: avisa em vez de só ignorar.
  if (strcmp(s, "auto") == 0 || lerInteiro(s, &v)) {
    Serial.printf("[serial] '%s' ignorado: nivel forcado so existe com FONTE_SIMULADA 1\n", s);
    return;
  }
#endif
  Serial.printf("[serial] '%s' ignorado\n", s);
}

void comandos_serial_iniciar() {
  Serial.print("[serial] comandos:");
#if FONTE_SIMULADA
  Serial.printf(" um nivel (%d a %d dB) ou 'auto' |", SIM_FORCADO_MIN_DB, SIM_FORCADO_MAX_DB);
#endif
  Serial.println(" 'limite N' | 'x N' | 'wifi' | 'wifi apagar' | 'web' | 'etiqueta' | 'nuvem'");
}

void comandos_serial_atualizar() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (linhaLonga) {
        Serial.println("[serial] linha longa demais, ignorada");
      } else if (tamLinha > 0) {
        linha[tamLinha] = '\0';
        interpretarLinha(linha);
      }
      tamLinha   = 0;
      linhaLonga = false;
    } else if (tamLinha < sizeof(linha) - 1) {
      linha[tamLinha++] = c;
    } else {
      linhaLonga = true;
    }
  }
}

#else
// COMANDOS_SERIAL = 0: nada é lido do Serial.
void comandos_serial_iniciar() {
  Serial.println("[serial] comandos desligados (COMANDOS_SERIAL 0)");
}
void comandos_serial_atualizar() {}
#endif
