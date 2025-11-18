#include "aht20.h"
#include "freertos/task.h"
#include "esp_timer.h"

static const char *TAG = "AHT20";

esp_err_t aht20_write(aht20_t *dev, uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(dev->i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t aht20_read_bytes(aht20_t *dev, uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->address << 1) | I2C_MASTER_READ, true);
    if (len > 1) i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(dev->i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static bool aht20_wait_for_idle(aht20_t *dev) {
    int64_t timeout = esp_timer_get_time() + (AHT20_IDLE_TIMEOUT_MS * 1000);
    while (esp_timer_get_time() < timeout) {
        if (aht20_read_bytes(dev, dev->buffer, 1) == ESP_OK) {
            if (!(dev->buffer[0] & AHT20_STATUS_BUSY)) {
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGE(TAG, "Timeout after %dms (status: 0x%02x)", AHT20_IDLE_TIMEOUT_MS, dev->buffer[0]);
    return false;
}

bool aht20_init(aht20_t *dev, i2c_port_t i2c_port, uint8_t address) {
    dev->i2c_port = i2c_port;
    dev->address = address;
    
    for (int i = 0; i < AHT20_INIT_RETRIES; i++) {
        aht20_reset(dev);
        vTaskDelay(pdMS_TO_TICKS(50));

        uint8_t init_cmd[3] = {AHT20_CMD_INITIALIZE, 0x08, 0x00};
        if (aht20_write(dev, init_cmd, sizeof(init_cmd)) == ESP_OK) {
            if (aht20_wait_for_idle(dev)) {
                if (aht20_read_bytes(dev, dev->buffer, 1) == ESP_OK) {
                    if (dev->buffer[0] & AHT20_STATUS_CALIBRATED) {
                        ESP_LOGI(TAG, "Initialized successfully (attempt %d)", i+1);
                        return true;
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGE(TAG, "Initialization failed after %d attempts", AHT20_INIT_RETRIES);
    return false;
}

void aht20_reset(aht20_t *dev) {
    uint8_t reset_cmd = AHT20_CMD_SOFTRESET;
    aht20_write(dev, &reset_cmd, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
}

bool aht20_read(aht20_t *dev, float *temperature, float *humidity) {
    uint8_t trigger_cmd[3] = {AHT20_CMD_TRIGGER, 0x33, 0x00};
    if (aht20_write(dev, trigger_cmd, sizeof(trigger_cmd)) != ESP_OK) {
        ESP_LOGE(TAG, "Trigger command failed");
        return false;
    }

    if (!aht20_wait_for_idle(dev)) {
        return false;
    }

    if (aht20_read_bytes(dev, dev->buffer, 6) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read measurement data");
        return false;
    }

    uint32_t raw_humidity = ((uint32_t)dev->buffer[1] << 12) | 
                           ((uint32_t)dev->buffer[2] << 4) | 
                           ((uint32_t)dev->buffer[3] >> 4);
    dev->humidity = (raw_humidity * 100.0f) / (1 << 20);

    uint32_t raw_temp = ((uint32_t)(dev->buffer[3] & 0x0F) << 16) | 
                       ((uint32_t)dev->buffer[4] << 8) | 
                       dev->buffer[5];
    dev->temperature = ((raw_temp * 200.0f) / (1 << 20)) - 50.0f;

    if (temperature) *temperature = dev->temperature;
    if (humidity) *humidity = dev->humidity;

    return true;
}

void aht20_scan_i2c_bus(i2c_port_t port) {
    ESP_LOGI(TAG, "Scanning I2C bus...");
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Found device at 0x%02X", addr);
        }
    }
}