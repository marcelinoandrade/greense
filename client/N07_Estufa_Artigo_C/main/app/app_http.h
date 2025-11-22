#ifndef APP_HTTP_H
#define APP_HTTP_H

#include "esp_err.h"
#include <stdbool.h>
#include <time.h>

// Envia dados via HTTPS POST (similar à ESP32-CAM)
esp_err_t app_http_send_data(const char *url, const uint8_t *data, size_t data_len, const char *content_type);

// Verifica conectividade com servidor
bool app_http_check_connectivity(const char *url);

// Envia frame térmico individual para servidor /termica
// Formato: {"temperaturas": [768 floats], "timestamp": unix_timestamp}
esp_err_t app_http_send_thermal_frame(const float *temps, time_t timestamp);

#endif // APP_HTTP_H

