#include "app_cultivation_tolerance.h"

#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "APP_CULTIVATION";

static cultivation_tolerance_t current_tolerance;

static void set_defaults(void)
{
    current_tolerance.temp_ar_min = 20.0f;
    current_tolerance.temp_ar_max = 30.0f;
    current_tolerance.umid_ar_min = 50.0f;
    current_tolerance.umid_ar_max = 80.0f;
    current_tolerance.temp_solo_min = 18.0f;
    current_tolerance.temp_solo_max = 25.0f;
    current_tolerance.umid_solo_min = 40.0f;
    current_tolerance.umid_solo_max = 80.0f;
    current_tolerance.luminosidade_min = 500.0f;
    current_tolerance.luminosidade_max = 2000.0f;
    current_tolerance.dpv_min = 0.5f;
    current_tolerance.dpv_max = 2.0f;
}

void cultivation_tolerance_get_defaults(cultivation_tolerance_t *out)
{
    if (out == NULL) return;
    set_defaults();
    *out = current_tolerance;
}

esp_err_t cultivation_tolerance_init(void)
{
    set_defaults();

    nvs_handle_t handle;
    esp_err_t err = nvs_open("appcfg", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open falhou (%s), usando padrão", esp_err_to_name(err));
        return err;
    }

    size_t required_size = sizeof(cultivation_tolerance_t);
    err = nvs_get_blob(handle, "cult_tolerance", &current_tolerance, &required_size);
    if (err == ESP_OK && required_size == sizeof(cultivation_tolerance_t)) {
        ESP_LOGI(TAG, "Tolerâncias de cultivo carregadas do NVS");
    } else {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Valor inválido ou erro lendo cult_tolerance (%s)", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Nenhuma tolerância salva. Usando padrão");
        }
        set_defaults();
    }
    nvs_close(handle);
    return ESP_OK;
}

void cultivation_tolerance_get(cultivation_tolerance_t *out)
{
    if (out == NULL) return;
    *out = current_tolerance;
}

esp_err_t cultivation_tolerance_set(const cultivation_tolerance_t *tolerance)
{
    if (tolerance == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Validação básica: min < max para cada par
    if (tolerance->temp_ar_min >= tolerance->temp_ar_max ||
        tolerance->umid_ar_min >= tolerance->umid_ar_max ||
        tolerance->temp_solo_min >= tolerance->temp_solo_max ||
        tolerance->umid_solo_min >= tolerance->umid_solo_max ||
        tolerance->luminosidade_min >= tolerance->luminosidade_max ||
        tolerance->dpv_min >= tolerance->dpv_max) {
        ESP_LOGE(TAG, "Valores de tolerância inválidos: min >= max");
        return ESP_ERR_INVALID_ARG;
    }

    current_tolerance = *tolerance;

    nvs_handle_t handle;
    esp_err_t err = nvs_open("appcfg", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro abrindo NVS para salvar tolerâncias (%s)", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, "cult_tolerance", &current_tolerance, sizeof(cultivation_tolerance_t));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro salvando tolerâncias (%s)", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Tolerâncias de cultivo atualizadas");
    return ESP_OK;
}

esp_err_t cultivation_tolerance_reset_to_defaults(void)
{
    cultivation_tolerance_t defaults;
    set_defaults();
    defaults = current_tolerance;
    return cultivation_tolerance_set(&defaults);
}

