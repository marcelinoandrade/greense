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
        {22, 50},   \
        {23, 50},   \
        {0, 50},    \
        {1, 50},    \
        {2, 50},    \
        {3, 50},    \
        {4, 50},    \
        {5, 50},    \
        {6, 50},    \
        {7, 50},    \
        {8, 50},    \
        {9, 50},    \
        {10, 10},   \
        {16, 0},    \
        {21, 50},   \
        {22, 10}    \
    }

#endif // CONFIG_H




