#include "sensores.h"
#include "aht20.h"
#include "ens160.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <stdlib.h>
#include <time.h>

static const char *TAG = "SENSORES";

static aht20_t aht20_sensor;
static ens160_t ens160_sensor;
static bool sensors_initialized = false;

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

    // Inicializa sensores
    bool aht20_ok = aht20_init(&aht20_sensor, I2C_NUM_0, AHT20_I2C_ADDR_DEFAULT);
    bool ens160_ok = ens160_init(&ens160_sensor, I2C_NUM_0);
    
    sensors_initialized = aht20_ok && ens160_ok;
    ESP_LOGI(TAG, "Sensors initialized: %s", sensors_initialized ? "YES" : "NO");
}

sensor_data_t sensores_ler_dados(void) {
    sensor_data_t dados;
    float temp_real, umid_real;

    // Tenta ler dados reais
    if (sensors_initialized && aht20_read(&aht20_sensor, &temp_real, &umid_real)) {
        dados.temp = temp_real;
        dados.umid = umid_real;
        
        // Lê CO2 do ENS160
        ens160_calibrate(&ens160_sensor, temp_real, umid_real);
        vTaskDelay(pdMS_TO_TICKS(100));
        dados.co2 = ens160_get_eco2(&ens160_sensor);
    } else {
        // Fallback para valores aleatórios
        dados.temp = randf(20.0, 35.0);
        dados.umid = randf(30.0, 90.0);
        dados.co2 = randf(400.0, 2000.0);
    }

    // Outros valores (aleatórios)
    dados.luz = randf(100.0, 2000.0);
    dados.agua_min = rand() % 2;
    dados.agua_max = rand() % 2;
    dados.temp_reserv_int = randf(15.0, 25.0);
    dados.ph = randf(5.5, 7.5);
    dados.ec = randf(0.5, 2.5);
    dados.temp_reserv_ext = randf(15.0, 25.0);

    return dados;
}