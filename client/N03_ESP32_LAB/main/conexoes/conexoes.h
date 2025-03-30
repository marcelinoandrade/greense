#ifndef CONEXAO_H
#define CONEXAO_H

#include <stdbool.h>

void conexao_wifi_init(void);
bool conexao_wifi_is_connected(void);

void conexao_mqtt_start(void);
bool conexao_mqtt_is_connected(void);
bool conexao_mqtt_publish(const char *topic, const char *message);

#endif // CONEXAO_H
