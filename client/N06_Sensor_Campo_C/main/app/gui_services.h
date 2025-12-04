#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Interface de Serviços para GUI
 * ============================================================
 * Esta interface permite que GUI acesse serviços do APP
 * sem conhecer detalhes de implementação.
 */

typedef struct {
    float min;
    float max;
    float avg;
    float latest;
    bool  has_data;
} gui_sensor_stats_t;

typedef struct gui_recent_stats {
    int window_samples;
    int total_samples;
    size_t storage_used_bytes;
    size_t storage_total_bytes;
    gui_sensor_stats_t temp_ar;
    gui_sensor_stats_t umid_ar;
    gui_sensor_stats_t temp_solo;
    gui_sensor_stats_t umid_solo;
    gui_sensor_stats_t luminosidade;
    gui_sensor_stats_t dpv;
} gui_recent_stats_t;

typedef struct {
    /* Sensores */
    float (*get_temp_air)(void);
    float (*get_humid_air)(void);
    float (*get_temp_soil)(void);
    int   (*get_soil_raw)(void);
    
    /* Conversão e calibração */
    float (*get_soil_pct)(int raw);
    void  (*get_calibration)(float *seco, float *molhado);
    esp_err_t (*set_calibration)(float seco, float molhado);
    
    /* Período de amostragem */
    uint32_t (*get_sampling_period_ms)(void);
    esp_err_t (*set_sampling_period_ms)(uint32_t period_ms);
    
    /* Período estatístico (número de amostras) */
    int (*get_stats_window_count)(void);
    esp_err_t (*set_stats_window_count)(int count);
    
    /* Histórico */
    char* (*build_history_json)(void);
    bool  (*get_recent_stats)(int max_samples, gui_recent_stats_t *stats_out);
    
    /* Tolerâncias de cultivo */
    void (*get_cultivation_tolerance)(float *temp_ar_min, float *temp_ar_max,
                                      float *umid_ar_min, float *umid_ar_max,
                                      float *temp_solo_min, float *temp_solo_max,
                                      float *umid_solo_min, float *umid_solo_max,
                                      float *luminosidade_min, float *luminosidade_max,
                                      float *dpv_min, float *dpv_max);
    esp_err_t (*set_cultivation_tolerance)(float temp_ar_min, float temp_ar_max,
                                           float umid_ar_min, float umid_ar_max,
                                           float temp_solo_min, float temp_solo_max,
                                           float umid_solo_min, float umid_solo_max,
                                           float luminosidade_min, float luminosidade_max,
                                           float dpv_min, float dpv_max);
    
    /* Manutenção */
    esp_err_t (*clear_logged_data)(void);
} gui_services_t;

/**
 * @brief Registra serviços do APP para uso pela GUI
 * 
 * Deve ser chamado por app_main antes de inicializar GUI
 * 
 * @param services Estrutura com ponteiros para funções do APP
 */
void gui_services_register(const gui_services_t *services);

/**
 * @brief Obtém serviços registrados
 * 
 * Retorna NULL se serviços não foram registrados
 */
const gui_services_t* gui_services_get(void);

#ifdef __cplusplus
}
#endif

