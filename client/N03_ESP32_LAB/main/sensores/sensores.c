#include "sensores.h"
#include "aht20.h"
#include "ens160.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdlib.h>
#include <time.h>

static const char *TAG = "SENSORES";

static aht20_t aht20_sensor;
static ens160_t ens160_sensor;
static bool sensors_initialized = false;

// Pinos das boias
#define GPIO_BOIA_MIN 32
#define GPIO_BOIA_MAX 33

float randf(float min, float max) {
    return min + ((float)rand() / RAND_MAX) * (max - min);
}

void sensores_init(void) {
    srand((unsigned int) time(NULL));
    
    // Configura I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);

    // Inicializa sensores I2C
    bool aht20_ok = aht20_init(&aht20_sensor, I2C_NUM_0, AHT20_I2C_ADDR_DEFAULT);
    bool ens160_ok = ens160_init(&ens160_sensor, I2C_NUM_0);
    sensors_initialized = aht20_ok && ens160_ok;
    ESP_LOGI(TAG, "Sensors initialized: %s", sensors_initialized ? "YES" : "NO");

    // Configura GPIOs das boias como entrada com pull-up
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_BOIA_MIN) | (1ULL << GPIO_BOIA_MAX),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

sensor_data_t sensores_ler_dados(void) {
    sensor_data_t dados;
    float temp_real, umid_real;

    // Tenta ler dados reais
    if (sensors_initialized && aht20_read(&aht20_sensor, &temp_real, &umid_real)) {
        dados.temp = temp_real;
        dados.umid = umid_real;

        ens160_calibrate(&ens160_sensor, temp_real, umid_real);
        vTaskDelay(pdMS_TO_TICKS(100));
        dados.co2 = ens160_get_eco2(&ens160_sensor);
    } else {
        dados.temp = 0;
        dados.umid = 0;
        dados.co2 = 0;
    }

    // Lê os valores reais das boias
    dados.agua_min = gpio_get_level(GPIO_BOIA_MIN);  // 0 = acionada (baixa), 1 = desligada
    dados.agua_max = gpio_get_level(GPIO_BOIA_MAX);

    // Outros sensores (valores simulados por enquanto)
    dados.luz = 1;
    dados.temp_reserv_int = 25;
    dados.ph = 5.5;
    dados.ec = 1.8;
    dados.temp_reserv_ext = randf(15.0, 25.0);

    return dados;
}
