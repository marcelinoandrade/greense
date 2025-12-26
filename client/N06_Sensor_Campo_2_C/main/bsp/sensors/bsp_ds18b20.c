#include "bsp_ds18b20.h"
#include "../board.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdint.h>
#include <math.h>

static const char *TAG = "BSP_DS18B20";

/* Comandos 1-Wire */
#define CMD_SKIP_ROM      0xCC
#define CMD_CONVERT_T     0x44
#define CMD_READ_SCRATCH  0xBE

static gpio_num_t ds_gpio = BSP_GPIO_DS18B20;
static bool initialized = false;

/* delay em microssegundos */
static void ds_delay_us(int us) {
    esp_rom_delay_us(us);
}

/* Pull-down e release do barramento 1-Wire (bit-bang) */
static void ds_write_bit(int bit) {
    gpio_set_direction(ds_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(ds_gpio, 0);
    ds_delay_us(2);

    if (bit) {
        gpio_set_level(ds_gpio, 1);
    }

    ds_delay_us(60);
    gpio_set_level(ds_gpio, 1);
    ds_delay_us(2);
}

static int ds_read_bit(void) {
    int r;
    gpio_set_direction(ds_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(ds_gpio, 0);
    ds_delay_us(2);

    gpio_set_direction(ds_gpio, GPIO_MODE_INPUT);
    ds_delay_us(8);

    r = gpio_get_level(ds_gpio);
    ds_delay_us(60);
    return r;
}

static void ds_write_byte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        ds_write_bit(data & 0x1);
        data >>= 1;
    }
}

static uint8_t ds_read_byte(void) {
    uint8_t r = 0;
    for (int i = 0; i < 8; i++) {
        r >>= 1;
        if (ds_read_bit()) {
            r |= 0x80;
        }
    }
    return r;
}

/* Reset 1-Wire. Retorna 1 se OK, 0 se erro (sem presença). */
static int ds_reset(void) {
    gpio_set_direction(ds_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(ds_gpio, 0);
    ds_delay_us(480);

    gpio_set_level(ds_gpio, 1);
    gpio_set_direction(ds_gpio, GPIO_MODE_INPUT);
    ds_delay_us(70);

    int presence = !gpio_get_level(ds_gpio);
    ds_delay_us(410);

    return presence;
}

esp_err_t ds18b20_bsp_init(void)
{
    if (initialized) {
        return ESP_OK;
    }
    
    ds_gpio = BSP_GPIO_DS18B20;
    gpio_set_direction(ds_gpio, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(ds_gpio, GPIO_PULLUP_ONLY);
    
    initialized = true;
    ESP_LOGI(TAG, "DS18B20 inicializado no GPIO %d", ds_gpio);
    return ESP_OK;
}

float ds18b20_bsp_read_temperature(void)
{
    if (!initialized) {
        ESP_LOGE(TAG, "DS18B20 não inicializado");
        return -127.0f;
    }
    
    // 1. Reset do barramento 1-Wire
    if (!ds_reset()) {
        ESP_LOGW(TAG, "DS18B20: nenhum sensor presente (reset falhou)");
        return -127.0f;
    }
    
    // 2. Skip ROM (0xCC) - para um único sensor no barramento
    ds_write_byte(CMD_SKIP_ROM);
    
    // 3. Convert Temperature (0x44) - inicia conversão
    ds_write_byte(CMD_CONVERT_T);
    
    // 4. Aguarda conversão (750ms para resolução de 12 bits)
    vTaskDelay(pdMS_TO_TICKS(750));
    
    // 5. Reset novamente
    if (!ds_reset()) {
        ESP_LOGW(TAG, "DS18B20: reset falhou após conversão");
        return -127.0f;
    }
    
    // 6. Skip ROM novamente
    ds_write_byte(CMD_SKIP_ROM);
    
    // 7. Read Scratchpad (0xBE) - lê os dados
    ds_write_byte(CMD_READ_SCRATCH);
    
    // 8. Lê 9 bytes do scratchpad
    uint8_t scratchpad[9];
    for (int i = 0; i < 9; i++) {
        scratchpad[i] = ds_read_byte();
    }
    
    // 9. Converte os 2 primeiros bytes para temperatura
    // Os bytes 0 e 1 contêm a temperatura (LSB e MSB)
    int16_t raw_temp = (scratchpad[1] << 8) | scratchpad[0];
    
    // 10. Converte para °C (resolução de 0.0625°C)
    float temp = (float)raw_temp / 16.0f;
    
    // 11. Verifica se a temperatura está em range válido (-55°C a +125°C)
    if (temp < -55.0f || temp > 125.0f) {
        ESP_LOGW(TAG, "DS18B20: temperatura fora do range válido: %.2f C", temp);
        return -127.0f;
    }
    
    ESP_LOGI(TAG, "DS18B20: temperatura lida: %.2f C", temp);
    return temp;
}

