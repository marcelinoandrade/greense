#ifndef ENS160_H
#define ENS160_H

#include <stdbool.h>
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENS160_ADDR 0x53
#define ENS160_STANDARD_MODE 0x02

typedef struct {
    i2c_port_t i2c_port;
    bool initialized;
} ens160_t;

bool ens160_init(ens160_t *dev, i2c_port_t i2c_port);
void ens160_calibrate(ens160_t *dev, float temperature, float humidity);
float ens160_get_eco2(ens160_t *dev);

#ifdef __cplusplus
}
#endif

#endif // ENS160_H