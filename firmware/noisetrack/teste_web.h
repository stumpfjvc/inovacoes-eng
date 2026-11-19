// NoiseTrack: autoteste da parte pura da página (Parte 10), ligado por TESTE_WEB.
#pragma once

// Testa a montagem do JSON, a validação de limite e X, a senha, o bloqueio por
// tentativas erradas (com tempo simulado, inclusive a volta do millis()) e as
// contas do WebSocket. Não usa a rede. Devolve true se tudo passou.
bool teste_web_rodar();
