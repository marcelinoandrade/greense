#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o gerenciador de período estatístico (número de amostras).
 *
 * Deve ser chamado após nvs_flash_init().
 */
esp_err_t stats_window_init(void);

/**
 * @brief Retorna o número de amostras atual para estatísticas e gráficos.
 */
int stats_window_get_count(void);

/**
 * @brief Define um novo número de amostras para estatísticas e gráficos.
 *
 * Apenas valores suportados serão aceitos (5, 10, 15, 20).
 */
esp_err_t stats_window_set_count(int count);

/**
 * @brief Verifica se o valor informado é suportado.
 */
bool stats_window_is_valid(int count);

#ifdef __cplusplus
}
#endif

