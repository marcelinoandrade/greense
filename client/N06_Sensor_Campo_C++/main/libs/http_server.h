#pragma once

#include "esp_err.h"

/* Sobe o Wi-Fi em modo AP e inicia o servidor HTTP.
 *
 * AP:
 *   SSID: "ESP32_TEMP"
 *   senha: "12345678"
 *   IP padrão: 192.168.4.1
 *
 * Rotas HTTP:
 *   GET /            -> dashboard HTML com gráfico e status
 *   GET /history     -> JSON com últimos pontos (para o JS do dashboard)
 *   GET /download    -> CSV completo (Content-Disposition: attachment)
 *   GET /calibra     -> página HTML de calibração do sensor de umidade do solo
 *   GET /set_calibra -> aplica calibração passada via query string (?seco=X&molhado=Y)
 *
 * Proteções:
 *   - task httpd com stack aumentado (10 kB) para evitar stack overflow
 *   - timeout de recv/send
 *   - LRU purge
 *
 * Retorna ESP_OK se tudo inicializar (AP + httpd).
 */
esp_err_t http_server_start(void);

/* Para o servidor HTTP se estiver rodando.
 * Retorna ESP_OK se parou.
 */
esp_err_t http_server_stop(void);
