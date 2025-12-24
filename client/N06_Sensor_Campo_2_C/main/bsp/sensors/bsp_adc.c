#include "bsp_adc.h"
#include "../board.h"

#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include <stdbool.h>

static const char *TAG = "BSP_ADC";

static adc_oneshot_unit_handle_t adc_handle = NULL;
static bool initialized = false;

esp_err_t adc_bsp_init(void)
{
    if (initialized) {
        return ESP_OK;
    }

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id  = BSP_ADC_SOIL_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit falhou: %s", esp_err_to_name(err));
        return err;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = BSP_ADC_SOIL_BITWIDTH,
        .atten    = BSP_ADC_SOIL_ATTEN,
    };

    err = adc_oneshot_config_channel(adc_handle,
                                     BSP_ADC_SOIL_CHANNEL,
                                     &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel falhou: %s", esp_err_to_name(err));
        return err;
    }

    initialized = true;
    ESP_LOGI(TAG,
             "ADC umidade solo inicializado. canal=%d bitwidth=%d atten=%d",
             (int)BSP_ADC_SOIL_CHANNEL,
             (int)BSP_ADC_SOIL_BITWIDTH,
             (int)BSP_ADC_SOIL_ATTEN);

    return ESP_OK;
}

esp_err_t adc_bsp_read_soil(int *raw_value)
{
    if (!initialized || adc_handle == NULL) {
        ESP_LOGW(TAG, "ADC não inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    if (raw_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = adc_oneshot_read(adc_handle,
                                     BSP_ADC_SOIL_CHANNEL,
                                     raw_value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha leitura ADC: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

