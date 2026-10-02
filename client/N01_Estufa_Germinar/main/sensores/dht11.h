#ifndef DHT11_H
#define DHT11_H

#include "driver/gpio.h"
#include <stdbool.h>

void dht11_init(gpio_num_t gpio);
bool dht11_read(gpio_num_t gpio, float *temperature, float *humidity);

#endif
