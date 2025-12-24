#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o barramento 1-Wire do DS18B20 usando GPIO do board.h
 */
esp_err_t ds18b20_bsp_init(void);

/**
 * @brief Lê temperatura em °C do DS18B20
 * 
 * Bloqueia ~750 ms (tempo de conversão).
 * Retorna temperatura em °C.
 * Retorna -127.0 em erro.
 */
float ds18b20_bsp_read_temperature(void);

#ifdef __cplusplus
}
#endif

