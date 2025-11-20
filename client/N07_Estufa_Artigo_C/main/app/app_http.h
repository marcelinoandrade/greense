#ifndef APP_HTTP_H
#define APP_HTTP_H

#include "esp_err.h"
#include <stdbool.h>

// Envia dados via HTTPS POST (similar à ESP32-CAM)
esp_err_t app_http_send_data(const char *url, const uint8_t *data, size_t data_len, const char *content_type);

// Verifica conectividade com servidor
bool app_http_check_connectivity(const char *url);

#endif // APP_HTTP_H

