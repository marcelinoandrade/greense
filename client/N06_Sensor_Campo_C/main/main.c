#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "sensores/sensores.h"
#include "libs/data_logger.h"
#include "libs/http_server.h"
#include "atuadores/atuadores.h"

static const char *TAG = "APP_MAIN";

// Tarefa periódica que lê sensores e registra no SPIFFS
static void tarefa_log(void *pvParameter)
{
    while (1)
    {
        log_entry_t entry;
        entry.temp_ar    = sensores_get_temp_ar();
        entry.umid_ar    = sensores_get_umid_ar();
        entry.temp_solo  = sensores_get_temp_solo();
        // leitura bruta do solo (ADC). converte para % usando calibração atual
        int leitura_raw = sensores_get_umid_solo_raw();
        entry.umid_solo = data_logger_raw_to_pct(leitura_raw);

        if (!isnan(entry.temp_solo) && !isnan(entry.umid_solo))
        {
            if (data_logger_append(&entry))
            {
                ESP_LOGI(TAG,
                         "Log salvo! Temp Solo: %.2f C, Umid Solo: %.2f %%",
                         entry.temp_solo,
                         entry.umid_solo);

                // sinaliza flash de gravação
                atuadores_sinalizar_gravacao();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(60000)); // 60 s
    }
}

void app_main(void)
{
    // Inicializa NVS (necessário para Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Inicializa atuadores (LED de status)
    atuadores_init();
    // ainda não sabemos se AP está ativo
    atuadores_set_ap_status(false);

    // Inicializa sensores
    sensores_init();

    // Inicializa logger (SPIFFS e calibração)
    ESP_ERROR_CHECK(data_logger_init());

    // Sobe servidor HTTP + SoftAP
    // http_server_start() deve:
    //   - montar AP
    //   - registrar handlers
    //   - logar "Modo AP iniciado..."
    // Após sucesso consideramos AP ativo
    ESP_ERROR_CHECK(http_server_start());
    atuadores_set_ap_status(true);

    // Cria tarefa que controla LED de status (pisca, apaga etc)
    xTaskCreate(
        atuadores_task,
        "atuadores_task",
        2048,
        NULL,
        4,
        NULL
    );

    // Cria tarefa de registro periódico
    xTaskCreate(
        tarefa_log,
        "tarefa_log",
        4096,
        NULL,
        5,
        NULL
    );

    ESP_LOGI(TAG, "Sistema iniciado");
}
