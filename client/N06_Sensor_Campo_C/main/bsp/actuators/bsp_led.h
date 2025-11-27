#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t led_bsp_init(void);
void led_bsp_on(void);
void led_bsp_off(void);
void led_bsp_toggle(void);

#ifdef __cplusplus
}
#endif

