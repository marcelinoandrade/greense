#include "bsp_sensors.h"
#include "bsp_ds18b20.h"
#include "bsp_adc.h"
#include "esp_log.h"
#include "esp_random.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

static const char *TAG = "BSP_SENSORS";

/* Cache de última leitura válida */
static float last_temp_soil = NAN;
static int last_soil_raw = -1;

/* Geração de dados simulados para sensores sem hardware */
static float simulated_temp_air = 26.0f;
static float simulated_humid_air = 60.0f;
static float simulated_temp_soil = 24.0f;
static int   simulated_soil_raw  = 2500;

static float random_between(float min, float max)
{
    if (max <= min) {
        return min;
    }

    const float range = max - min;
    float frac = (float)esp_random() / (float)UINT32_MAX;
    return min + frac * range;
}

static float update_simulated_value(float current, float min, float max, float max_delta)
{
    float next = current + random_between(-max_delta, max_delta);
    if (next < min) next = min;
    if (next > max) next = max;
    return next;
}

static int update_simulated_raw(int current, int min, int max, int max_delta)
{
    int next = current + (int)random_between(-max_delta, max_delta);
    if (next < min) next = min;
    if (next > max) next = max;
    return next;
}

/* Implementação das operações */
static esp_err_t bsp_sensors_init_impl(void)
{
    esp_err_t err;
    
    err = ds18b20_bsp_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar DS18B20");
        return err;
    }
    
    err = adc_bsp_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar ADC");
        return err;
    }
    
    ESP_LOGI(TAG, "Sensores BSP inicializados");
    return ESP_OK;
}

static esp_err_t bsp_sensors_read_temp_soil_impl(float *temp)
{
    if (temp == NULL) return ESP_ERR_INVALID_ARG;
    
    float t = ds18b20_bsp_read_temperature();
    if (t <= -126.0f) {
        // Erro na leitura, retorna último valor válido
        *temp = isnan(last_temp_soil) ? NAN : last_temp_soil;
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    last_temp_soil = t;
    *temp = t;
    return ESP_OK;
}

static esp_err_t bsp_sensors_read_soil_raw_impl(int *raw)
{
    if (raw == NULL) return ESP_ERR_INVALID_ARG;
    
    int value = 0;
    esp_err_t err = adc_bsp_read_soil(&value);
    if (err != ESP_OK) {
        *raw = last_soil_raw >= 0 ? last_soil_raw : -1;
        return err;
    }
    
    last_soil_raw = value;
    *raw = value;
    return ESP_OK;
}

static esp_err_t bsp_sensors_read_all_impl(bsp_sensor_data_t *data)
{
    if (data == NULL) return ESP_ERR_INVALID_ARG;
    
    // Atualiza valores simulados para sensores que não existem no hardware
    simulated_temp_air  = update_simulated_value(simulated_temp_air, 20.0f, 40.0f, 0.5f);
    simulated_humid_air = update_simulated_value(simulated_humid_air, 0.0f, 100.0f, 2.5f);

    data->temp_air  = simulated_temp_air;
    data->humid_air = simulated_humid_air;
    
    // Temperatura do solo (gera simulação se não houver hardware)
    float temp_soil = NAN;
    if (bsp_sensors_read_temp_soil_impl(&temp_soil) != ESP_OK || isnan(temp_soil)) {
        simulated_temp_soil = update_simulated_value(simulated_temp_soil, 20.0f, 40.0f, 0.4f);
        temp_soil = simulated_temp_soil;
    }
    data->temp_soil = temp_soil;
    
    // Umidade do solo (raw). Simula caso ADC indisponível
    int soil_raw = -1;
    if (bsp_sensors_read_soil_raw_impl(&soil_raw) != ESP_OK || soil_raw < 0) {
        simulated_soil_raw = update_simulated_raw(simulated_soil_raw, 400, 4000, 150);
        soil_raw = simulated_soil_raw;
    }
    data->soil_raw = soil_raw;
    
    return ESP_OK;
}

static bool bsp_sensors_is_ready_impl(void)
{
    return true; // TODO: implementar verificação real
}

/* Estrutura de operações */
static const bsp_sensors_ops_t sensors_ops = {
    .init = bsp_sensors_init_impl,
    .read_all = bsp_sensors_read_all_impl,
    .read_temp_air = NULL, // TODO: implementar quando tiver sensor
    .read_humid_air = NULL,
    .read_temp_soil = bsp_sensors_read_temp_soil_impl,
    .read_soil_raw = bsp_sensors_read_soil_raw_impl,
    .is_ready = bsp_sensors_is_ready_impl,
};

const bsp_sensors_ops_t* bsp_sensors_get_ops(void)
{
    return &sensors_ops;
}

