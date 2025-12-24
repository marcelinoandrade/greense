#include "app_stats_window.h"

#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "APP_STATS_WINDOW";

/* Opções válidas de número de amostras */
static const int k_stats_window_options[] = { 5, 10, 15, 20 };
static const size_t k_stats_window_option_count = 
    sizeof(k_stats_window_options) / sizeof(k_stats_window_options[0]);

static const int k_default_stats_window = 10;
static int current_stats_window = k_default_stats_window;

static bool is_valid_internal(int count)
{
    for (size_t i = 0; i < k_stats_window_option_count; ++i) {
        if (k_stats_window_options[i] == count) {
            return true;
        }
    }
    return false;
}

bool stats_window_is_valid(int count)
{
    return is_valid_internal(count);
}

esp_err_t stats_window_init(void)
{
    current_stats_window = k_default_stats_window;

    nvs_handle_t handle;
    esp_err_t err = nvs_open("appcfg", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open falhou (%s), usando padrão", esp_err_to_name(err));
        return err;
    }

    int32_t stored = 0;
    err = nvs_get_i32(handle, "stats_window", &stored);
    if (err == ESP_OK && is_valid_internal((int)stored)) {
        current_stats_window = (int)stored;
        ESP_LOGI(TAG, "Período estatístico carregado do NVS: %d amostras", current_stats_window);
    } else {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Valor inválido ou erro lendo stats_window (%s)", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Nenhum período estatístico salvo. Usando padrão %d amostras", current_stats_window);
        }
    }
    nvs_close(handle);
    return ESP_OK;
}

int stats_window_get_count(void)
{
    return current_stats_window;
}

esp_err_t stats_window_set_count(int count)
{
    if (!is_valid_internal(count)) {
        return ESP_ERR_INVALID_ARG;
    }

    current_stats_window = count;

    nvs_handle_t handle;
    esp_err_t err = nvs_open("appcfg", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro abrindo NVS para salvar período estatístico (%s)", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_i32(handle, "stats_window", (int32_t)current_stats_window);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro salvando período estatístico (%s)", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Período estatístico atualizado para %d amostras", current_stats_window);
    return ESP_OK;
}

