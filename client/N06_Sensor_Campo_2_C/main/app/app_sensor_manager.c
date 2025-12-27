#include "app_sensor_manager.h"
#include "../bsp/sensors/bsp_sensors.h"
#include "app_data_logger.h"  // Para conversão de umidade
#include "app_sampling_period.h"  // Para acessar período de amostragem
#include "esp_log.h"
#include <math.h>

static const char *TAG = "APP_SENSOR_MGR";
static bool initialized = false;

// Últimos valores válidos para detecção de outliers
static float last_valid_temp_air = NAN;
static float last_valid_humid_air = NAN;
static float last_valid_temp_soil = NAN;
static float last_valid_humid_soil = NAN;

/**
 * @brief Calcula o Déficit de Pressão de Vapor (DPV) em kPa
 * 
 * DPV = es - ea
 * onde:
 *   es = pressão de vapor de saturação (kPa) - função da temperatura
 *   ea = pressão de vapor atual (kPa) = es * (umidade_relativa / 100)
 * 
 * Fórmula de Buck para es (em kPa) - mais precisa para temperaturas altas:
 *   es = 0.61121 * exp((18.678 - T/234.5) * T / (257.14 + T))
 *   onde T é a temperatura em °C
 * 
 * A fórmula de Buck é mais adequada para condições tropicais e subtropicais,
 * como as encontradas no Brasil, especialmente em temperaturas acima de 0°C.
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
    
    // Pressão de vapor de saturação (es) usando fórmula de Buck
    // Mais precisa para temperaturas altas (adequada para clima brasileiro)
    float es = 0.61121f * expf((18.678f - temp_c / 234.5f) * temp_c / (257.14f + temp_c));
    
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
    
    // Valida NAN em todos os sensores críticos
    if (isnan(reading->temp_air) || isnan(reading->humid_air) ||
        isnan(reading->temp_soil) || isnan(reading->humid_soil)) {
        return false;
    }
    
    // Valida ranges para temperatura do ar (DHT11: -40°C a +80°C)
    if (reading->temp_air < -40.0f || reading->temp_air > 80.0f) {
        ESP_LOGW(TAG, "Temp. ar fora do range: %.1f°C", reading->temp_air);
        return false;
    }
    
    // Valida umidade do ar (0-100%)
    if (reading->humid_air < 0.0f || reading->humid_air > 100.0f) {
        ESP_LOGW(TAG, "Umidade ar fora do range: %.1f%%", reading->humid_air);
        return false;
    }
    
    // Valida temperatura do solo (DS18B20: -55°C a +125°C, range prático)
    if (reading->temp_soil < -40.0f || reading->temp_soil > 85.0f) {
        ESP_LOGW(TAG, "Temp. solo fora do range: %.1f°C", reading->temp_soil);
        return false;
    }
    
    // Valida umidade do solo (0-100%)
    if (reading->humid_soil < 0.0f || reading->humid_soil > 100.0f) {
        ESP_LOGW(TAG, "Umidade solo fora do range: %.1f%%", reading->humid_soil);
        return false;
    }
    
    // Valida luminosidade (BH1750: 1-65535 lux, mas valores negativos indicam erro)
    if (reading->luminosity < 0.0f || reading->luminosity > 100000.0f) {
        ESP_LOGW(TAG, "Luminosidade fora do range: %.0f lux", reading->luminosity);
        return false;
    }
    
    return true;
}

/**
 * @brief Calcula limite máximo de mudança permitida baseado no período de amostragem
 * 
 * Para períodos curtos (10s-1min): limites restritivos (detecta erros imediatos)
 * Para períodos médios (10min-1h): limites moderados
 * Para períodos longos (6h-12h): limites permissivos (permite variações naturais)
 */
static void get_outlier_limits_for_period(uint32_t period_ms, 
                                          float *max_temp_change, 
                                          float *max_humid_change)
{
    // Taxa de mudança máxima esperada por hora
    // Baseado em variações climáticas típicas:
    // - Temperatura: até 15°C/hora em condições extremas, normalmente 5-10°C/hora
    // - Umidade: até 30%/hora em condições extremas, normalmente 10-20%/hora
    
    float hours = (float)period_ms / (1000.0f * 3600.0f);
    
    // Limites base (por hora)
    float base_temp_change_per_hour = 12.0f;  // °C/hora
    float base_humid_change_per_hour = 25.0f; // %/hora
    
    // Para períodos muito curtos (< 1 min), usa limites fixos restritivos
    if (period_ms < 60 * 1000) {
        *max_temp_change = 5.0f;   // Máximo 5°C de mudança em < 1 min
        *max_humid_change = 10.0f; // Máximo 10% de mudança em < 1 min
        return;
    }
    
    // Para períodos médios (1 min - 1 hora), escala linear
    if (hours < 1.0f) {
        *max_temp_change = 5.0f + (hours * 7.0f);  // 5°C a 12°C
        *max_humid_change = 10.0f + (hours * 15.0f); // 10% a 25%
        return;
    }
    
    // Para períodos longos (>= 1 hora), escala proporcional ao tempo
    // Com margem de segurança de 1.5x para variações climáticas extremas
    *max_temp_change = base_temp_change_per_hour * hours * 1.5f;
    *max_humid_change = base_humid_change_per_hour * hours * 1.5f;
    
    // Limites máximos absolutos (mesmo em 12h, não esperamos > 50°C de mudança)
    if (*max_temp_change > 50.0f) *max_temp_change = 50.0f;
    if (*max_humid_change > 100.0f) *max_humid_change = 100.0f;
}

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
                                                     uint32_t sampling_period_ms)
{
    if (reading == NULL) return false;
    
    // Primeiro, validação básica (ranges e NAN)
    if (!sensor_manager_is_valid(reading)) {
        return false;
    }
    
    // Detecção de outliers adaptativa (só se houver último valor válido)
    float max_temp_change, max_humid_change;
    get_outlier_limits_for_period(sampling_period_ms, &max_temp_change, &max_humid_change);
    
    // Valida temperatura do ar
    if (!isnan(last_valid_temp_air)) {
        float temp_diff = fabsf(reading->temp_air - last_valid_temp_air);
        if (temp_diff > max_temp_change) {
            ESP_LOGW(TAG, "Outlier temp_ar: mudança de %.1f°C (limite: %.1f°C para período de %lu ms)",
                     temp_diff, max_temp_change, (unsigned long)sampling_period_ms);
            return false;
        }
    }
    
    // Valida umidade do ar
    if (!isnan(last_valid_humid_air)) {
        float humid_diff = fabsf(reading->humid_air - last_valid_humid_air);
        if (humid_diff > max_humid_change) {
            ESP_LOGW(TAG, "Outlier umid_ar: mudança de %.1f%% (limite: %.1f%% para período de %lu ms)",
                     humid_diff, max_humid_change, (unsigned long)sampling_period_ms);
            return false;
        }
    }
    
    // Valida temperatura do solo
    if (!isnan(last_valid_temp_soil)) {
        float temp_diff = fabsf(reading->temp_soil - last_valid_temp_soil);
        if (temp_diff > max_temp_change) {
            ESP_LOGW(TAG, "Outlier temp_solo: mudança de %.1f°C (limite: %.1f°C para período de %lu ms)",
                     temp_diff, max_temp_change, (unsigned long)sampling_period_ms);
            return false;
        }
    }
    
    // Valida umidade do solo
    if (!isnan(last_valid_humid_soil)) {
        float humid_diff = fabsf(reading->humid_soil - last_valid_humid_soil);
        if (humid_diff > max_humid_change) {
            ESP_LOGW(TAG, "Outlier umid_solo: mudança de %.1f%% (limite: %.1f%% para período de %lu ms)",
                     humid_diff, max_humid_change, (unsigned long)sampling_period_ms);
            return false;
        }
    }
    
    // Atualiza últimos valores válidos
    last_valid_temp_air = reading->temp_air;
    last_valid_humid_air = reading->humid_air;
    last_valid_temp_soil = reading->temp_soil;
    last_valid_humid_soil = reading->humid_soil;
    
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

