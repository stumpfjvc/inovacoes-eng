// NoiseTrack: comandos digitados no Serial (115200, uma linha por comando).
// Ligados por COMANDOS_SERIAL (config.h). Funcionam na placa real e no Wokwi:
//   limite 70   -> muda e grava o limite (30 a 100 dB)
//   x 8         -> muda e grava o tempo X (1 a 60 s)
//   wifi        -> mostra o estado da rede, o IP e o horário
//   wifi apagar -> apaga a rede salva (liga o AP de setup)
//   web         -> mostra o endereço da página, os clientes e a senha de configuração
//   etiqueta    -> mostra a etiqueta: senha do AP de setup e senha Config
// Só com FONTE_SIMULADA = 1:
//   80 / auto   -> força o nível simulado (20 a 120 dB) ou volta ao automático
// Com COMANDOS_SERIAL = 0, nada é lido do Serial.
#pragma once

// Mostra a ajuda dos comandos que existem nesta configuração.
void comandos_serial_iniciar();

// Chamar a cada volta do loop(). Lê só o que já chegou no Serial (nunca espera)
// e executa cada linha completa. Pode gravar na flash, por isso fica no loop().
void comandos_serial_atualizar();
