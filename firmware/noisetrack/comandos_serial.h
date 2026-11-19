// NoiseTrack: comandos digitados no Serial, só para teste com a fonte simulada.
//   80 / auto   -> força o nível simulado (20 a 120 dB) ou volta ao automático
//   limite 70   -> muda e grava o limite (30 a 120 dB)    [TESTE_PREFS]
//   x 8         -> muda e grava o tempo X (1 a 600 s)      [TESTE_PREFS]
//   wifi        -> mostra o estado da rede, o IP e o horário
//   wifi apagar -> apaga a rede salva (liga o AP de setup)
//   web         -> mostra o endereço da página, os clientes e a senha de configuração
// Com FONTE_SIMULADA = 0, nada aqui é usado.
#pragma once

// Mostra a ajuda dos comandos.
void comandos_serial_iniciar();

// Chamar a cada volta do loop(). Lê só o que já chegou no Serial (nunca espera)
// e executa cada linha completa. Pode gravar na flash, por isso fica no loop().
void comandos_serial_atualizar();
