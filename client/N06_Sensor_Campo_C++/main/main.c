#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h" // Necessário para o SPIFFS
#include "sensores.h"    // Sua biblioteca de sensores
#include "data_logger.h" // Sua nova biblioteca de log

static const char *TAG = "APP_MAIN";

void app_main(void)
{
    // Inicialização do NVS (Necessário para o SPIFFS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicializa o Logger (e o SPIFFS)
    // O primeiro argumento é o "ponto de montagem" (como "C:\" no Windows)
    // O segundo é o nome do arquivo
    if (!data_logger_init("/spiffs", "/log_temp.csv")) {
        ESP_LOGE(TAG, "Falha ao inicializar o logger! Parando.");
        return;
    }

    // Inicializa seus sensores
    ESP_LOGI(TAG, "Inicializando os sensores...");
    // sensores_init() (do Canvas) agora inicializa o ds18b20 E o soil_moisture
    sensores_init(); 
    ESP_LOGI(TAG, "Sensores inicializados.");

    // Lê e imprime o conteúdo atual do log na inicialização
    data_logger_read_all();

    while (1)
    {
        // 1. Lê os dados dos seus sensores
        // sensores_ler_dados() (do Canvas) agora retorna temp_solo e umid_solo
        sensor_data_t dados_sensores = sensores_ler_dados(); 

        // 2. Prepara a estrutura de log
        log_data_t log_entry;
        
        // --- Valores REAIS ---
        log_entry.temp_solo = dados_sensores.temp_solo; // Real (DS18B20)
        log_entry.umid_solo = dados_sensores.umid_solo; // Real (Soil Moisture)

        // --- Valores FIXOS ---
        log_entry.temp_ar = 25.0;   // Fixo
        log_entry.umid_ar = 50.0;   // Fixo

        // 3. Salva o log no arquivo CSV
        if (dados_sensores.temp_solo == -127.0) {
            ESP_LOGE(TAG, "Falha ao ler o sensor DS18B20. Log não salvo.");
        } else {
            if(data_logger_append(log_entry)) {
                // Log atualizado para mostrar os dois valores reais
                ESP_LOGI(TAG, "Log salvo! Temp Solo: %.2f C, Umid Solo: %.2f %%", 
                         log_entry.temp_solo, log_entry.umid_solo);
            } else {
                ESP_LOGE(TAG, "Falha ao salvar o log!");
            }
        }

        // Espera 1 minuto (60 segundos) antes de registrar novamente
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

