// NoiseTrack: envia o nível e o estado para a nuvem (Supabase) e recebe o
// limite e o X. Roda numa tarefa FreeRTOS própria, de baixa prioridade, e só
// fala com a internet quando o Wi-Fi do local está conectado (não no portal).
// Nada aqui pode atrasar o alarme local: a tarefa de áudio é mais prioritária
// e fica em outro núcleo.
//
// A NUVEM MANDA: se o limite/X que vier de lá for diferente do daqui, vale o de
// lá, e ele é gravado na memória pelo loop() (a mesma função que a página usa).
#pragma once
#include <stdint.h>

// Confere a configuração (URL, chaves, certificado) e, se estiver tudo certo,
// cria a tarefa. Se faltar algo, avisa UMA vez no Serial e deixa a nuvem
// desligada. Chamar no setup(), depois de rede_iniciar() (precisa do ID).
void nuvem_iniciar();

// Chamar a cada volta do loop(). Avisa a tarefa se o Wi-Fi está conectado e
// aplica (e grava, se mudou) o limite/X que a nuvem mandou. Não espera nada.
void nuvem_atualizar(uint32_t agora);

// Comando "nuvem": ligada/desligada, último resultado, último sucesso e a
// configuração recebida. Nunca mostra a chave do aparelho nem a pública inteira.
void nuvem_imprimir_status();
