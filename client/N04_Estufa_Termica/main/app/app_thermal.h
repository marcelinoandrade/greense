#ifndef APP_THERMAL_H
#define APP_THERMAL_H

#include "freertos/FreeRTOS.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "esp_err.h"

#define APP_THERMAL_LINHAS 24
#define APP_THERMAL_COLS   32
#define APP_THERMAL_TOTAL  (APP_THERMAL_LINHAS * APP_THERMAL_COLS)

// Estrutura para frame térmico com timestamp
typedef struct {
    time_t timestamp;  // Timestamp Unix do momento da captura
    float temps[APP_THERMAL_TOTAL];  // 768 valores de temperatura (24×32)
} thermal_frame_t;

// Captura um frame térmico (sem timestamp)
bool app_thermal_capture_frame(float out[APP_THERMAL_TOTAL], TickType_t timeout_ticks);

// Obter próximo número de sequência (incrementa e salva na NVS)
uint32_t app_thermal_get_next_sequence(void);

// Resetar contador de sequência
esp_err_t app_thermal_reset_sequence(void);

#endif // APP_THERMAL_H

