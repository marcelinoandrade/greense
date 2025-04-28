#ifndef AHT20_H
#define AHT20_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"
#include "esp_log.h"

#define AHT20_I2C_ADDR_DEFAULT     0x38
#define AHT20_CMD_INITIALIZE       0xBE
#define AHT20_CMD_TRIGGER          0xAC
#define AHT20_CMD_SOFTRESET        0xBA
#define AHT20_STATUS_BUSY          0x80
#define AHT20_STATUS_CALIBRATED    0x08
#define AHT20_INIT_RETRIES         3
#define AHT20_IDLE_TIMEOUT_MS      2000

typedef struct {
    i2c_port_t i2c_port;
    uint8_t address;
    uint8_t buffer[6];
    float temperature;
    float humidity;
} aht20_t;

esp_err_t aht20_write(aht20_t *dev, uint8_t *data, size_t len);
esp_err_t aht20_read_bytes(aht20_t *dev, uint8_t *data, size_t len);
bool aht20_init(aht20_t *dev, i2c_port_t i2c_port, uint8_t address);
void aht20_reset(aht20_t *dev);
bool aht20_read(aht20_t *dev, float *temperature, float *humidity);
void aht20_scan_i2c_bus(i2c_port_t port);

#endif