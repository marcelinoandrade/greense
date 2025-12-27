#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temp_air;
    float humid_air;
    float temp_soil;
    float humid_soil;  // Já convertido para %
    float luminosity;  // lux (intensidade de luminosidade)
    float dpv;         // kPa (Déficit de Pressão de Vapor)
} sensor_reading_t;

esp_err_t sensor_manager_init(void);
esp_err_t sensor_manager_read(sensor_reading_t *reading);
bool sensor_manager_is_valid(const sensor_reading_t *reading);

/**
 * @brief Valida leitura de sensores com detecção adaptativa de outliers
 * 
 * Esta função valida ranges básicos e também detecta outliers baseado no
 * período de amostragem. Para períodos longos (6-12h), permite mudanças
 * maiores que são esperadas em ciclos dia/noite.
 * 
 * @param reading Leitura dos sensores a validar
 * @param sampling_period_ms Período de amostragem em milissegundos
 * @return true se válido, false caso contrário
 */
bool sensor_manager_is_valid_with_outlier_detection(const sensor_reading_t *reading, 
                                                     uint32_t sampling_period_ms);

// Funções de compatibilidade com código antigo
float sensor_manager_get_temp_ar(void);
float sensor_manager_get_umid_ar(void);
float sensor_manager_get_temp_solo(void);
int sensor_manager_get_umid_solo_raw(void);

#ifdef __cplusplus
}
#endif

