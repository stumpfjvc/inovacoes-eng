// NoiseTrack: página do ESP32 com medidor ao vivo (Parte 10).
// HTTP na porta 80 (página, /status, /config) e WebSocket na porta 81.
// Tudo roda a partir do loop(); o alerta local nunca depende daqui.
#pragma once
#include <stdint.h>

// Registra as rotas. Chamar no setup(), depois de rede_iniciar().
void web_iniciar();

// Chamar a cada volta do loop(). Liga os servidores quando o Wi-Fi do local
// está CONECTADO e desliga quando não está; atende pedidos e envia o estado
// aos clientes do WebSocket a cada janela nova. Nunca espera a rede.
void web_atualizar(uint32_t agora);

// Desliga os servidores e fecha os clientes. O rede.cpp chama antes de ligar
// o portal do AP, que precisa da porta 80.
void web_parar();

// Imprime no Serial o endereço, os clientes e a senha de configuração (comando "web").
void web_imprimir_status();
