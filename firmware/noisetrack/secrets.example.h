// NoiseTrack: modelo do arquivo de segredos.
//
// Copie este arquivo para "secrets.h", na mesma pasta, e troque o valor.
// O secrets.h está no .gitignore e NUNCA deve ir para o Git.
// Sem o secrets.h, o firmware compila com um segredo padrão e avisa no Serial
// que a senha do AP de setup fica previsível.
#pragma once

// Misturado ao MAC da placa para gerar a senha do AP "NoiseTrack-Setup".
// Use um texto longo e aleatório, e o MESMO em todas as placas do grupo
// (a senha de cada placa continua diferente, porque o MAC muda).
#define WIFI_AP_SEGREDO "troque-por-um-texto-longo-e-aleatorio"

// Chave SECRETA deste aparelho na nuvem (Supabase). É diferente em cada placa.
// Gere com:  bash nuvem/ferramentas/novo_aparelho.sh <id>   (o <id> é o que o
// comando "etiqueta" mostra) e cole aqui o conteúdo de ~/.noisetrack/<id>.chave.
// Enquanto ficar este modelo, a nuvem fica desligada.
#define NUVEM_CHAVE_APARELHO "COLE_AQUI_A_CHAVE_DO_APARELHO"
