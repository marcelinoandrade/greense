#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Interface abstrata de sensores (BSP)
 * ============================================================
 * Esta interface permite trocar implementações de hardware
 * sem alterar o código da aplicação.
 */

typedef struct {
    float temp_air;      // °C
    float humid_air;     // % (0-100)
    float temp_soil;     // °C
    int   soil_raw;      // ADC raw (0-4095)
    float luminosity;    // lux (intensidade de luminosidade)
} bsp_sensor_data_t;

typedef struct {
    /* Inicialização */
    esp_err_t (*init)(void);
    
    /* Leitura de sensores */
    esp_err_t (*read_all)(bsp_sensor_data_t *data);
    esp_err_t (*read_temp_air)(float *temp);
    esp_err_t (*read_humid_air)(float *humid);
    esp_err_t (*read_temp_soil)(float *temp);
    esp_err_t (*read_soil_raw)(int *raw);
    
    /* Status */
    bool (*is_ready)(void);
} bsp_sensors_ops_t;

/**
 * @brief Obtém a implementação de sensores para esta placa
 * @return Ponteiro para estrutura de operações (não deve ser NULL)
 */
const bsp_sensors_ops_t* bsp_sensors_get_ops(void);

#ifdef __cplusplus
}
#endif

