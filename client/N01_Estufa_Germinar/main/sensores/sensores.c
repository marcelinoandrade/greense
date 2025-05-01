#include "sensores.h"
#include "aht20.h"
#include "ens160.h"
#include "ds18b20.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"
#include <stdlib.h>
#include <time.h>

static const char *TAG = "SENSORES";

static aht20_t aht20_sensor;
static ens160_t ens160_sensor;
static bool sensors_initialized = false;


// Pinos dos sensores
#define GPIO_BOIA_MIN 32
#define GPIO_BOIA_MAX 33
#define GPIO_SENSOR_LUZ 25
#define GPIO_DS18B20 26
#define GPIO_SENSOR_UMIDADE 36
#define UMID_SOLO_ADC_SECO   3800
#define UMID_SOLO_ADC_UMIDO  1800

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

    // Configura GPIOs das boias
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_BOIA_MIN) | (1ULL << GPIO_BOIA_MAX),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Configura GPIO do sensor de luz
    gpio_config_t luz_conf = {
        .pin_bit_mask = (1ULL << GPIO_SENSOR_LUZ),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&luz_conf);

    // Configura ADC para sensor de umidade de solo (GPIO36)
    adc1_config_width(ADC_WIDTH_BIT_12); // 0 - 4095
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);

    // Inicializa o sensor DS18B20
    ds18b20_init(GPIO_DS18B20);
}

sensor_data_t sensores_ler_dados(void) {
    sensor_data_t dados;
    float temp_real, umid_real;

    if (sensors_initialized && aht20_read(&aht20_sensor, &temp_real, &umid_real)) {
        dados.temp = temp_real;
        dados.umid = umid_real;

        ens160_calibrate(&ens160_sensor, temp_real, umid_real);
        vTaskDelay(pdMS_TO_TICKS(100));
        dados.co2 =ens160_get_eco2(&ens160_sensor);
    } else {
        dados.temp = 0;
        dados.umid = 0;
        dados.co2 = 0;
    }

    // Leitura das boias
    dados.agua_min = 0; //gpio_get_level(GPIO_BOIA_MIN);
    dados.agua_max = 0; //gpio_get_level(GPIO_BOIA_MAX);

    // Leitura do sensor de luz digital
    int nivel_sensor_bruto = gpio_get_level(GPIO_SENSOR_LUZ);
    dados.luz = (nivel_sensor_bruto == 0) ? 1 : 0;  // Luz está ON se sensor detectar claridade (0)

    // Leitura do sensor DS18B20
    dados.temp_reserv_int = 0; //ds18b20_read_temperature(GPIO_DS18B20);

    // Leitura do sensor de umidade de solo
    int leitura_bruta = adc1_get_raw(ADC1_CHANNEL_0);
    dados.umid_solo_raw = leitura_bruta;
    //dados.umid_solo_pct = 100.0f * (4095 - leitura_bruta) / 4095.0f;
    float umid_pct = 100.0f * (UMID_SOLO_ADC_SECO - leitura_bruta) / (UMID_SOLO_ADC_SECO - UMID_SOLO_ADC_UMIDO);
    if (umid_pct > 100.0f) umid_pct = 100.0f;
    if (umid_pct < 0.0f)   umid_pct = 0.0f;
    dados.umid_solo_pct = umid_pct;

    // Simulações de sensores adicionais
    dados.ph = 0.0;
    dados.ec = 0.0;
    dados.temp_reserv_ext = 0; //randf(15.0, 25.0);

    return dados;
}
