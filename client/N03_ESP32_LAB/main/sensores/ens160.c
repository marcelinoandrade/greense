#include "ens160.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/task.h"

#define ENS160_OPMODE_REG 0x10
#define ENS160_TEMP_IN_REG 0x13
#define ENS160_RH_IN_REG 0x15
#define ENS160_DATA_ECO2_REG 0x24

static const char *TAG = "ENS160";

static esp_err_t ens160_write_reg(ens160_t *dev, uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    return i2c_master_write_to_device(dev->i2c_port, ENS160_ADDR, buf, sizeof(buf), pdMS_TO_TICKS(100));
}

static esp_err_t ens160_read_reg(ens160_t *dev, uint8_t reg, uint8_t *value, size_t len) {
    return i2c_master_write_read_device(dev->i2c_port, ENS160_ADDR, &reg, 1, value, len, pdMS_TO_TICKS(100));
}

bool ens160_init(ens160_t *dev, i2c_port_t i2c_port) {
    dev->i2c_port = i2c_port;
    dev->initialized = false;
    
    if (ens160_write_reg(dev, ENS160_OPMODE_REG, ENS160_STANDARD_MODE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set standard mode");
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(500));
    dev->initialized = true;
    ESP_LOGI(TAG, "Initialized successfully");
    return true;
}

void ens160_calibrate(ens160_t *dev, float temperature, float humidity) {
    if (!dev->initialized) return;

    // Temperature calibration (convert to Kelvin * 64)
    uint16_t temp_cal = (uint16_t)((temperature + 273.15) * 64);
    uint8_t buf_temp[3] = {ENS160_TEMP_IN_REG, temp_cal & 0xFF, (temp_cal >> 8) & 0xFF};
    i2c_master_write_to_device(dev->i2c_port, ENS160_ADDR, buf_temp, sizeof(buf_temp), pdMS_TO_TICKS(100));

    // Humidity calibration (convert to % * 512)
    uint16_t hum_cal = (uint16_t)(humidity * 512);
    uint8_t buf_hum[3] = {ENS160_RH_IN_REG, hum_cal & 0xFF, (hum_cal >> 8) & 0xFF};
    i2c_master_write_to_device(dev->i2c_port, ENS160_ADDR, buf_hum, sizeof(buf_hum), pdMS_TO_TICKS(100));

    vTaskDelay(pdMS_TO_TICKS(100));
}

float ens160_get_eco2(ens160_t *dev) {
    if (!dev->initialized) return -1.0;

    uint8_t buf[2];
    if (ens160_read_reg(dev, ENS160_DATA_ECO2_REG, buf, sizeof(buf)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ECO2");
        return -1.0;
    }
    
    float eco2 = (float)((buf[1] << 8) | buf[0]);
    ESP_LOGD(TAG, "ECO2: %.1f ppm", eco2);
    return eco2;
}