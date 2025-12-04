#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Estrutura com os limites de tolerância para cada variável */
typedef struct {
    float temp_ar_min;
    float temp_ar_max;
    float umid_ar_min;
    float umid_ar_max;
    float temp_solo_min;
    float temp_solo_max;
    float umid_solo_min;
    float umid_solo_max;
    float luminosidade_min;
    float luminosidade_max;
    float dpv_min;
    float dpv_max;
} cultivation_tolerance_t;

/**
 * @brief Inicializa o gerenciador de tolerâncias de cultivo.
 *
 * Deve ser chamado após nvs_flash_init().
 */
esp_err_t cultivation_tolerance_init(void);

/**
 * @brief Retorna os valores de tolerância atuais.
 */
void cultivation_tolerance_get(cultivation_tolerance_t *out);

/**
 * @brief Define novos valores de tolerância.
 */
esp_err_t cultivation_tolerance_set(const cultivation_tolerance_t *tolerance);

/**
 * @brief Retorna os valores padrão de tolerância.
 */
void cultivation_tolerance_get_defaults(cultivation_tolerance_t *out);

/**
 * @brief Restaura os valores padrão de tolerância e salva no NVS.
 */
esp_err_t cultivation_tolerance_reset_to_defaults(void);

#ifdef __cplusplus
}
#endif

