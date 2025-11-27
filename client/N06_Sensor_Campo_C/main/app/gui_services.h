#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

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
    
    /* Histórico */
    char* (*build_history_json)(void);
    
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

