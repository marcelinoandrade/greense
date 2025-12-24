#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o sensor BH1750 via I2C
 * @return ESP_OK em caso de sucesso
 */
esp_err_t bh1750_bsp_init(void);

/**
 * @brief Lê a luminosidade do sensor BH1750
 * @param lux Ponteiro para armazenar luminosidade em lux
 * @return ESP_OK em caso de sucesso
 */
esp_err_t bh1750_bsp_read(float *lux);

/**
 * @brief Verifica se o sensor BH1750 está disponível
 * @return true se o sensor está disponível e inicializado
 */
bool bh1750_bsp_is_available(void);

/**
 * @brief Configura o barramento I2C compartilhado (chamado pelo AHT10)
 * @param bus_handle Handle do barramento I2C compartilhado
 */
void bh1750_set_shared_i2c_bus(void* bus_handle);

#ifdef __cplusplus
}
#endif

