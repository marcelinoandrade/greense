#ifndef CONFIG_H
#define CONFIG_H

// Wi-Fi
#include "secrets.h"

// HTTP/HTTPS
#define URL_POST       "http://greense.com.br/termica"

// Sensores
#define SENSOR_READ_INTERVAL 5  // segundos

// Horários de aquisição (HH:MM)
#define ACQUISITION_TIMES \
    { \
        {12, 10},   \
        {12, 12},   \
        {12, 14},   \
        {12, 16}    \
    }

#endif // CONFIG_H




