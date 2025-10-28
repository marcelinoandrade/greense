#include "soil_moisture.h"

#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "SOIL_MOISTURE";

/* Canal físico: GPIO34 = ADC1_CH6 */
#define SOIL_ADC_UNIT        ADC_UNIT_1
#define SOIL_ADC_CHANNEL     ADC_CHANNEL_6    /* ADC1_CH6 -> GPIO34 */
#define SOIL_ADC_BITWIDTH    ADC_BITWIDTH_12  /* 0..4095 */
#define SOIL_ADC_ATTEN       ADC_ATTEN_DB_11  /* ~0-3.3V */

static adc_oneshot_unit_handle_t adc_handle = NULL;
static bool adc_ready = false;

esp_err_t soil_moisture_init(void)
{
    if (adc_ready) {
        return ESP_OK;
    }

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id  = SOIL_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit falhou: %s", esp_err_to_name(err));
        return err;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = SOIL_ADC_BITWIDTH,
        .atten    = SOIL_ADC_ATTEN,
    };

    err = adc_oneshot_config_channel(adc_handle,
                                     SOIL_ADC_CHANNEL,
                                     &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel falhou: %s", esp_err_to_name(err));
        return err;
    }

    adc_ready = true;
    ESP_LOGI(TAG,
             "ADC umidade solo inicializado. canal=%d bitwidth=%d atten=%d",
             (int)SOIL_ADC_CHANNEL,
             (int)SOIL_ADC_BITWIDTH,
             (int)SOIL_ADC_ATTEN);

    return ESP_OK;
}

int soil_moisture_leitura_bruta(void)
{
    if (!adc_ready || adc_handle == NULL) {
        ESP_LOGW(TAG, "ADC nao inicializado");
        return -1;
    }

    int raw = 0;
    esp_err_t err = adc_oneshot_read(adc_handle,
                                     SOIL_ADC_CHANNEL,
                                     &raw);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha leitura ADC: %s", esp_err_to_name(err));
        return -1;
    }

    return raw; /* seco ~4000 molhado ~400 */
}
