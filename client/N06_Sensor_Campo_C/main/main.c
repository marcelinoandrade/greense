#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

// BSP
#include "bsp/sensors/sensors_bsp.h"

// APP
#include "app/sensor_manager.h"
#include "app/data_logger.h"
#include "app/atuadores.h"

// GUI
#include "gui/web/http_server.h"

static const char *TAG = "APP_MAIN";

// Tarefa periódica que lê sensores e registra no SPIFFS
static void tarefa_log(void *pvParameter)
{
    while (1)
    {
        sensor_reading_t reading;
        if (sensor_manager_read(&reading) == ESP_OK)
        {
            if (sensor_manager_is_valid(&reading))
            {
                log_entry_t entry;
                entry.temp_ar    = reading.temp_air;
                entry.umid_ar    = reading.humid_air;
                entry.temp_solo  = reading.temp_soil;
                entry.umid_solo = reading.humid_soil;

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

    // Inicializa sensores via APP (que usa BSP internamente)
    ESP_ERROR_CHECK(sensor_manager_init());

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

    ESP_LOGI(TAG, "Sistema iniciado - Arquitetura BSP/APP/GUI");
}
