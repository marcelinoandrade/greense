#include "bsp_sensors.h"
#include "bsp_ds18b20.h"
#include "bsp_adc.h"
#include "bsp_aht10.h"
#include "bsp_bh1750.h"
#include "esp_log.h"
#include "esp_err.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

static const char *TAG = "BSP_SENSORS";

/* Cache de última leitura válida */
static float last_temp_soil = NAN;
static int last_soil_raw = -1;
static float last_temp_air = NAN;
static float last_humid_air = NAN;
static float last_luminosity = NAN;

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

    /* Inicializa AHT10 (temperatura e umidade do ar) */
    err = aht10_bsp_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "AHT10 não disponível: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "AHT10 inicializado com sucesso");
        
        /* Compartilha o barramento I2C com BH1750 */
        void* i2c_bus = aht10_get_i2c_bus_handle();
        if (i2c_bus != NULL) {
            bh1750_set_shared_i2c_bus((void*)i2c_bus);
        }
    }

    /* Inicializa BH1750 (luxímetro) */
    err = bh1750_bsp_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BH1750 não disponível: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "BH1750 inicializado com sucesso");
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

/* Funções auxiliares para ler temperatura/umidade do ar e luminosidade */
static esp_err_t bsp_sensors_read_temp_air_impl(float *temp)
{
    if (temp == NULL) return ESP_ERR_INVALID_ARG;
    
    if (aht10_bsp_is_available()) {
        float temp_val, humid_val;
        esp_err_t err = aht10_bsp_read(&temp_val, &humid_val);
        if (err == ESP_OK) {
            last_temp_air = temp_val;
            *temp = temp_val;
            return ESP_OK;
        }
    }
    
    /* Retorna último valor válido ou NAN se não houver */
    *temp = last_temp_air;
    return ESP_ERR_INVALID_RESPONSE;
}

static esp_err_t bsp_sensors_read_humid_air_impl(float *humid)
{
    if (humid == NULL) return ESP_ERR_INVALID_ARG;
    
    if (aht10_bsp_is_available()) {
        float temp_val, humid_val;
        esp_err_t err = aht10_bsp_read(&temp_val, &humid_val);
        if (err == ESP_OK) {
            last_humid_air = humid_val;
            *humid = humid_val;
            return ESP_OK;
        }
    }
    
    /* Retorna último valor válido ou NAN se não houver */
    *humid = last_humid_air;
    return ESP_ERR_INVALID_RESPONSE;
}

static esp_err_t bsp_sensors_read_all_impl(bsp_sensor_data_t *data)
{
    if (data == NULL) return ESP_ERR_INVALID_ARG;
    
    /* Lê temperatura e umidade do ar do AHT10 */
    float temp_air = NAN;
    float humid_air = NAN;
    bsp_sensors_read_temp_air_impl(&temp_air);
    bsp_sensors_read_humid_air_impl(&humid_air);
    data->temp_air = temp_air;
    data->humid_air = humid_air;
    
    /* Lê luminosidade do BH1750 */
    float luminosity = NAN;
    if (bh1750_bsp_is_available()) {
        esp_err_t err = bh1750_bsp_read(&luminosity);
        if (err == ESP_OK) {
            last_luminosity = luminosity;
            data->luminosity = luminosity;
        } else {
            /* Usa último valor válido ou NAN */
            data->luminosity = last_luminosity;
        }
    } else {
        /* Usa último valor válido ou NAN */
        data->luminosity = last_luminosity;
    }
    
    /* Temperatura do solo */
    float temp_soil = NAN;
    bsp_sensors_read_temp_soil_impl(&temp_soil);
    data->temp_soil = temp_soil;
    
    /* Umidade do solo (raw) */
    int soil_raw = -1;
    bsp_sensors_read_soil_raw_impl(&soil_raw);
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
    .read_temp_air = bsp_sensors_read_temp_air_impl,
    .read_humid_air = bsp_sensors_read_humid_air_impl,
    .read_temp_soil = bsp_sensors_read_temp_soil_impl,
    .read_soil_raw = bsp_sensors_read_soil_raw_impl,
    .is_ready = bsp_sensors_is_ready_impl,
};

const bsp_sensors_ops_t* bsp_sensors_get_ops(void)
{
    return &sensors_ops;
}

