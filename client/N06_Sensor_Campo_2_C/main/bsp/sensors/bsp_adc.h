#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o ADC oneshot usando configuração do board.h
 */
esp_err_t adc_bsp_init(void);

/**
 * @brief Lê o valor ADC bruto do sensor de umidade do solo
 *
 * Retorna 0..4095 em sucesso.
 * Retorna ESP_ERR_INVALID_STATE se não inicializado.
 */
esp_err_t adc_bsp_read_soil(int *raw_value);

#ifdef __cplusplus
}
#endif

