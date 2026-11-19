// NoiseTrack: Wi-Fi, AP de setup com portal, mDNS e NTP (Parte 9).
// Tudo roda a partir do loop() e nunca espera a rede: cada chamada faz um
// pedacinho e volta. O alerta local (tarefa de áudio) não depende de nada daqui.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "rede_logica.h"

// Prepara o Wi-Fi, lê a rede salva e calcula a senha do AP (imprime no Serial).
// Chamar no setup(), depois de armazenamento_iniciar().
void rede_iniciar();

// Chamar a cada volta do loop(). Atende o portal (se ligado), avança a máquina
// de estados e confere o NTP. Não usa delay() nem espera a conexão.
void rede_atualizar(uint32_t agora);

EstadoRede rede_estado();

// Avisa que a rede salva mudou fora do portal (ex.: "wifi apagar" no Serial).
void rede_credenciais_mudaram();

// Imprime no Serial o estado, a rede, o IP e o horário (comando "wifi").
void rede_imprimir_status();

// Texto da etiqueta: "Rede: NoiseTrack-Setup | Senha: XXXXXXXX".
void rede_texto_etiqueta(char* saida, size_t tam);
