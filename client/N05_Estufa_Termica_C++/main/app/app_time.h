#ifndef APP_TIME_H
#define APP_TIME_H

#include <time.h>
#include <stdbool.h>

// Inicializar sincronização NTP
bool app_time_init(void);

// Obter timestamp Unix atual
time_t app_time_get_unix_timestamp(void);

// Obter próximo horário de aquisição
time_t app_time_get_next_acquisition_time(void);

// Verificar se é hora de fazer aquisição
bool app_time_is_acquisition_time(void);

// Verificar se o timestamp é válido (NTP sincronizado)
bool app_time_is_valid(void);

// Obter string de tempo formatada (para logs)
void app_time_get_formatted(char *buffer, size_t buffer_size);

// Formatar duração em formato legível (ex: "2 horas e 15 minutos")
void app_time_format_duration(time_t seconds, char *buffer, size_t buffer_size);

#endif // APP_TIME_H

