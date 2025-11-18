#ifndef APP_THERMAL_H
#define APP_THERMAL_H

#include "freertos/FreeRTOS.h"

#define APP_THERMAL_LINHAS 24
#define APP_THERMAL_COLS   32
#define APP_THERMAL_TOTAL  (APP_THERMAL_LINHAS * APP_THERMAL_COLS)

bool app_thermal_capture_frame(float out[APP_THERMAL_TOTAL], TickType_t timeout_ticks);

#endif // APP_THERMAL_H




