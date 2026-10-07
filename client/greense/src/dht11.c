#include "dht11.h"

#include "esp_attr.h"
#include "esp_cpu.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DHT11";

/* Meio do caminho entre o bit 0 (~28 µs) e o bit 1 (~70 µs). */
#define DHT11_BIT_THRESHOLD_US 50
#define DHT11_HIGH_MIN_US      15
#define DHT11_HIGH_MAX_US      100
#define DHT11_MIN_INTERVAL_US  1000000

/*
 * Dois formatos usam os mesmos 5 bytes e o mesmo checksum.
 * DHT11: inteiro + décimo em cada par. Os décimos ficam entre 0 e 9.
 * 16 bits (DHT22 / AM2302): valor = (byte alto << 8 | byte baixo) / 10.
 * Se o "décimo" passa de 9, o quadro é de 16 bits.
 */
#define DHT11_TEMP_MIN 0.0f
#define DHT11_TEMP_MAX 50.0f
#define DHT22_TEMP_MIN -40.0f
#define DHT22_TEMP_MAX 80.0f
#define DHT_HUM_MIN 0.0f
#define DHT_HUM_MAX 100.0f

static uint32_t s_ticks_per_us = 1;
static int64_t s_last_read_us;

static int IRAM_ATTR wait_level(gpio_num_t pin, int level, uint32_t timeout_us)
{
    uint32_t start = esp_cpu_get_cycle_count();
    uint32_t limit = timeout_us * s_ticks_per_us;

    while (gpio_get_level(pin) != level) {
        if ((esp_cpu_get_cycle_count() - start) > limit) {
            return -1;
        }
    }
    return (int)((esp_cpu_get_cycle_count() - start) / s_ticks_per_us);
}

static void log_frame(const int high_us[40], const uint8_t data[5], int nbits)
{
#if DHT11_DEBUG
    for (int i = 0; i < nbits; i++) {
        int bit = (high_us[i] >= DHT11_BIT_THRESHOLD_US) ? 1 : 0;
        ESP_LOGI(TAG, "bit %02d: %d us -> %d", i, high_us[i], bit);
    }
    if (nbits == 40) {
        uint8_t calc = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
        ESP_LOGI(TAG, "RAW: %02X %02X %02X %02X %02X",
                 data[0], data[1], data[2], data[3], data[4]);
        ESP_LOGI(TAG, "checksum calculado: %02X", calc);
        ESP_LOGI(TAG, "checksum recebido: %02X", data[4]);
    }
#else
    (void)high_us;
    (void)data;
    (void)nbits;
#endif
}

void dht11_init(gpio_num_t gpio)
{
    uint32_t ticks = esp_rom_get_cpu_ticks_per_us();
    s_ticks_per_us = ticks ? ticks : 1;

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    /* Nível 1 em open-drain solta a linha. O 4,7 kΩ externo leva DATA a 3,3 V. */
    gpio_set_level(gpio, 1);
}

bool dht11_read(gpio_num_t gpio, float *temperature, float *humidity)
{
    uint8_t data[5] = {0};
    int high_us[40] = {0};
    int nbits = 0;
    bool presence = false;
    bool pulses_ok = true;

    uint32_t ticks = esp_rom_get_cpu_ticks_per_us();
    s_ticks_per_us = ticks ? ticks : 1;

    int64_t now = esp_timer_get_time();
    if (s_last_read_us != 0 && (now - s_last_read_us) < DHT11_MIN_INTERVAL_US) {
        ESP_LOGW(TAG, "leitura cedo demais");
        return false;
    }
    s_last_read_us = now;

    gpio_set_level(gpio, 0);
    esp_rom_delay_us(20000);

    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    taskENTER_CRITICAL(&mux);

    gpio_set_level(gpio, 1);
    esp_rom_delay_us(30);

    presence = wait_level(gpio, 0, 200) >= 0
        && wait_level(gpio, 1, 120) >= 0
        && wait_level(gpio, 0, 120) >= 0;

    for (int i = 0; presence && i < 40; i++) {
        if (wait_level(gpio, 1, 80) < 0) {
            pulses_ok = false;
            break;
        }
        int pulse = wait_level(gpio, 0, 120);
        high_us[i] = pulse;
        nbits++;
        if (pulse < DHT11_HIGH_MIN_US || pulse > DHT11_HIGH_MAX_US) {
            pulses_ok = false;
            break;
        }
        data[i / 8] = (uint8_t)((data[i / 8] << 1) | (pulse >= DHT11_BIT_THRESHOLD_US));
    }

    gpio_set_level(gpio, 1);
    taskEXIT_CRITICAL(&mux);

    log_frame(high_us, data, nbits);

    if (!presence || nbits == 0) {
        ESP_LOGW(TAG, "sensor desconectado ou sem resposta no GPIO %d", gpio);
        return false;
    }
    if (!pulses_ok || nbits != 40) {
        ESP_LOGW(TAG, "pulso fora da faixa ou timeout (%d bits)", nbits);
        return false;
    }

    uint8_t calc = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (calc != data[4]) {
        ESP_LOGW(TAG, "checksum invalido: calculado %02X, recebido %02X", calc, data[4]);
        return false;
    }

    bool formato_16bits = (data[1] > 9) || (data[3] > 9);
    float hum;
    float temp;
    if (formato_16bits) {
        hum = ((uint16_t)((data[0] << 8) | data[1])) / 10.0f;
        uint16_t raw_temp = (uint16_t)((data[2] << 8) | data[3]);
        if (raw_temp & 0x8000) {
            temp = -((raw_temp & 0x7FFF) / 10.0f);
        } else {
            temp = raw_temp / 10.0f;
        }
    } else {
        hum = data[0] + (data[1] * 0.1f);
        temp = data[2] + (data[3] * 0.1f);
    }

    float temp_min = formato_16bits ? DHT22_TEMP_MIN : DHT11_TEMP_MIN;
    float temp_max = formato_16bits ? DHT22_TEMP_MAX : DHT11_TEMP_MAX;
    if (hum < DHT_HUM_MIN || hum > DHT_HUM_MAX || temp < temp_min || temp > temp_max) {
        ESP_LOGW(TAG, "leitura impossivel: %.1f C / %.1f %%", temp, hum);
        return false;
    }
    ESP_LOGI(TAG, "formato %s -> %.1f C / %.1f %%",
             formato_16bits ? "16 bits" : "DHT11", temp, hum);

    if (humidity) {
        *humidity = hum;
    }
    if (temperature) {
        *temperature = temp;
    }
    return true;
}
