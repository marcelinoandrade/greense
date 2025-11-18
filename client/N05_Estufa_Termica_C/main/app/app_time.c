#include "app_time.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "../config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <time.h>

#define TAG "APP_TIME"

// Tabela de horários de aquisição (HH:MM)
static const struct {
    int hour;
    int minute;
} acquisition_times[] = ACQUISITION_TIMES;

static const int num_acquisition_times = sizeof(acquisition_times) / sizeof(acquisition_times[0]);
static bool time_synced = false;

static void time_sync_notification_cb(struct timeval *tv) {
    time_synced = true;
    ESP_LOGI(TAG, "✅ Tempo sincronizado via NTP");
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "Data/Hora atual: %04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

bool app_time_init(void) {
    ESP_LOGI(TAG, "Inicializando sincronização NTP...");
    
    // Configurar servidores NTP (Brasil) - usando funções não-deprecated
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "a.st1.ntp.br");
    esp_sntp_setservername(2, "b.st1.ntp.br");
    
    // Configurar timezone (Brasília: UTC-3)
    setenv("TZ", "BRT3", 1);
    tzset();
    
    // Callback de sincronização
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    
    // Iniciar SNTP
    esp_sntp_init();
    
    ESP_LOGI(TAG, "Aguardando sincronização NTP...");
    
    // Aguardar sincronização (até 30 segundos - aumentado)
    int retry = 0;
    while (!time_synced && retry < 300) {  // 30 segundos
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }
    
    if (time_synced) {
        ESP_LOGI(TAG, "✅ NTP sincronizado com sucesso");
        return true;
    } else {
        ESP_LOGW(TAG, "⚠️ NTP não sincronizado após 30s");
        return false;
    }
}

time_t app_time_get_unix_timestamp(void) {
    time_t now;
    time(&now);
    return now;
}

bool app_time_is_valid(void) {
    time_t now = app_time_get_unix_timestamp();
    // Timestamp válido deve ser maior que 2020-01-01 (1577836800)
    // Isso garante que NTP sincronizou corretamente
    return (now > 1577836800);
}

time_t app_time_get_next_acquisition_time(void) {
    time_t now = app_time_get_unix_timestamp();
    
    // Se timestamp inválido, retornar 0 para indicar erro
    if (!app_time_is_valid()) {
        ESP_LOGW(TAG, "⚠️ Timestamp inválido, NTP não sincronizado");
        return 0;
    }
    
    struct tm timeinfo, next_time;
    localtime_r(&now, &timeinfo);
    
    // Copiar estrutura atual
    memcpy(&next_time, &timeinfo, sizeof(struct tm));
    
    // Encontrar próximo horário de aquisição
    for (int i = 0; i < num_acquisition_times; i++) {
        // Resetar estrutura para hoje
        memcpy(&next_time, &timeinfo, sizeof(struct tm));
        
        next_time.tm_hour = acquisition_times[i].hour;
        next_time.tm_min = acquisition_times[i].minute;
        next_time.tm_sec = 0;
        next_time.tm_isdst = -1;
        
        time_t next = mktime(&next_time);
        
        // Validar se mktime retornou valor válido
        if (next == -1) {
            ESP_LOGE(TAG, "Erro ao calcular horário %02d:%02d", acquisition_times[i].hour, acquisition_times[i].minute);
            continue;
        }
        
        // Se o horário ainda não passou hoje
        if (next > now) {
            return next;
        }
    }
    
    // Se não encontrou hoje, usar o primeiro horário de amanhã
    memcpy(&next_time, &timeinfo, sizeof(struct tm));
    next_time.tm_hour = acquisition_times[0].hour;
    next_time.tm_min = acquisition_times[0].minute;
    next_time.tm_sec = 0;
    next_time.tm_mday++;
    next_time.tm_isdst = -1;
    
    time_t next = mktime(&next_time);
    if (next == -1) {
        ESP_LOGE(TAG, "Erro ao calcular horário de amanhã");
        return 0;  // Retornar erro
    }
    
    return next;
}

bool app_time_is_acquisition_time(void) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Verificar se o horário atual corresponde a algum horário de aquisição
    for (int i = 0; i < num_acquisition_times; i++) {
        if (timeinfo.tm_hour == acquisition_times[i].hour &&
            timeinfo.tm_min == acquisition_times[i].minute &&
            timeinfo.tm_sec < 30) {  // Janela de 30 segundos
            return true;
        }
    }
    
    return false;
}

void app_time_get_formatted(char *buffer, size_t buffer_size) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

void app_time_format_duration(time_t seconds, char *buffer, size_t buffer_size) {
    // Tratar valores negativos
    if (seconds < 0) {
        snprintf(buffer, buffer_size, "tempo negativo");
        return;
    }
    
    // Menos de 1 minuto: mostrar em segundos
    if (seconds < 60) {
        snprintf(buffer, buffer_size, "%lld segundos", (long long)seconds);
        return;
    }
    
    long long days = (long long)seconds / 86400;
    long long hours = ((long long)seconds % 86400) / 3600;
    long long minutes = ((long long)seconds % 3600) / 60;
    long long secs = (long long)seconds % 60;
    
    // Formatar de forma legível
    if (days > 0) {
        if (hours > 0 && minutes > 0) {
            snprintf(buffer, buffer_size, "%lld dias, %lld horas e %lld minutos", days, hours, minutes);
        } else if (hours > 0) {
            snprintf(buffer, buffer_size, "%lld dias e %lld horas", days, hours);
        } else if (minutes > 0) {
            snprintf(buffer, buffer_size, "%lld dias e %lld minutos", days, minutes);
        } else {
            snprintf(buffer, buffer_size, "%lld dias", days);
        }
    } else if (hours > 0) {
        if (minutes > 0) {
            snprintf(buffer, buffer_size, "%lld horas e %lld minutos", hours, minutes);
        } else {
            snprintf(buffer, buffer_size, "%lld horas", hours);
        }
    } else {
        if (secs > 0) {
            snprintf(buffer, buffer_size, "%lld minutos e %lld segundos", minutes, secs);
        } else {
            snprintf(buffer, buffer_size, "%lld minutos", minutes);
        }
    }
}

