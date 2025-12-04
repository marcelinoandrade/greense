#include "bsp_aht10.h"
#include "../board.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "BSP_AHT10";

/* Endereço I2C do AHT10 */
#define AHT10_I2C_ADDR        0x38

/* Comandos do AHT10 */
#define AHT10_CMD_INIT        0xBE
#define AHT10_CMD_TRIGGER     0xAC
#define AHT10_CMD_SOFT_RESET  0xBA
#define AHT10_CMD_STATUS      0x71

/* Parâmetros do comando trigger */
#define AHT10_TRIGGER_PARAM1  0x33
#define AHT10_TRIGGER_PARAM2  0x00

static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static i2c_master_dev_handle_t aht10_handle = NULL;
static bool initialized = false;

esp_err_t aht10_bsp_init(void)
{
    if (initialized) {
        return ESP_OK;
    }

    esp_err_t ret;

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

    ret = i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar barramento I2C: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Configuração do dispositivo AHT10 */
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AHT10_I2C_ADDR,
        .scl_speed_hz = 100000,  // 100kHz (padrão para AHT10)
    };

    ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_config, &aht10_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao adicionar dispositivo AHT10: %s", esp_err_to_name(ret));
        i2c_del_master_bus(i2c_bus_handle);
        i2c_bus_handle = NULL;
        return ret;
    }

    /* Soft reset do sensor */
    uint8_t reset_cmd = AHT10_CMD_SOFT_RESET;
    ret = i2c_master_transmit(aht10_handle, &reset_cmd, 1, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Soft reset falhou, continuando...");
    }
    vTaskDelay(pdMS_TO_TICKS(20));

    /* Inicialização do sensor */
    uint8_t init_cmd[3] = {AHT10_CMD_INIT, 0x08, 0x00};
    ret = i2c_master_transmit(aht10_handle, init_cmd, 3, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar AHT10: %s", esp_err_to_name(ret));
        i2c_master_bus_rm_device(aht10_handle);
        i2c_del_master_bus(i2c_bus_handle);
        aht10_handle = NULL;
        i2c_bus_handle = NULL;
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    initialized = true;
    ESP_LOGI(TAG, "AHT10 inicializado com sucesso (I2C addr: 0x%02X)", AHT10_I2C_ADDR);
    return ESP_OK;
}

esp_err_t aht10_bsp_read(float *temp_air, float *humid_air)
{
    if (!initialized || aht10_handle == NULL) {
        ESP_LOGW(TAG, "AHT10 não inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    if (temp_air == NULL || humid_air == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret;

    /* Envia comando de trigger para iniciar medição */
    uint8_t trigger_cmd[3] = {AHT10_CMD_TRIGGER, AHT10_TRIGGER_PARAM1, AHT10_TRIGGER_PARAM2};
    ret = i2c_master_transmit(aht10_handle, trigger_cmd, 3, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao enviar comando trigger: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Aguarda conversão (máximo 80ms) */
    vTaskDelay(pdMS_TO_TICKS(80));

    /* Lê 6 bytes de dados */
    uint8_t data[6] = {0};
    ret = i2c_master_receive(aht10_handle, data, 6, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao ler dados do AHT10: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Verifica status byte (primeiro byte) */
    if ((data[0] & 0x80) != 0) {
        ESP_LOGW(TAG, "AHT10 ainda está medindo (bit 7 = 1)");
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* Converte umidade (20 bits: data[1], data[2], data[3] bits 7:4) */
    uint32_t humid_raw = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | ((uint32_t)data[3] >> 4);
    *humid_air = ((float)humid_raw / 1048576.0f) * 100.0f;  // 2^20 = 1048576

    /* Converte temperatura (20 bits: data[3] bits 3:0, data[4], data[5]) */
    uint32_t temp_raw = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[5];
    *temp_air = ((float)temp_raw / 1048576.0f) * 200.0f - 50.0f;

    return ESP_OK;
}

bool aht10_bsp_is_available(void)
{
    return initialized && (aht10_handle != NULL);
}

void* aht10_get_i2c_bus_handle(void)
{
    return (void*)i2c_bus_handle;
}

