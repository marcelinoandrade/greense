#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t dht11_bsp_init(void);
bool dht11_bsp_is_available(void);
esp_err_t dht11_bsp_read(float *temperature, float *humidity);

#ifdef __cplusplus
}
#endif


