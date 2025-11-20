#ifndef BSP_WIFI_H
#define BSP_WIFI_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>

esp_err_t bsp_wifi_init(void);
esp_err_t bsp_wifi_connect(void);
bool bsp_wifi_is_connected(void);
esp_err_t bsp_wifi_wait_connected(TickType_t timeout);

// Função adicional para verificar conexão (similar à ESP32-CAM)
bool bsp_wifi_check_connection(void);

#endif // BSP_WIFI_H

