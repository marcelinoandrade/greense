#include "app_sampling_period.h"

#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "../bsp/board.h"

static const char *TAG = "APP_SAMPLING";

typedef struct {
    uint32_t ms;
} sampling_option_t;

static const sampling_option_t k_sampling_options[] = {
    { 10 * 1000 },          // 10 segundos
    { 60 * 1000 },          // 1 minuto
    { 10 * 60 * 1000 },     // 10 minutos
    { 60 * 60 * 1000 },     // 1 hora
    { 6 * 60 * 60 * 1000 }, // 6 horas
    { 12 * 60 * 60 * 1000 } // 12 horas
};

static uint32_t current_period_ms = BSP_SENSOR_SAMPLE_INTERVAL_MS;

static bool is_valid_internal(uint32_t ms)
{
    for (size_t i = 0; i < sizeof(k_sampling_options)/sizeof(k_sampling_options[0]); ++i) {
        if (k_sampling_options[i].ms == ms) {
            return true;
        }
    }
    return false;
}

bool sampling_period_is_valid(uint32_t period_ms)
{
    return is_valid_internal(period_ms);
}

esp_err_t sampling_period_init(void)
{
    current_period_ms = BSP_SENSOR_SAMPLE_INTERVAL_MS;

    nvs_handle_t handle;
    esp_err_t err = nvs_open("appcfg", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open falhou (%s), usando padrão", esp_err_to_name(err));
        return err;
    }

    uint32_t stored = 0;
    err = nvs_get_u32(handle, "sample_ms", &stored);
    if (err == ESP_OK && is_valid_internal(stored)) {
        current_period_ms = stored;
        ESP_LOGI(TAG, "Período carregado do NVS: %lu ms", (unsigned long)current_period_ms);
    } else {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Valor inválido ou erro lendo sample_ms (%s)", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Nenhum período salvo. Usando padrão %lu ms", (unsigned long)current_period_ms);
        }
    }
    nvs_close(handle);
    return ESP_OK;
}

uint32_t sampling_period_get_ms(void)
{
    return current_period_ms;
}

esp_err_t sampling_period_set_ms(uint32_t period_ms)
{
    if (!is_valid_internal(period_ms)) {
        return ESP_ERR_INVALID_ARG;
    }

    current_period_ms = period_ms;

    nvs_handle_t handle;
    esp_err_t err = nvs_open("appcfg", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro abrindo NVS para salvar período (%s)", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u32(handle, "sample_ms", current_period_ms);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro salvando período (%s)", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Período atualizado para %lu ms", (unsigned long)current_period_ms);
    return ESP_OK;
}


