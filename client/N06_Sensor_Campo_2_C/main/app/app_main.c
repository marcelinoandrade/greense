#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_timer.h"

// BSP
#include "bsp/sensors/bsp_sensors.h"
#include "bsp/network/bsp_wifi_ap.h"
#include "bsp/board.h"

// APP
#include "app_sensor_manager.h"
#include "app_data_logger.h"
#include "app_atuadores.h"
#include "app_sampling_period.h"
#include "app_stats_window.h"
#include "app_cultivation_tolerance.h"
#include "gui_services.h"

// GUI
#include "gui/web/gui_http_server.h"

static const char *TAG = "APP_MAIN";

/* Estrutura estática para expor serviços da camada APP para a GUI */
static gui_services_t gui_services_impl;

/* Wrapper para build_history_json que usa a configuração de stats_window */
static char* build_history_json_wrapper(void)
{
    int max_samples = stats_window_get_count();
    ESP_LOGD(TAG, "build_history_json_wrapper: usando %d amostras", max_samples);
    return data_logger_build_history_json(max_samples);
}

/* Wrappers para tolerâncias de cultivo */
static void get_cultivation_tolerance_wrapper(float *temp_ar_min, float *temp_ar_max,
                                              float *umid_ar_min, float *umid_ar_max,
                                              float *temp_solo_min, float *temp_solo_max,
                                              float *umid_solo_min, float *umid_solo_max,
                                              float *luminosidade_min, float *luminosidade_max,
                                              float *dpv_min, float *dpv_max)
{
    cultivation_tolerance_t tol;
    cultivation_tolerance_get(&tol);
    if (temp_ar_min) *temp_ar_min = tol.temp_ar_min;
    if (temp_ar_max) *temp_ar_max = tol.temp_ar_max;
    if (umid_ar_min) *umid_ar_min = tol.umid_ar_min;
    if (umid_ar_max) *umid_ar_max = tol.umid_ar_max;
    if (temp_solo_min) *temp_solo_min = tol.temp_solo_min;
    if (temp_solo_max) *temp_solo_max = tol.temp_solo_max;
    if (umid_solo_min) *umid_solo_min = tol.umid_solo_min;
    if (umid_solo_max) *umid_solo_max = tol.umid_solo_max;
    if (luminosidade_min) *luminosidade_min = tol.luminosidade_min;
    if (luminosidade_max) *luminosidade_max = tol.luminosidade_max;
    if (dpv_min) *dpv_min = tol.dpv_min;
    if (dpv_max) *dpv_max = tol.dpv_max;
}

static esp_err_t set_cultivation_tolerance_wrapper(float temp_ar_min, float temp_ar_max,
                                                    float umid_ar_min, float umid_ar_max,
                                                    float temp_solo_min, float temp_solo_max,
                                                    float umid_solo_min, float umid_solo_max,
                                                    float luminosidade_min, float luminosidade_max,
                                                    float dpv_min, float dpv_max)
{
    cultivation_tolerance_t tol;
    tol.temp_ar_min = temp_ar_min;
    tol.temp_ar_max = temp_ar_max;
    tol.umid_ar_min = umid_ar_min;
    tol.umid_ar_max = umid_ar_max;
    tol.temp_solo_min = temp_solo_min;
    tol.temp_solo_max = temp_solo_max;
    tol.umid_solo_min = umid_solo_min;
    tol.umid_solo_max = umid_solo_max;
    tol.luminosidade_min = luminosidade_min;
    tol.luminosidade_max = luminosidade_max;
    tol.dpv_min = dpv_min;
    tol.dpv_max = dpv_max;
    return cultivation_tolerance_set(&tol);
}

static const uint32_t SENSOR_JANELA_MS = 5000;
static const uint32_t SENSOR_RETRY_MS  = 2000;

static bool capturar_primeiro_valido(sensor_reading_t *dest)
{
    uint32_t period_ms = sampling_period_get_ms();
    int64_t deadline_us = esp_timer_get_time() + (int64_t)SENSOR_JANELA_MS * 1000;

    while (esp_timer_get_time() < deadline_us) {
        sensor_reading_t leitura = {0};
        if (sensor_manager_read(&leitura) == ESP_OK &&
            sensor_manager_is_valid_with_outlier_detection(&leitura, period_ms)) {
            *dest = leitura;
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(SENSOR_RETRY_MS));
    }
    return false;
}

// Tarefa periódica que lê sensores e registra no SPIFFS
static void tarefa_log(void *pvParameter)
{
    static log_entry_t ultimo_entry = {0};
    static bool ultimo_entry_valido = false;

    while (1)
    {
        uint32_t intervalo_ms = sampling_period_get_ms();
        int64_t janela_inicio = esp_timer_get_time();

        sensor_reading_t reading;
        bool leitura_valida = capturar_primeiro_valido(&reading);

        log_entry_t entry;
        bool deve_registrar = false;

        if (leitura_valida)
        {
            entry.temp_ar   = reading.temp_air;
            entry.umid_ar   = reading.humid_air;
            entry.temp_solo = reading.temp_soil;
            entry.umid_solo = reading.humid_soil;
            entry.luminosidade = reading.luminosity;
            entry.dpv = reading.dpv;

            ultimo_entry       = entry;
            ultimo_entry_valido = true;
            deve_registrar      = true;
        }
        else if (ultimo_entry_valido)
        {
            entry = ultimo_entry;
            deve_registrar = true;
            ESP_LOGW(TAG,
                     "Sem leitura válida na janela de %u ms; reutilizando valor anterior",
                     (unsigned)SENSOR_JANELA_MS);
        }
        else
        {
            ESP_LOGW(TAG,
                     "Nenhuma leitura válida disponível na janela de %u ms",
                     (unsigned)SENSOR_JANELA_MS);
        }

        if (deve_registrar)
        {
            if (data_logger_append(&entry))
            {
                // Formatação direta no log para evitar corrupção de buffer
                if (isfinite(entry.temp_ar) && isfinite(entry.umid_ar)) {
                    ESP_LOGI(TAG,
                             "Log salvo! Ar: %.2f C / %.2f %%, Solo: %.2f C / %.2f %%, Lux: %.1f, DPV: %.3f kPa",
                             entry.temp_ar,
                             entry.umid_ar,
                             entry.temp_solo,
                             entry.umid_solo,
                             entry.luminosidade,
                             entry.dpv);
                } else {
                    ESP_LOGI(TAG,
                             "Log salvo! Ar: -- C / -- %%, Solo: %.2f C / %.2f %%, Lux: %.1f, DPV: %.3f kPa",
                             entry.temp_solo,
                             entry.umid_solo,
                             entry.luminosidade,
                             entry.dpv);
                }

                // sinaliza flash de gravação
                atuadores_sinalizar_gravacao();
            }
        }

        int64_t elapsed_ms = (esp_timer_get_time() - janela_inicio) / 1000;
        if (elapsed_ms < (int64_t)intervalo_ms) {
            uint32_t sleep_ms = (uint32_t)(intervalo_ms - elapsed_ms);
            if (sleep_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(sleep_ms));
            } else {
                taskYIELD();
            }
        } else {
            taskYIELD();
        }
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

    // Carrega período de amostragem atual (NVS)
    ESP_ERROR_CHECK(sampling_period_init());
    
    // Carrega período estatístico atual (NVS)
    ESP_ERROR_CHECK(stats_window_init());
    
    // Carrega tolerâncias de cultivo (NVS)
    ESP_ERROR_CHECK(cultivation_tolerance_init());

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
    gui_services_impl.get_sampling_period_ms = sampling_period_get_ms;
    gui_services_impl.set_sampling_period_ms = sampling_period_set_ms;
    gui_services_impl.get_stats_window_count = stats_window_get_count;
    gui_services_impl.set_stats_window_count = stats_window_set_count;
    gui_services_impl.build_history_json = build_history_json_wrapper;
    gui_services_impl.get_recent_stats  = data_logger_get_recent_stats;
    gui_services_impl.get_cultivation_tolerance = get_cultivation_tolerance_wrapper;
    gui_services_impl.set_cultivation_tolerance = set_cultivation_tolerance_wrapper;
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
