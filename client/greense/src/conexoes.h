#ifndef CONEXAO_H
#define CONEXAO_H

#include "greense_app.h"
#include <stdbool.h>

void conexao_wifi_init(const greense_app_config_t *cfg);
bool conexao_wifi_is_connected(void);

void conexao_mqtt_start(const greense_app_config_t *cfg);
bool conexao_mqtt_is_connected(void);
bool conexao_mqtt_publish(const char *topic, const char *message);

#endif
