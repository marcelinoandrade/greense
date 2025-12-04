#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o sensor AHT10 via I2C
 * @return ESP_OK em caso de sucesso
 */
esp_err_t aht10_bsp_init(void);

/**
 * @brief Lê temperatura e umidade do ar do sensor AHT10
 * @param temp_air Ponteiro para armazenar temperatura em °C
 * @param humid_air Ponteiro para armazenar umidade relativa em % (0-100)
 * @return ESP_OK em caso de sucesso
 */
esp_err_t aht10_bsp_read(float *temp_air, float *humid_air);

/**
 * @brief Verifica se o sensor AHT10 está disponível
 * @return true se o sensor está disponível e inicializado
 */
bool aht10_bsp_is_available(void);

/**
 * @brief Obtém o handle do barramento I2C para compartilhar com outros dispositivos
 * @return Handle do barramento I2C ou NULL se não inicializado
 */
void* aht10_get_i2c_bus_handle(void);

#ifdef __cplusplus
}
#endif

