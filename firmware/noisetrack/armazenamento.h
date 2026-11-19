// NoiseTrack: limite, X e calibração guardados na flash (Preferences), Parte 8.
// Só chamar do setup() ou do loop(): gravar na flash pode demorar alguns
// milissegundos, e a tarefa de áudio nunca pode esperar por isso.
#pragma once
#include <stdint.h>
#include <stddef.h>

// Resultado de uma gravação.
struct ResultadoSalvar {
  bool aceito;      // o valor estava dentro da faixa?
  bool gravou;      // escreveu na flash? (false se aceito mas igual ao guardado)
  bool erroFlash;   // tentou gravar e a flash recusou
};

// Abre a memória, lê limite e X (ou usa os padrões se faltarem ou estiverem
// fora da faixa) e entrega os valores à tarefa de áudio.
// Chamar depois de tarefa_audio_iniciar(). Devolve false se a memória não abriu.
bool armazenamento_iniciar();

// Valida (30–120 dB e 1–600 s), grava só o que mudou e atualiza a tarefa.
ResultadoSalvar armazenamento_salvar(int limiteDb, int tempoXs);

// Valores guardados agora na flash (ou os padrões, se faltarem ou forem inválidos).
int armazenamento_limite_db();
int armazenamento_tempo_x_s();

// Calibração por microfone (mic 0 = A, 1 = B), reservada para a Parte 11.
// Ainda NÃO entra na conta de dB: lá continuam OFFSET_CAL_MIC_A/B_DB do config.h.
float           armazenamento_offset_cal(int mic);   // 0.0 se não houver valor guardado
ResultadoSalvar armazenamento_salvar_offset_cal(int mic, float db);

// Wi-Fi do local (namespace "wifi", chaves "ssid" e "senha"), Parte 9.
constexpr size_t WIFI_TAM_SSID  = 33;   // 32 caracteres + fim do texto
constexpr size_t WIFI_TAM_SENHA = 64;   // 63 caracteres + fim do texto

// Lê a rede salva. Devolve false (e textos vazios) se não houver uma válida.
bool armazenamento_wifi_ler(char* ssid, size_t tamSsid, char* senha, size_t tamSenha);
// Valida (nome de 1 a 32, senha vazia ou de 8 a 63) e grava só o que mudou.
ResultadoSalvar armazenamento_wifi_salvar(const char* ssid, const char* senha);
// Apaga a rede salva. Devolve false se a memória não abriu.
bool armazenamento_wifi_apagar();
