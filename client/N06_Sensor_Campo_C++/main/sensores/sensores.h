#ifndef SENSORES_H
#define SENSORES_H

typedef struct {
    float temp_solo;
    float umid_solo;
} sensor_data_t;

void sensores_init(void);
sensor_data_t sensores_ler_dados(void);

#endif // SENSORES_H
