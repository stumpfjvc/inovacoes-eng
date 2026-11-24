// NoiseTrack: autoteste da parte pura da nuvem, ligado por TESTE_NUVEM.
#pragma once

// Testa estados, JSON do pedido (com escape), leitura da resposta HTTP,
// interpretação de aparelho_sync (válida, fora da faixa, faltando, lixo),
// conferência da configuração e a espera crescente. Não usa a rede.
// Devolve true se tudo passou.
bool teste_nuvem_rodar();
