// NoiseTrack: certificado raiz (CA) para conferir o HTTPS da nuvem.
//
// Só é usado com NUVEM_TLS_VALIDAR 1 (no config_local.h). Com 0, a conexão é
// criptografada mas o aparelho NÃO confere se está falando mesmo com o Supabase.
//
// O QUE É: um certificado PÚBLICO da empresa que assina o certificado do site
// (a "autoridade certificadora raiz"). Não é segredo e pode ir para o Git.
//
// COMO OBTER (no PC, com internet):
//   Opção A, pelo navegador (Chrome/Edge):
//     1. Abra https://SEU-PROJETO.supabase.co (o mesmo endereço do NUVEM_URL).
//     2. Clique no cadeado (ou no ícone de "configurações do site") ao lado do
//        endereço -> "A conexão é segura" -> "O certificado é válido".
//     3. Aba "Detalhes" -> em "Hierarquia do certificado", clique na linha de
//        CIMA (a raiz) -> "Exportar..." -> formato "Base64 (ASCII), .pem/.crt".
//     4. Abra o arquivo salvo no Bloco de Notas e copie TUDO, das linhas
//        -----BEGIN CERTIFICATE----- até -----END CERTIFICATE-----.
//   Opção B, pelo Git Bash:
//     openssl s_client -connect SEU-PROJETO.supabase.co:443 -showcerts </dev/null
//     Veja no fim da lista o "i:" (issuer = quem assinou) do último certificado
//     e baixe esse certificado raiz do site da autoridade (em formato PEM).
//
// COMO COLAR: troque a linha COLE_AQUI abaixo pelo texto copiado (pode colar
// mais de um certificado, um embaixo do outro, se não tiver certeza de qual é).
// Depois: NUVEM_TLS_VALIDAR 1 no config_local.h, compilar e gravar.
//
// Se um dia o Supabase trocar de autoridade, a conexão passa a falhar com erro
// de certificado (o alarme local continua funcionando); aí é só repetir isto.
#pragma once

static const char NUVEM_CA_RAIZ[] = R"CA(
COLE_AQUI_O_CERTIFICADO_RAIZ_EM_PEM
)CA";
