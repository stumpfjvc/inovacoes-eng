// NoiseTrack: lógica pura da rede. Sem WiFi, sem Serial, sem millis().
#include "rede_logica.h"
#include "config.h"
#include <string.h>
#include "mbedtls/sha256.h"

const char* nomeEstadoRede(EstadoRede e) {
  switch (e) {
    case EstadoRede::SEM_CREDENCIAL: return "sem_credencial";
    case EstadoRede::CONECTANDO:     return "conectando";
    case EstadoRede::CONECTADO:      return "conectado";
    case EstadoRede::CAIU:           return "caiu";
    case EstadoRede::AP_SETUP:       return "ap_setup";
  }
  return "?";
}

// Tempo de t0 até agora; continua certo quando millis() dá a volta.
static uint32_t decorrido(uint32_t agora, uint32_t t0) { return (uint32_t)(agora - t0); }

void MaquinaRede::irPara(EstadoRede novo, uint32_t agora) {
  if (novo != estado_) tEstado_ = agora;
  estado_ = novo;
}

void MaquinaRede::conectou(DecisaoRede& d) {
  estado_ = EstadoRede::CONECTADO;
  d.acabouDeConectar = true;
  if (apLigado_) {            // conectou: o AP de setup não é mais necessário
    d.desligarAp = true;
    apLigado_ = false;
  }
}

void MaquinaRede::abrirAp(DecisaoRede& d, uint32_t agora) {
  irPara(EstadoRede::AP_SETUP, agora);
  if (!apLigado_) {
    d.ligarAp = true;
    apLigado_ = true;
  }
  tUltimaTentativa_ = agora;   // a próxima tentativa com o AP ligado espera WIFI_RETRY_AP_MS
}

DecisaoRede MaquinaRede::atualizar(const EntradaRede& e, uint32_t agora) {
  const EstadoRede anterior = estado_;
  DecisaoRede d = {};

  if (!e.temCredencial) {
    // Sem rede salva: sai da rede atual e liga o AP (se permitido).
    if (estado_ == EstadoRede::CONECTANDO || estado_ == EstadoRede::CONECTADO ||
        estado_ == EstadoRede::CAIU) {
      d.desconectar = true;
    }
    if (e.permitirAp) abrirAp(d, agora);
    else              irPara(EstadoRede::SEM_CREDENCIAL, agora);
  } else if (e.credencialNova) {
    // Rede nova (portal ou Serial): recomeça do zero. O AP, se estiver ligado,
    // continua até conectar, para o celular ainda ver a página.
    irPara(EstadoRede::CONECTANDO, agora);
    tEstado_ = agora;
    d.tentarConectar = true;
    tUltimaTentativa_ = agora;
  } else {
    switch (estado_) {
      case EstadoRede::SEM_CREDENCIAL:   // agora há credencial: primeira tentativa
        irPara(EstadoRede::CONECTANDO, agora);
        d.tentarConectar = true;
        tUltimaTentativa_ = agora;
        break;

      case EstadoRede::CONECTANDO:
        if (e.conectado) {
          conectou(d);
        } else if (decorrido(agora, tEstado_) >= WIFI_TIMEOUT_MS) {
          if (e.permitirAp) {
            abrirAp(d, agora);           // não conectou a tempo: liga o AP de setup
          } else {
            irPara(EstadoRede::CAIU, agora);   // Wokwi: sem AP, só tenta de novo
            tUltimaTentativa_ = agora;
          }
        }
        break;

      case EstadoRede::CONECTADO:
        if (!e.conectado) {
          irPara(EstadoRede::CAIU, agora);
          tUltimaTentativa_ = agora;
        }
        break;

      case EstadoRede::CAIU:
        if (e.conectado) {
          conectou(d);
        } else if (decorrido(agora, tUltimaTentativa_) >= WIFI_RETRY_MS) {
          d.tentarConectar = true;
          tUltimaTentativa_ = agora;
        }
        break;

      case EstadoRede::AP_SETUP:
        if (e.conectado) {
          conectou(d);
        } else if (!e.clientesNoAp &&
                   decorrido(agora, tUltimaTentativa_) >= WIFI_RETRY_AP_MS) {
          // Ninguém usando o portal: tenta a rede salva de novo (ex.: o roteador
          // demorou mais que o ESP32 para ligar depois de uma queda de energia).
          d.tentarConectar = true;
          tUltimaTentativa_ = agora;
        }
        break;
    }
  }

  d.estado = estado_;
  d.mudou  = estado_ != anterior;
  return d;
}

// ---------- Validação do portal ----------
bool rede_ssid_valido(const char* ssid) {
  if (ssid == nullptr) return false;
  const size_t n = strlen(ssid);
  if (n < 1 || n > 32) return false;
  for (size_t i = 0; i < n; i++) {
    const unsigned char c = (unsigned char)ssid[i];
    if (c < 32 || c == 127) return false;   // caracteres de controle
  }
  return true;
}

bool rede_senha_valida(const char* senha) {
  if (senha == nullptr) return false;
  const size_t n = strlen(senha);
  if (n == 0) return true;                  // rede aberta
  if (n < 8 || n > 63) return false;
  for (size_t i = 0; i < n; i++) {
    const unsigned char c = (unsigned char)senha[i];
    if (c < 32 || c > 126) return false;    // só ASCII visível e espaço
  }
  return true;
}

// ---------- Senha do AP ----------
// 8 algarismos (2–9) + 24 maiúsculas (sem I, O) + 24 minúsculas (sem l, o) = 56.
const char REDE_ALFABETO_SENHA[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz";

void rede_gerar_senha_ap(const uint8_t mac[6], const char* segredo, char* saida) {
  rede_gerar_senha(mac, "", segredo, saida);
}

void rede_gerar_senha(const uint8_t mac[6], const char* prefixo, const char* segredo, char* saida) {
  // SHA-256 embaralha (MAC + prefixo + segredo) num resumo de 32 bytes. Quem só
  // conhece o MAC (que o próprio AP anuncia) não consegue refazer a conta sem o
  // segredo. Prefixos diferentes dão senhas sem relação visível entre si.
  uint8_t resumo[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);   // 0 = SHA-256 (1 seria SHA-224)
  mbedtls_sha256_update_ret(&ctx, mac, 6);
  mbedtls_sha256_update_ret(&ctx, (const unsigned char*)prefixo, strlen(prefixo));
  mbedtls_sha256_update_ret(&ctx, (const unsigned char*)segredo, strlen(segredo));
  mbedtls_sha256_finish_ret(&ctx, resumo);
  mbedtls_sha256_free(&ctx);

  // Cada um dos 8 primeiros bytes (0–255) escolhe uma letra: resto da divisão
  // pelo tamanho do alfabeto. Dá cerca de 46 bits de acaso, bem acima do
  // necessário para uma senha de AP de setup.
  const size_t tamAlfabeto = sizeof(REDE_ALFABETO_SENHA) - 1;
  for (size_t i = 0; i < REDE_TAM_SENHA_AP; i++) {
    saida[i] = REDE_ALFABETO_SENHA[resumo[i] % tamAlfabeto];
  }
  saida[REDE_TAM_SENHA_AP] = '\0';
}
