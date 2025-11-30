#include "app_sensor_manager.h"
#include "../bsp/sensors/bsp_sensors.h"
#include "app_data_logger.h"  // Para conversão de umidade
#include "esp_log.h"
#include <math.h>

static const char *TAG = "APP_SENSOR_MGR";
static bool initialized = false;

/**
 * @brief Calcula o Déficit de Pressão de Vapor (DPV) em kPa
 * 
 * DPV = es - ea
 * onde:
 *   es = pressão de vapor de saturação (kPa) - função da temperatura
 *   ea = pressão de vapor atual (kPa) = es * (umidade_relativa / 100)
 * 
 * Fórmula de Magnus para es (em kPa):
 *   es = 0.6108 * exp(17.27 * T / (T + 237.3))
 *   onde T é a temperatura em °C
 * 
 * @param temp_c Temperatura do ar em °C
 * @param humid_pct Umidade relativa do ar em %
 * @return DPV em kPa, ou NAN se dados inválidos
 */
static float calculate_dpv(float temp_c, float humid_pct)
{
    if (isnan(temp_c) || isnan(humid_pct) || 
        temp_c < -50.0f || temp_c > 60.0f ||
        humid_pct < 0.0f || humid_pct > 100.0f) {
        return NAN;
    }
    
    // Pressão de vapor de saturação (es) usando fórmula de Magnus
    float es = 0.6108f * expf(17.27f * temp_c / (temp_c + 237.3f));
    
    // Pressão de vapor atual (ea)
    float ea = es * (humid_pct / 100.0f);
    
    // Déficit de Pressão de Vapor
    float dpv = es - ea;
    
    return dpv;
}

esp_err_t sensor_manager_init(void)
{
    if (initialized) {
        return ESP_OK;
    }
    
    const bsp_sensors_ops_t *ops = bsp_sensors_get_ops();
    if (ops == NULL || ops->init == NULL) {
        ESP_LOGE(TAG, "BSP de sensores não disponível");
        return ESP_ERR_NOT_FOUND;
    }
    
    esp_err_t err = ops->init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar sensores BSP");
        return err;
    }
    
    initialized = true;
    ESP_LOGI(TAG, "Sensor Manager inicializado");
    return ESP_OK;
}

esp_err_t sensor_manager_read(sensor_reading_t *reading)
{
    if (reading == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!initialized) {
        ESP_LOGW(TAG, "Sensor Manager não inicializado");
        return ESP_ERR_INVALID_STATE;
    }
    
    const bsp_sensors_ops_t *ops = bsp_sensors_get_ops();
    bsp_sensor_data_t bsp_data = {0};
    
    // Lê todos os sensores via BSP
    if (ops->read_all != NULL) {
        ops->read_all(&bsp_data);
    } else {
        // Fallback: lê individualmente
        if (ops->read_temp_soil) ops->read_temp_soil(&bsp_data.temp_soil);
        if (ops->read_soil_raw) ops->read_soil_raw(&bsp_data.soil_raw);
    }
    
    // Converte para formato da aplicação
    reading->temp_air = bsp_data.temp_air;
    reading->humid_air = bsp_data.humid_air;
    reading->temp_soil = bsp_data.temp_soil;
    reading->luminosity = bsp_data.luminosity;
    
    // Converte umidade do solo de raw para %
    reading->humid_soil = data_logger_raw_to_pct(bsp_data.soil_raw);
    
    // Calcula DPV (Déficit de Pressão de Vapor) a partir de temperatura e umidade do ar
    reading->dpv = calculate_dpv(reading->temp_air, reading->humid_air);
    
    return ESP_OK;
}

bool sensor_manager_is_valid(const sensor_reading_t *reading)
{
    if (reading == NULL) return false;
    
    // Valida ranges razoáveis
    if (isnan(reading->temp_soil) || isnan(reading->humid_soil)) {
        return false;
    }
    
    if (reading->temp_soil < -40.0f || reading->temp_soil > 85.0f) {
        return false;  // DS18B20 range: -55°C a +125°C, mas validamos range prático
    }
    
    if (reading->humid_soil < 0.0f || reading->humid_soil > 100.0f) {
        return false;
    }
    
    return true;
}

// Funções de compatibilidade com código antigo
float sensor_manager_get_temp_ar(void)
{
    sensor_reading_t reading = {0};
    sensor_manager_read(&reading);
    return reading.temp_air;
}

float sensor_manager_get_umid_ar(void)
{
    sensor_reading_t reading = {0};
    sensor_manager_read(&reading);
    return reading.humid_air;
}

float sensor_manager_get_temp_solo(void)
{
    sensor_reading_t reading = {0};
    sensor_manager_read(&reading);
    return reading.temp_soil;
}

int sensor_manager_get_umid_solo_raw(void)
{
    const bsp_sensors_ops_t *ops = bsp_sensors_get_ops();
    if (ops == NULL || ops->read_soil_raw == NULL) {
        return -1;
    }
    
    int raw = -1;
    ops->read_soil_raw(&raw);
    return raw;
}

