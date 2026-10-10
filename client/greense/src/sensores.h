#ifndef SENSORES_H
#define SENSORES_H

#include "greense_app.h"

typedef struct {
    float temp;
    float umid;
    float co2;
    float luz;
    int agua_min;
    int agua_max;
    float temp_reserv_int;
    float ph;
    float ec;
    float temp_reserv_ext;
    float temp_externa;
    float umid_externa;
    int umid_solo_raw;
    float umid_solo_pct;
} sensor_data_t;

void sensores_init(const greense_app_config_t *cfg);
sensor_data_t sensores_ler_dados(void);

#endif
