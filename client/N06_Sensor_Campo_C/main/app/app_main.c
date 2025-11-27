#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

// BSP
#include "bsp/sensors/bsp_sensors.h"
#include "bsp/network/bsp_wifi_ap.h"
#include "bsp/board.h"

// APP
#include "app_sensor_manager.h"
#include "app_data_logger.h"
#include "app_atuadores.h"
#include "gui_services.h"

// GUI
#include "gui/web/gui_http_server.h"

static const char *TAG = "APP_MAIN";

/* Estrutura estática para expor serviços da camada APP para a GUI */
static gui_services_t gui_services_impl;

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

        vTaskDelay(pdMS_TO_TICKS(BSP_SENSOR_SAMPLE_INTERVAL_MS));
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
    
    // Limpa todos os dados armazenados (reinicia do zero)
    ESP_ERROR_CHECK(data_logger_clear_all());

    // ============================================================
    // CONECTA CAMADAS (remove dependência circular)
    // ============================================================
    
    // Conecta BSP → APP (callbacks Wi-Fi)
    wifi_ap_register_callbacks(
        atuadores_cliente_conectou,
        atuadores_cliente_desconectou
    );
    
    // Conecta APP → GUI (serviços) usando estrutura estática (evita ponteiro inválido)
    gui_services_impl.get_temp_air       = sensor_manager_get_temp_ar;
    gui_services_impl.get_humid_air      = sensor_manager_get_umid_ar;
    gui_services_impl.get_temp_soil      = sensor_manager_get_temp_solo;
    gui_services_impl.get_soil_raw       = sensor_manager_get_umid_solo_raw;
    gui_services_impl.get_soil_pct       = data_logger_raw_to_pct;
    gui_services_impl.get_calibration    = data_logger_get_calibracao;
    gui_services_impl.set_calibration    = data_logger_set_calibracao;
    gui_services_impl.build_history_json = data_logger_build_history_json;
    gui_services_impl.clear_logged_data  = data_logger_clear_all;
    gui_services_register(&gui_services_impl);

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
