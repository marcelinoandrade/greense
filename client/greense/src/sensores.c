#include "sensores.h"
#include "dht11.h"
#include "ds18b20.h"
#include "greense.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "SENSORES";

static adc_oneshot_unit_handle_t adc_solo;
static int s_boias;
static int s_adc_seco;
static int s_adc_umido;

void sensores_init(const greense_app_config_t *cfg)
{
    s_boias = cfg->boias;
    s_adc_seco = cfg->umid_solo_adc_seco;
    s_adc_umido = cfg->umid_solo_adc_umido;

    dht11_init(GREENSE_PIN_AR);
    ds18b20_init(GREENSE_PIN_SOLO_TEMP);

    gpio_config_t luz_conf = {
        .pin_bit_mask = (1ULL << GREENSE_PIN_LUZ),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&luz_conf);

    if (s_boias) {
        gpio_config_t boia_conf = {
            .pin_bit_mask = (1ULL << GREENSE_PIN_BOIA_MIN) | (1ULL << GREENSE_PIN_BOIA_MAX),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&boia_conf);
    }

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = GREENSE_ADC_SOLO_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_solo));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_solo, GREENSE_ADC_SOLO_CHANNEL, &chan_cfg));

    ESP_LOGI(TAG, "%s | ar GPIO%d | solo temp GPIO%d | luz GPIO%d | solo umid GPIO%d ADC%d | boias %s",
             GREENSE_PLACA_NOME,
             GREENSE_PIN_AR, GREENSE_PIN_SOLO_TEMP, GREENSE_PIN_LUZ, GREENSE_PIN_SOLO_UMID,
             GREENSE_ADC_SOLO_CHANNEL, s_boias ? "sim" : "nao");
    if (s_boias) {
        ESP_LOGI(TAG, "boia min GPIO%d | boia max GPIO%d", GREENSE_PIN_BOIA_MIN, GREENSE_PIN_BOIA_MAX);
    }
}

sensor_data_t sensores_ler_dados(void)
{
    sensor_data_t dados = {0};
    float temp = 0;
    float umid = 0;

    if (dht11_read(GREENSE_PIN_AR, &temp, &umid)) {
        dados.temp = temp;
        dados.umid = umid;
    }

    dados.luz = (gpio_get_level(GREENSE_PIN_LUZ) == 0) ? 1 : 0;
    if (s_boias) {
        dados.agua_min = gpio_get_level(GREENSE_PIN_BOIA_MIN);
        dados.agua_max = gpio_get_level(GREENSE_PIN_BOIA_MAX);
    }
    dados.temp_reserv_int = ds18b20_read_temperature(GREENSE_PIN_SOLO_TEMP);

    int leitura_bruta = 0;
    if (adc_oneshot_read(adc_solo, GREENSE_ADC_SOLO_CHANNEL, &leitura_bruta) == ESP_OK) {
        dados.umid_solo_raw = leitura_bruta;
        float faixa = (float)(s_adc_seco - s_adc_umido);
        float umid_pct = 0;
        if (faixa != 0.0f) {
            umid_pct = 100.0f * (s_adc_seco - leitura_bruta) / faixa;
        }
        if (umid_pct > 100.0f) {
            umid_pct = 100.0f;
        }
        if (umid_pct < 0.0f) {
            umid_pct = 0.0f;
        }
        dados.umid_solo_pct = umid_pct;
    }

    ESP_LOGI(TAG, "ar %.1f C / %.1f %% | solo %.1f C | umid %d (%.0f%%) | luz %.0f | boia min %d max %d",
             dados.temp, dados.umid, dados.temp_reserv_int,
             dados.umid_solo_raw, dados.umid_solo_pct, dados.luz,
             dados.agua_min, dados.agua_max);
    return dados;
}
