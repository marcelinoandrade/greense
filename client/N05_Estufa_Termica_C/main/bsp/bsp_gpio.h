#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "esp_err.h"

#define BSP_LED_PIN 8

esp_err_t bsp_gpio_init(void);
esp_err_t bsp_gpio_set_level(int pin, int level);
int bsp_gpio_get_level(int pin);

#endif // BSP_GPIO_H




