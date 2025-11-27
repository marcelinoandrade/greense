#include "bsp_sensors.h"
#include "bsp_ds18b20.h"
#include "bsp_adc.h"
#include "esp_log.h"
#include <math.h>
#include <stdbool.h>

static const char *TAG = "BSP_SENSORS";

/* Cache de última leitura válida */
static float last_temp_soil = NAN;
static int last_soil_raw = -1;

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
    
    // Temperatura do ar (placeholder - implementar quando tiver sensor)
    data->temp_air = 25.0f;
    
    // Umidade do ar (placeholder)
    data->humid_air = 50.0f;
    
    // Temperatura do solo
    bsp_sensors_read_temp_soil_impl(&data->temp_soil);
    
    // Umidade do solo (raw)
    bsp_sensors_read_soil_raw_impl(&data->soil_raw);
    
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

