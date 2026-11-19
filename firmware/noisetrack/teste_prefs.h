// NoiseTrack: autoteste da memória (Parte 8), ligado por TESTE_PREFS.
#pragma once

// Salva, lê de volta, confere a recusa de valores fora da faixa e que o mesmo
// valor não regrava. Imprime PASS ou FAIL e restaura os valores originais.
// Chamar no setup(), depois de armazenamento_iniciar(). Devolve true se tudo passou.
bool teste_prefs_rodar();
