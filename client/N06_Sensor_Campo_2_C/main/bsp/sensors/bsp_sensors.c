#include "bsp_sensors.h"
#include "bsp_ds18b20.h"
#include "bsp_adc.h"
#include "bsp_dht11.h"
#include "bsp_bh1750.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

static esp_err_t bsp_sensors_read_dht11_cached(float *temp, float *humid)
{
    static TickType_t last_read_ticks = 0;
    static float cached_temp = NAN;
    static float cached_humid = NAN;

    const TickType_t now = xTaskGetTickCount();
    const TickType_t min_interval = pdMS_TO_TICKS(2000); // DHT11 precisa de ~2s entre leituras

    if (last_read_ticks != 0 &&
        (now - last_read_ticks) < min_interval &&
        isfinite(cached_temp) && isfinite(cached_humid)) {
        if (temp) *temp = cached_temp;
        if (humid) *humid = cached_humid;
        return ESP_OK;
    }

    float t = NAN, h = NAN;
    esp_err_t err = dht11_bsp_read(&t, &h);
    if (err == ESP_OK) {
        cached_temp = t;
        cached_humid = h;
        last_read_ticks = now;
        last_temp_air = t;
        last_humid_air = h;
        if (temp) *temp = t;
        if (humid) *humid = h;
        return ESP_OK;
    }

    // Retorna último valor válido em caso de falha
    if (temp) *temp = cached_temp;
    if (humid) *humid = cached_humid;
    return err;
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

    /* Inicializa DHT11 (temperatura e umidade do ar) */
    err = dht11_bsp_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DHT11 não disponível: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "DHT11 inicializado com sucesso");
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
    return bsp_sensors_read_dht11_cached(temp, NULL);
}

static esp_err_t bsp_sensors_read_humid_air_impl(float *humid)
{
    if (humid == NULL) return ESP_ERR_INVALID_ARG;
    return bsp_sensors_read_dht11_cached(NULL, humid);
}

static esp_err_t bsp_sensors_read_all_impl(bsp_sensor_data_t *data)
{
    if (data == NULL) return ESP_ERR_INVALID_ARG;
    
    /* Lê temperatura e umidade do ar do DHT11 */
    float temp_air = NAN;
    float humid_air = NAN;
    bsp_sensors_read_dht11_cached(&temp_air, &humid_air);
    data->temp_air = temp_air;
    data->humid_air = humid_air;
    
    /* Lê luminosidade do BH1750 (mantém último valor em caso de falha) */
    float luminosity = last_luminosity;
    if (bh1750_bsp_is_available()) {
        float lux_read = NAN;
        esp_err_t err = bh1750_bsp_read(&lux_read);
        if (err == ESP_OK) {
            last_luminosity = lux_read;
            luminosity = lux_read;
        }
    }
    data->luminosity = luminosity;
    
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

