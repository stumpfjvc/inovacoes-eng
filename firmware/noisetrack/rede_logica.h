// NoiseTrack: lógica pura da rede (Parte 9). Não usa WiFi, Serial nem millis():
// recebe o que aconteceu e o instante atual, e devolve o que fazer.
// O hardware fica em rede.cpp. Assim dá para testar tudo com tempo simulado.
#pragma once
#include <stdint.h>
#include <stddef.h>

enum class EstadoRede : uint8_t {
  SEM_CREDENCIAL,  // nenhuma rede salva (e AP não permitido, ou ainda não decidiu)
  CONECTANDO,      // primeira tentativa com a rede salva (até WIFI_TIMEOUT_MS)
  CONECTADO,       // no Wi-Fi do local
  CAIU,            // estava conectado e perdeu: tenta de novo a cada WIFI_RETRY_MS
  AP_SETUP         // AP "NoiseTrack-Setup" ligado, com o portal
};

const char* nomeEstadoRede(EstadoRede e);

// O que aconteceu desde a última chamada.
struct EntradaRede {
  bool temCredencial;   // há rede salva?
  bool conectado;       // o Wi-Fi diz que está conectado?
  bool clientesNoAp;    // alguém está conectado no AP de setup?
  bool credencialNova;  // o portal ou o Serial acabou de mudar a rede salva
  bool permitirAp;      // false no Wokwi: nunca liga o AP
};

// O que o rede.cpp deve fazer agora (pode ser mais de uma coisa).
struct DecisaoRede {
  EstadoRede estado;
  bool mudou;             // o estado mudou nesta chamada?
  bool tentarConectar;    // chamar WiFi.begin com a rede salva
  bool desconectar;       // sair da rede atual
  bool ligarAp;           // ligar o AP de setup e o portal
  bool desligarAp;        // desligar o AP de setup e o portal
  bool acabouDeConectar;  // iniciar mDNS e NTP, imprimir o IP
};

// Funciona como uma classe Java: "MaquinaRede m;" e m.atualizar(...) a cada passo.
class MaquinaRede {
 public:
  DecisaoRede atualizar(const EntradaRede& e, uint32_t agora);
  EstadoRede  estado()    const { return estado_; }
  bool        apLigado()  const { return apLigado_; }

 private:
  void irPara(EstadoRede novo, uint32_t agora);
  void conectou(DecisaoRede& d);
  void abrirAp(DecisaoRede& d, uint32_t agora);

  EstadoRede estado_          = EstadoRede::SEM_CREDENCIAL;
  uint32_t   tEstado_         = 0;      // quando entrou no estado atual
  uint32_t   tUltimaTentativa_ = 0;     // última chamada a tentarConectar
  bool       apLigado_        = false;
};

// Validação do portal: nome de 1 a 32 bytes, sem caracteres de controle.
bool rede_ssid_valido(const char* ssid);
// Senha vazia (rede aberta) ou de 8 a 63 caracteres ASCII visíveis (regra do WPA2).
bool rede_senha_valida(const char* senha);

// Alfabeto da senha do AP: sem 0/O/o, 1/l/I, para ler sem confusão na etiqueta.
extern const char REDE_ALFABETO_SENHA[];
constexpr size_t REDE_TAM_SENHA_AP = 8;

// Senha do AP: SHA-256 de (MAC + segredo), convertida em 8 caracteres do alfabeto.
// "saida" precisa de REDE_TAM_SENHA_AP + 1 posições (o +1 é o fim do texto).
void rede_gerar_senha_ap(const uint8_t mac[6], const char* segredo, char* saida);
