#include "bsp_bh1750.h"
#include "../board.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "BSP_BH1750";

/* Endereço I2C do BH1750 (ADDR pin conectado a GND) */
#define BH1750_I2C_ADDR        0x23

/* Comandos do BH1750 */
#define BH1750_CMD_POWER_DOWN  0x00
#define BH1750_CMD_POWER_ON    0x01
#define BH1750_CMD_RESET       0x07
#define BH1750_CMD_CONT_H_MODE 0x10  // Modo contínuo alta resolução (1 lux, ~120ms)
#define BH1750_CMD_CONT_H_MODE2 0x11 // Modo contínuo alta resolução 2 (0.5 lux, ~120ms)
#define BH1750_CMD_CONT_L_MODE 0x13  // Modo contínuo baixa resolução (4 lux, ~16ms)

static i2c_master_dev_handle_t bh1750_handle = NULL;
static i2c_master_bus_handle_t i2c_bus_handle_shared = NULL;
static bool initialized = false;

esp_err_t bh1750_bsp_init(void)
{
    if (initialized) {
        return ESP_OK;
    }

    esp_err_t ret;

    /* Verifica se o barramento I2C compartilhado foi configurado */
    /* Se não, cria um novo (mas idealmente deve ser compartilhado com AHT10) */
    if (i2c_bus_handle_shared == NULL) {
        ESP_LOGW(TAG, "Barramento I2C não compartilhado, criando novo...");
        /* Configuração do barramento I2C */
        i2c_master_bus_config_t i2c_bus_config = {
            .i2c_port = BSP_I2C_NUM,
            .sda_io_num = BSP_I2C_SDA,
            .scl_io_num = BSP_I2C_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags = {
                .enable_internal_pullup = true,
            },
        };

        ret = i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle_shared);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Falha ao criar barramento I2C: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    /* Configuração do dispositivo BH1750 */
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BH1750_I2C_ADDR,
        .scl_speed_hz = 100000,  // 100kHz
    };

    ret = i2c_master_bus_add_device(i2c_bus_handle_shared, &dev_config, &bh1750_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao adicionar dispositivo BH1750: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Power on */
    uint8_t cmd = BH1750_CMD_POWER_ON;
    ret = i2c_master_transmit(bh1750_handle, &cmd, 1, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao ligar BH1750: %s", esp_err_to_name(ret));
        i2c_master_bus_rm_device(bh1750_handle);
        bh1750_handle = NULL;
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Reset */
    cmd = BH1750_CMD_RESET;
    ret = i2c_master_transmit(bh1750_handle, &cmd, 1, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Reset falhou, continuando...");
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Configura modo contínuo alta resolução */
    cmd = BH1750_CMD_CONT_H_MODE;
    ret = i2c_master_transmit(bh1750_handle, &cmd, 1, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao configurar modo contínuo: %s", esp_err_to_name(ret));
        i2c_master_bus_rm_device(bh1750_handle);
        bh1750_handle = NULL;
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(120));  // Aguarda primeira medição

    initialized = true;
    ESP_LOGI(TAG, "BH1750 inicializado com sucesso (I2C addr: 0x%02X)", BH1750_I2C_ADDR);
    return ESP_OK;
}

esp_err_t bh1750_bsp_read(float *lux)
{
    if (!initialized || bh1750_handle == NULL) {
        ESP_LOGW(TAG, "BH1750 não inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    if (lux == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret;

    /* Lê 2 bytes de dados */
    uint8_t data[2] = {0};
    ret = i2c_master_receive(bh1750_handle, data, 2, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao ler dados do BH1750: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Converte para lux */
    uint16_t raw = ((uint16_t)data[0] << 8) | (uint16_t)data[1];
    *lux = (float)raw / 1.2f;  // Divisão por 1.2 para modo de alta resolução

    return ESP_OK;
}

bool bh1750_bsp_is_available(void)
{
    return initialized && (bh1750_handle != NULL);
}

/* Função para compartilhar o barramento I2C com AHT10 */
void bh1750_set_shared_i2c_bus(void* bus_handle)
{
    i2c_bus_handle_shared = (i2c_master_bus_handle_t)bus_handle;
}

