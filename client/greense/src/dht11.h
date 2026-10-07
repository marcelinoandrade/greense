#ifndef DHT11_H
#define DHT11_H

#include "driver/gpio.h"
#include <stdbool.h>

/* 1 = imprime a duração dos 40 bits e os bytes crus. */
#ifndef DHT11_DEBUG
#define DHT11_DEBUG 0
#endif

void dht11_init(gpio_num_t gpio);
bool dht11_read(gpio_num_t gpio, float *temperature, float *humidity);

#endif
