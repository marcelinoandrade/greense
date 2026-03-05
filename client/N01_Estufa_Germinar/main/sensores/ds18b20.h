#ifndef DS18B20_H
#define DS18B20_H

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

void ds18b20_init(gpio_num_t gpio);
float ds18b20_read_temperature(gpio_num_t gpio);

#ifdef __cplusplus
}
#endif

#endif // DS18B20_H
