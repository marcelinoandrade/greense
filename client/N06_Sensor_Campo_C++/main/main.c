#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <math.h>              
#include "sensores.h"
#include "data_logger.h"
#include "http_server.h"

static const char *TAG = "APP_MAIN";

static void tarefa_log(void *arg)
{
    while (1)
    {
        log_entry_t entry;
        entry.temp_ar   = sensores_get_temp_ar();
        entry.umid_ar   = sensores_get_umid_ar();
        entry.temp_solo = sensores_get_temp_solo();

        // leitura bruta do solo (ADC). converte para % usando calibração atual
        int leitura_raw = sensores_get_umid_solo_raw();
        entry.umid_solo = data_logger_raw_to_pct(leitura_raw);

        // só grava se leitura de solo for válida
        if (!isnan(entry.temp_solo) && !isnan(entry.umid_solo))
        {
            if (data_logger_append(&entry))
            {
                ESP_LOGI(TAG,
                         "Log salvo! Temp Solo: %.2f C, Umid Solo: %.2f %%",
                         entry.temp_solo,
                         entry.umid_solo);
            }
            else
            {
                ESP_LOGE(TAG, "Falha ao salvar o log");
            }
        }
        else
        {
            ESP_LOGW(TAG, "Leitura inválida. Log ignorado");
        }

        /* período de 60 s entre amostras */
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void app_main(void)
{
    /* NVS é requisito para Wi-Fi e também para SPIFFS em IDF 5.x */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_LOGI(TAG, "Inicializando SPIFFS e logger");
    ESP_ERROR_CHECK(data_logger_init());

    ESP_LOGI(TAG, "Dump inicial do log para conferência");
    data_logger_dump_to_logcat(); /* imprime N,temp_ar,... igual você viu no boot */

    ESP_LOGI(TAG, "Inicializando sensores");
    sensores_init(); /* deve configurar ADC, DS18B20 etc */

    ESP_LOGI(TAG, "Inicializando servidor HTTP em modo AP");
    ESP_ERROR_CHECK(http_server_start()); /* sobe AP + servidor */

    /* cria tarefa de log periódico em background */
    xTaskCreatePinnedToCore(
        tarefa_log,
        "tarefa_log",
        4096,
        NULL,
        5,
        NULL,
        1);

    ESP_LOGI(TAG, "Sistema iniciado");
}
