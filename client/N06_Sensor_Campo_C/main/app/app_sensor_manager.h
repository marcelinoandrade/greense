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

// Funções de compatibilidade com código antigo
float sensor_manager_get_temp_ar(void);
float sensor_manager_get_umid_ar(void);
float sensor_manager_get_temp_solo(void);
int sensor_manager_get_umid_solo_raw(void);

#ifdef __cplusplus
}
#endif

