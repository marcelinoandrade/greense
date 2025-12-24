#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o gerenciador de período de amostragem.
 *
 * Deve ser chamado após nvs_flash_init().
 */
esp_err_t sampling_period_init(void);

/**
 * @brief Retorna o período de amostragem atual (em ms).
 */
uint32_t sampling_period_get_ms(void);

/**
 * @brief Define um novo período de amostragem (em ms).
 *
 * Apenas valores suportados serão aceitos.
 */
esp_err_t sampling_period_set_ms(uint32_t period_ms);

/**
 * @brief Verifica se o valor informado é suportado.
 */
bool sampling_period_is_valid(uint32_t period_ms);

#ifdef __cplusplus
}
#endif


