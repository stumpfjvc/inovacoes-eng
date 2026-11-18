// NoiseTrack: autoteste da tarefa de áudio (Parte 7), ligado por TESTE_TAREFA.
#pragma once
#include <stdint.h>

// Anota o instante e o contador de janelas. Chamar logo depois de criar a tarefa.
void teste_tarefa_iniciar(uint32_t agora);

// Chamar a cada volta do loop(). Não espera: quando passam 3 s, confere uma
// única vez se as janelas avançaram ~8 por segundo e imprime PASS ou FAIL,
// junto com a menor pilha livre da tarefa.
void teste_tarefa_verificar(uint32_t agora);
