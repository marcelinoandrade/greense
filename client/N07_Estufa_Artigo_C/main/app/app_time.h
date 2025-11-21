#ifndef APP_TIME_H
#define APP_TIME_H

#include <time.h>
#include <stdbool.h>
#include "../config.h"

// Inicializar sincronização NTP
bool app_time_init(void);

// Obter timestamp Unix atual
time_t app_time_get_unix_timestamp(void);

// Verificar se o timestamp é válido (NTP sincronizado)
bool app_time_is_valid(void);

// Obter string de tempo formatada (para logs)
void app_time_get_formatted(char *buffer, size_t buffer_size);

// Obter próximo horário de aquisição baseado no agendamento
time_t app_time_get_next_acquisition_time(const schedule_time_t schedule[], int schedule_size);

// Formatar duração em formato legível (ex: "2 horas e 15 minutos")
void app_time_format_duration(time_t seconds, char *buffer, size_t buffer_size);

#endif // APP_TIME_H

