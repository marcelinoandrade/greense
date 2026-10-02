#include "sensores.h"
#include "config.h"
#include "dht11.h"
#include "ds18b20.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "SENSORES";

#define UMID_SOLO_ADC_SECO   3800
#define UMID_SOLO_ADC_UMIDO  1800

static adc_oneshot_unit_handle_t adc_solo;

void sensores_init(void)
{
    dht11_init(GPIO_DHT11);
    ds18b20_init(GPIO_DS18B20);

    gpio_config_t luz_conf = {
        .pin_bit_mask = (1ULL << GPIO_SENSOR_LUZ),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&luz_conf);

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_solo));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_solo, ADC_CHANNEL_0, &chan_cfg));

    ESP_LOGI(TAG, "DHT11 GPIO%d | DS18B20 GPIO%d | LDR GPIO%d | solo GPIO%d",
             GPIO_DHT11, GPIO_DS18B20, GPIO_SENSOR_LUZ, GPIO_UMID_SOLO);
}

sensor_data_t sensores_ler_dados(void)
{
    sensor_data_t dados = {0};
    float temp = 0;
    float umid = 0;

    if (dht11_read(GPIO_DHT11, &temp, &umid)) {
        dados.temp = temp;
        dados.umid = umid;
    }

    dados.luz = (gpio_get_level(GPIO_SENSOR_LUZ) == 0) ? 1 : 0;
    dados.temp_reserv_int = ds18b20_read_temperature(GPIO_DS18B20);

    int leitura_bruta = 0;
    if (adc_oneshot_read(adc_solo, ADC_CHANNEL_0, &leitura_bruta) == ESP_OK) {
        dados.umid_solo_raw = leitura_bruta;
        float umid_pct = 100.0f * (UMID_SOLO_ADC_SECO - leitura_bruta)
            / (float)(UMID_SOLO_ADC_SECO - UMID_SOLO_ADC_UMIDO);
        if (umid_pct > 100.0f) {
            umid_pct = 100.0f;
        }
        if (umid_pct < 0.0f) {
            umid_pct = 0.0f;
        }
        dados.umid_solo_pct = umid_pct;
    }

    ESP_LOGI(TAG, "ar %.1f C / %.1f %% | solo %.1f C | umid %d (%.0f%%) | luz %.0f",
             dados.temp, dados.umid, dados.temp_reserv_int,
             dados.umid_solo_raw, dados.umid_solo_pct, dados.luz);
    return dados;
}
