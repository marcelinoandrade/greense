/* Servidor AP + HTTP para visualização e calibração
 *
 * Rotas:
 *   GET /            painel com gráficos e status
 *   GET /history     últimos pontos em JSON
 *   GET /download    CSV completo
 *   GET /calibra     página de calibração
 *   GET /set_calibra?seco=XXXX&molhado=YYYY   salva calibração
 *   GET /sampling    seleciona período de amostragem
 *   GET /set_sampling?periodo=XXXX            salva período
 */

#include "gui_http_server.h"
#include "logo.h"
#include "../../bsp/network/bsp_wifi_ap.h"
#include "../../app/gui_services.h"
#include "../../bsp/board.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include <sys/stat.h>
#include <errno.h>

static const char *TAG = "GUI_HTTP_SERVER";

static httpd_handle_t server_handle = NULL;

#define PRESETS_FILE_PATH BSP_SPIFFS_MOUNT "/presets.json"

typedef struct {
    uint32_t ms;
    const char *label;
} sampling_option_def_t;

static const sampling_option_def_t SAMPLING_OPTIONS[] = {
    { 10 * 1000,        "10 segundos" },
    { 60 * 1000,        "1 minuto" },
    { 10 * 60 * 1000,   "10 minutos" },
    { 60 * 60 * 1000,   "1 hora" },
    { 6 * 60 * 60 * 1000,  "6 horas" },
    { 12 * 60 * 60 * 1000, "12 horas" },
};

static const size_t SAMPLING_OPTION_COUNT =
    sizeof(SAMPLING_OPTIONS) / sizeof(SAMPLING_OPTIONS[0]);

static const sampling_option_def_t* find_sampling_option(uint32_t ms)
{
    for (size_t i = 0; i < SAMPLING_OPTION_COUNT; ++i) {
        if (SAMPLING_OPTIONS[i].ms == ms) {
            return &SAMPLING_OPTIONS[i];
        }
    }
    return NULL;
}

static void format_duration_ms(uint64_t duration_ms, char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return;
    }

    if (duration_ms == 0) {
        snprintf(out, out_len, "0 ms");
        return;
    }

    const uint64_t second = 1000ULL;
    const uint64_t minute = 60ULL * second;
    const uint64_t hour   = 60ULL * minute;
    const uint64_t day    = 24ULL * hour;

    if (duration_ms >= day) {
        unsigned long long days  = duration_ms / day;
        unsigned long long hours = (duration_ms % day) / hour;
        if (hours > 0) {
            snprintf(out, out_len, "%llu d %llu h", days, hours);
        } else {
            snprintf(out, out_len, "%llu d", days);
        }
    } else if (duration_ms >= hour) {
        unsigned long long hours   = duration_ms / hour;
        unsigned long long minutes = (duration_ms % hour) / minute;
        if (minutes > 0) {
            snprintf(out, out_len, "%llu h %llu min", hours, minutes);
        } else {
            snprintf(out, out_len, "%llu h", hours);
        }
    } else if (duration_ms >= minute) {
        unsigned long long minutes = duration_ms / minute;
        unsigned long long seconds = (duration_ms % minute) / second;
        if (seconds > 0) {
            snprintf(out, out_len, "%llu min %llu s", minutes, seconds);
        } else {
            snprintf(out, out_len, "%llu min", minutes);
        }
    } else if (duration_ms >= second) {
        unsigned long long seconds = duration_ms / second;
        unsigned long long ms_rem  = duration_ms % second;
        if (ms_rem > 0) {
            snprintf(out, out_len, "%llu s %llu ms", seconds, ms_rem);
        } else {
            snprintf(out, out_len, "%llu s", seconds);
        }
    } else {
        snprintf(out, out_len, "%llu ms", duration_ms);
    }
}

/* -------------------------------------------------------------------------- */
/* Função auxiliar para gerar menu de navegação                               */
/* -------------------------------------------------------------------------- */
static void build_nav_menu(const char *current_page, char *out, size_t out_size)
{
    const char *menu_items[][2] = {
        {"/", "Monitoramento"},
        {"/config", "Configuração"}
    };
    
    size_t used = 0;
    used += snprintf(out + used, out_size - used,
        "<nav class='main-nav'>"
        "<div class='nav-container'>");
    
    for (int i = 0; i < 2; i++) {
        const char *url = menu_items[i][0];
        const char *label = menu_items[i][1];
        bool is_active = false;
        
        // Página de Monitoramento
        if (i == 0) {
            is_active = (strcmp(current_page, "/") == 0);
        }
        // Página de Configuração (ativa também para subpáginas /sampling e /calibra)
        else if (i == 1) {
            is_active = (strcmp(current_page, "/config") == 0 ||
                        strcmp(current_page, "/sampling") == 0 ||
                        strcmp(current_page, "/calibra") == 0);
        }
        
        used += snprintf(out + used, out_size - used,
            "<a href='%s' class='nav-item%s'>%s</a>",
            url,
            is_active ? " active" : "",
            label);
    }
    
    used += snprintf(out + used, out_size - used,
        "</div>"
        "</nav>");
}

/* -------------------------------------------------------------------------- */
/* Handlers HTTP                                                              */
/* -------------------------------------------------------------------------- */

/* Estrutura para definir presets de cultivo */
typedef struct {
    const char *name;
    float temp_ar_min, temp_ar_max;
    float umid_ar_min, umid_ar_max;
    float temp_solo_min, temp_solo_max;
    float umid_solo_min, umid_solo_max;
    float luminosidade_min, luminosidade_max;
    float dpv_min, dpv_max;
} cultivation_preset_t;

static const cultivation_preset_t presets[] = {
    {
        .name = "Padrão",
        .temp_ar_min = 20.0f, .temp_ar_max = 30.0f,
        .umid_ar_min = 50.0f, .umid_ar_max = 80.0f,
        .temp_solo_min = 18.0f, .temp_solo_max = 25.0f,
        .umid_solo_min = 40.0f, .umid_solo_max = 80.0f,
        .luminosidade_min = 500.0f, .luminosidade_max = 2000.0f,
        .dpv_min = 0.5f, .dpv_max = 2.0f
    },
    {
        .name = "Tomate",
        .temp_ar_min = 18.0f, .temp_ar_max = 28.0f,
        .umid_ar_min = 60.0f, .umid_ar_max = 85.0f,
        .temp_solo_min = 16.0f, .temp_solo_max = 24.0f,
        .umid_solo_min = 60.0f, .umid_solo_max = 85.0f,
        .luminosidade_min = 1000.0f, .luminosidade_max = 3000.0f,
        .dpv_min = 0.8f, .dpv_max = 1.8f
    },
    {
        .name = "Morango",
        .temp_ar_min = 15.0f, .temp_ar_max = 25.0f,
        .umid_ar_min = 70.0f, .umid_ar_max = 90.0f,
        .temp_solo_min = 14.0f, .temp_solo_max = 22.0f,
        .umid_solo_min = 70.0f, .umid_solo_max = 90.0f,
        .luminosidade_min = 800.0f, .luminosidade_max = 2500.0f,
        .dpv_min = 0.6f, .dpv_max = 1.5f
    },
    {
        .name = "Alface",
        .temp_ar_min = 10.0f, .temp_ar_max = 22.0f,
        .umid_ar_min = 65.0f, .umid_ar_max = 85.0f,
        .temp_solo_min = 10.0f, .temp_solo_max = 20.0f,
        .umid_solo_min = 60.0f, .umid_solo_max = 85.0f,
        .luminosidade_min = 600.0f, .luminosidade_max = 2000.0f,
        .dpv_min = 0.5f, .dpv_max = 1.8f
    },
    {
        .name = "Rúcula",
        .temp_ar_min = 12.0f, .temp_ar_max = 24.0f,
        .umid_ar_min = 60.0f, .umid_ar_max = 80.0f,
        .temp_solo_min = 12.0f, .temp_solo_max = 22.0f,
        .umid_solo_min = 55.0f, .umid_solo_max = 80.0f,
        .luminosidade_min = 700.0f, .luminosidade_max = 2200.0f,
        .dpv_min = 0.6f, .dpv_max = 1.7f
    }
};

#define PRESET_COUNT (sizeof(presets) / sizeof(presets[0]))
#define FLOAT_EPSILON 0.01f

/* Detecta qual preset está ativo baseado nos valores de tolerância */
static const char* detect_active_preset(float temp_ar_min, float temp_ar_max,
                                        float umid_ar_min, float umid_ar_max,
                                        float temp_solo_min, float temp_solo_max,
                                        float umid_solo_min, float umid_solo_max,
                                        float luminosidade_min, float luminosidade_max,
                                        float dpv_min, float dpv_max)
{
    for (size_t i = 0; i < PRESET_COUNT; i++) {
        const cultivation_preset_t *p = &presets[i];
        if (fabsf(temp_ar_min - p->temp_ar_min) < FLOAT_EPSILON &&
            fabsf(temp_ar_max - p->temp_ar_max) < FLOAT_EPSILON &&
            fabsf(umid_ar_min - p->umid_ar_min) < FLOAT_EPSILON &&
            fabsf(umid_ar_max - p->umid_ar_max) < FLOAT_EPSILON &&
            fabsf(temp_solo_min - p->temp_solo_min) < FLOAT_EPSILON &&
            fabsf(temp_solo_max - p->temp_solo_max) < FLOAT_EPSILON &&
            fabsf(umid_solo_min - p->umid_solo_min) < FLOAT_EPSILON &&
            fabsf(umid_solo_max - p->umid_solo_max) < FLOAT_EPSILON &&
            fabsf(luminosidade_min - p->luminosidade_min) < FLOAT_EPSILON &&
            fabsf(luminosidade_max - p->luminosidade_max) < FLOAT_EPSILON &&
            fabsf(dpv_min - p->dpv_min) < FLOAT_EPSILON &&
            fabsf(dpv_max - p->dpv_max) < FLOAT_EPSILON) {
            return p->name;
        }
    }
    return "Personalizado";
}

/* Página principal / */
static esp_err_t handle_dashboard(httpd_req_t *req)
{
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Serviços não disponíveis");
        return ESP_FAIL;
    }
    
    gui_recent_stats_t recent_stats;
    memset(&recent_stats, 0, sizeof(recent_stats));
    bool stats_available = false;
    int stats_window = 10; // padrão
    if (svc->get_stats_window_count) {
        stats_window = svc->get_stats_window_count();
    }
    if (svc->get_recent_stats) {
        stats_available = svc->get_recent_stats(stats_window, &recent_stats);
    }
    
    // Obtém valores de tolerância configurados
    float temp_ar_min = 20.0f, temp_ar_max = 30.0f;
    float umid_ar_min = 50.0f, umid_ar_max = 80.0f;
    float temp_solo_min = 18.0f, temp_solo_max = 25.0f;
    float umid_solo_min = 40.0f, umid_solo_max = 80.0f;
    float luminosidade_min = 500.0f, luminosidade_max = 2000.0f;
    float dpv_min = 0.5f, dpv_max = 2.0f;
    if (svc->get_cultivation_tolerance) {
        svc->get_cultivation_tolerance(&temp_ar_min, &temp_ar_max,
                                      &umid_ar_min, &umid_ar_max,
                                      &temp_solo_min, &temp_solo_max,
                                      &umid_solo_min, &umid_solo_max,
                                      &luminosidade_min, &luminosidade_max,
                                      &dpv_min, &dpv_max);
    }
    
    // Detecta qual preset está ativo
    const char *active_preset = detect_active_preset(temp_ar_min, temp_ar_max,
                                                     umid_ar_min, umid_ar_max,
                                                     temp_solo_min, temp_solo_max,
                                                     umid_solo_min, umid_solo_max,
                                                     luminosidade_min, luminosidade_max,
                                                     dpv_min, dpv_max);
    char preset_text[64];
    snprintf(preset_text, sizeof(preset_text), "Preset: %s", active_preset);

    int window_samples = (stats_available) ? recent_stats.window_samples : 0;

    uint32_t sampling_ms = 0;
    if (svc->get_sampling_period_ms) {
        sampling_ms = svc->get_sampling_period_ms();
    }

    const sampling_option_def_t *sampling_opt = find_sampling_option(sampling_ms);
    char sampling_period_text[48];
    if (sampling_opt) {
        snprintf(sampling_period_text, sizeof(sampling_period_text), "%s", sampling_opt->label);
    } else if (sampling_ms > 0) {
        format_duration_ms((uint64_t)sampling_ms, sampling_period_text, sizeof(sampling_period_text));
    } else {
        snprintf(sampling_period_text, sizeof(sampling_period_text), "--");
    }

    char window_span_text[48];
    if (sampling_ms > 0 && window_samples > 0) {
        format_duration_ms((uint64_t)sampling_ms * (uint64_t)window_samples,
                           window_span_text,
                           sizeof(window_span_text));
    } else {
        snprintf(window_span_text, sizeof(window_span_text), "--");
    }

    char stats_info_text[256];
    if (stats_available && window_samples > 0) {
        snprintf(stats_info_text,
                 sizeof(stats_info_text),
                 "Baseado nas &uacute;ltimas %d medi&ccedil;&otilde;es (%s). Coleta a cada %s.",
                 window_samples,
                 window_span_text,
                 sampling_period_text);
    } else {
        snprintf(stats_info_text,
                 sizeof(stats_info_text),
                 "Aguardando medi&ccedil;&otilde;es. Os dados aparecer&atilde;o em breve.");
    }

    char window_summary_text[128];
    char total_samples_text[96];
    char memory_text[96];
    snprintf(window_summary_text, sizeof(window_summary_text), "--");
    snprintf(total_samples_text, sizeof(total_samples_text), "--");
    snprintf(memory_text, sizeof(memory_text), "--");

    if (stats_available) {
        if (window_samples > 0 && sampling_ms > 0 && window_span_text[0] != '\0') {
            snprintf(window_summary_text,
                     sizeof(window_summary_text),
                     "%d (%s)",
                     window_samples,
                     window_span_text);
        } else if (window_samples > 0) {
            snprintf(window_summary_text,
                     sizeof(window_summary_text),
                     "%d medi&ccedil;&otilde;es",
                     window_samples);
        } else {
            snprintf(window_summary_text,
                     sizeof(window_summary_text),
                     "Sem dados ainda");
        }

        snprintf(total_samples_text,
                 sizeof(total_samples_text),
                 "%d registros",
                 recent_stats.total_samples);

        if (recent_stats.storage_total_bytes > 0) {
            double used_kb  = (double)recent_stats.storage_used_bytes / 1024.0;
            double total_kb = (double)recent_stats.storage_total_bytes / 1024.0;
            double pct      = (total_kb > 0.0) ? (used_kb / total_kb) * 100.0 : 0.0;
            snprintf(memory_text,
                     sizeof(memory_text),
                     "%.1f%% (%.1f kB de %.1f kB)",
                     pct,
                     used_kb,
                     total_kb);
        }
    }

    char temp_ar_avg_text[32], temp_ar_min_text[32], temp_ar_max_text[32], temp_ar_last_text[32], temp_ar_limits_text[64];
    char umid_ar_avg_text[32], umid_ar_min_text[32], umid_ar_max_text[32], umid_ar_last_text[32], umid_ar_limits_text[64];
    char temp_solo_avg_text[32], temp_solo_min_text[32], temp_solo_max_text[32], temp_solo_last_text[32], temp_solo_limits_text[64];
    char umid_solo_avg_text[32], umid_solo_min_text[32], umid_solo_max_text[32], umid_solo_last_text[32], umid_solo_limits_text[64];
    char luminosidade_avg_text[32], luminosidade_min_text[32], luminosidade_max_text[32], luminosidade_last_text[32], luminosidade_limits_text[64];
    char dpv_avg_text[32], dpv_min_text[32], dpv_max_text[32], dpv_last_text[32], dpv_limits_text[64];

    if (stats_available && recent_stats.temp_ar.has_data) {
        snprintf(temp_ar_avg_text, sizeof(temp_ar_avg_text), "%.1f&nbsp;&deg;C", recent_stats.temp_ar.avg);
        snprintf(temp_ar_min_text, sizeof(temp_ar_min_text), "%.1f&nbsp;&deg;C", recent_stats.temp_ar.min);
        snprintf(temp_ar_max_text, sizeof(temp_ar_max_text), "%.1f&nbsp;&deg;C", recent_stats.temp_ar.max);
        snprintf(temp_ar_last_text, sizeof(temp_ar_last_text), "%.1f&nbsp;&deg;C", recent_stats.temp_ar.latest);
    } else {
        snprintf(temp_ar_avg_text, sizeof(temp_ar_avg_text), "--");
        snprintf(temp_ar_min_text, sizeof(temp_ar_min_text), "--");
        snprintf(temp_ar_max_text, sizeof(temp_ar_max_text), "--");
        snprintf(temp_ar_last_text, sizeof(temp_ar_last_text), "--");
    }

    if (stats_available && recent_stats.umid_ar.has_data) {
        snprintf(umid_ar_avg_text, sizeof(umid_ar_avg_text), "%.1f&nbsp;%%", recent_stats.umid_ar.avg);
        snprintf(umid_ar_min_text, sizeof(umid_ar_min_text), "%.1f&nbsp;%%", recent_stats.umid_ar.min);
        snprintf(umid_ar_max_text, sizeof(umid_ar_max_text), "%.1f&nbsp;%%", recent_stats.umid_ar.max);
        snprintf(umid_ar_last_text, sizeof(umid_ar_last_text), "%.1f&nbsp;%%", recent_stats.umid_ar.latest);
    } else {
        snprintf(umid_ar_avg_text, sizeof(umid_ar_avg_text), "--");
        snprintf(umid_ar_min_text, sizeof(umid_ar_min_text), "--");
        snprintf(umid_ar_max_text, sizeof(umid_ar_max_text), "--");
        snprintf(umid_ar_last_text, sizeof(umid_ar_last_text), "--");
    }

    if (stats_available && recent_stats.temp_solo.has_data) {
        snprintf(temp_solo_avg_text, sizeof(temp_solo_avg_text), "%.1f&nbsp;&deg;C", recent_stats.temp_solo.avg);
        snprintf(temp_solo_min_text, sizeof(temp_solo_min_text), "%.1f&nbsp;&deg;C", recent_stats.temp_solo.min);
        snprintf(temp_solo_max_text, sizeof(temp_solo_max_text), "%.1f&nbsp;&deg;C", recent_stats.temp_solo.max);
        snprintf(temp_solo_last_text, sizeof(temp_solo_last_text), "%.1f&nbsp;&deg;C", recent_stats.temp_solo.latest);
    } else {
        snprintf(temp_solo_avg_text, sizeof(temp_solo_avg_text), "--");
        snprintf(temp_solo_min_text, sizeof(temp_solo_min_text), "--");
        snprintf(temp_solo_max_text, sizeof(temp_solo_max_text), "--");
        snprintf(temp_solo_last_text, sizeof(temp_solo_last_text), "--");
    }

    if (stats_available && recent_stats.umid_solo.has_data) {
        snprintf(umid_solo_avg_text, sizeof(umid_solo_avg_text), "%.1f&nbsp;%%", recent_stats.umid_solo.avg);
        snprintf(umid_solo_min_text, sizeof(umid_solo_min_text), "%.1f&nbsp;%%", recent_stats.umid_solo.min);
        snprintf(umid_solo_max_text, sizeof(umid_solo_max_text), "%.1f&nbsp;%%", recent_stats.umid_solo.max);
        snprintf(umid_solo_last_text, sizeof(umid_solo_last_text), "%.1f&nbsp;%%", recent_stats.umid_solo.latest);
    } else {
        snprintf(umid_solo_avg_text, sizeof(umid_solo_avg_text), "--");
        snprintf(umid_solo_min_text, sizeof(umid_solo_min_text), "--");
        snprintf(umid_solo_max_text, sizeof(umid_solo_max_text), "--");
        snprintf(umid_solo_last_text, sizeof(umid_solo_last_text), "--");
    }

    if (stats_available && recent_stats.luminosidade.has_data) {
        snprintf(luminosidade_avg_text, sizeof(luminosidade_avg_text), "%.0f&nbsp;lux", recent_stats.luminosidade.avg);
        snprintf(luminosidade_min_text, sizeof(luminosidade_min_text), "%.0f&nbsp;lux", recent_stats.luminosidade.min);
        snprintf(luminosidade_max_text, sizeof(luminosidade_max_text), "%.0f&nbsp;lux", recent_stats.luminosidade.max);
        snprintf(luminosidade_last_text, sizeof(luminosidade_last_text), "%.0f&nbsp;lux", recent_stats.luminosidade.latest);
    } else {
        snprintf(luminosidade_avg_text, sizeof(luminosidade_avg_text), "--");
        snprintf(luminosidade_min_text, sizeof(luminosidade_min_text), "--");
        snprintf(luminosidade_max_text, sizeof(luminosidade_max_text), "--");
        snprintf(luminosidade_last_text, sizeof(luminosidade_last_text), "--");
    }

    if (stats_available && recent_stats.dpv.has_data) {
        snprintf(dpv_avg_text, sizeof(dpv_avg_text), "%.1f&nbsp;kPa", recent_stats.dpv.avg);
        snprintf(dpv_min_text, sizeof(dpv_min_text), "%.1f&nbsp;kPa", recent_stats.dpv.min);
        snprintf(dpv_max_text, sizeof(dpv_max_text), "%.1f&nbsp;kPa", recent_stats.dpv.max);
        snprintf(dpv_last_text, sizeof(dpv_last_text), "%.1f&nbsp;kPa", recent_stats.dpv.latest);
    } else {
        snprintf(dpv_avg_text, sizeof(dpv_avg_text), "--");
        snprintf(dpv_min_text, sizeof(dpv_min_text), "--");
        snprintf(dpv_max_text, sizeof(dpv_max_text), "--");
        snprintf(dpv_last_text, sizeof(dpv_last_text), "--");
    }

    // Formata textos dos limites predefinidos
    snprintf(temp_ar_limits_text, sizeof(temp_ar_limits_text), "%.1f - %.1f&nbsp;&deg;C", temp_ar_min, temp_ar_max);
    snprintf(umid_ar_limits_text, sizeof(umid_ar_limits_text), "%.1f - %.1f&nbsp;%%", umid_ar_min, umid_ar_max);
    snprintf(temp_solo_limits_text, sizeof(temp_solo_limits_text), "%.1f - %.1f&nbsp;&deg;C", temp_solo_min, temp_solo_max);
    snprintf(umid_solo_limits_text, sizeof(umid_solo_limits_text), "%.1f - %.1f&nbsp;%%", umid_solo_min, umid_solo_max);
    snprintf(luminosidade_limits_text, sizeof(luminosidade_limits_text), "%.0f - %.0f&nbsp;lux", luminosidade_min, luminosidade_max);
    snprintf(dpv_limits_text, sizeof(dpv_limits_text), "%.1f - %.1f&nbsp;kPa", dpv_min, dpv_max);

    char card_temp_ar[512];
    char card_umid_ar[512];
    char card_temp_solo[512];
    char card_umid_solo[512];
    char card_luminosidade[512];
    char card_dpv[512];

    snprintf(card_temp_ar, sizeof(card_temp_ar),
             "<div class='stats-card' id='card-temp-ar'>"
             "<h3>Temp. do ar</h3>"
             "<ul>"
             "<li><span>M&eacute;dia</span><strong>%s</strong></li>"
             "<li><span>Menor</span><strong>%s</strong></li>"
             "<li><span>Maior</span><strong>%s</strong></li>"
             "<li><span>Agora</span><strong>%s</strong></li>"
             "<li><span>Limites</span><strong>%s</strong></li>"
             "</ul>"
             "</div>",
             temp_ar_avg_text,
             temp_ar_min_text,
             temp_ar_max_text,
             temp_ar_last_text,
             temp_ar_limits_text);

    snprintf(card_umid_ar, sizeof(card_umid_ar),
             "<div class='stats-card' id='card-umid-ar'>"
             "<h3>Umidade ar</h3>"
             "<ul>"
             "<li><span>M&eacute;dia</span><strong>%s</strong></li>"
             "<li><span>Menor</span><strong>%s</strong></li>"
             "<li><span>Maior</span><strong>%s</strong></li>"
             "<li><span>Agora</span><strong>%s</strong></li>"
             "<li><span>Limites</span><strong>%s</strong></li>"
             "</ul>"
             "</div>",
             umid_ar_avg_text,
             umid_ar_min_text,
             umid_ar_max_text,
             umid_ar_last_text,
             umid_ar_limits_text);

    snprintf(card_temp_solo, sizeof(card_temp_solo),
             "<div class='stats-card' id='card-temp-solo'>"
             "<h3>Temp. do solo</h3>"
             "<ul>"
             "<li><span>M&eacute;dia</span><strong>%s</strong></li>"
             "<li><span>Menor</span><strong>%s</strong></li>"
             "<li><span>Maior</span><strong>%s</strong></li>"
             "<li><span>Agora</span><strong>%s</strong></li>"
             "<li><span>Limites</span><strong>%s</strong></li>"
             "</ul>"
             "</div>",
             temp_solo_avg_text,
             temp_solo_min_text,
             temp_solo_max_text,
             temp_solo_last_text,
             temp_solo_limits_text);

    snprintf(card_umid_solo, sizeof(card_umid_solo),
             "<div class='stats-card' id='card-umid-solo'>"
             "<h3>Umidade do solo</h3>"
             "<ul>"
             "<li><span>M&eacute;dia</span><strong>%s</strong></li>"
             "<li><span>Menor</span><strong>%s</strong></li>"
             "<li><span>Maior</span><strong>%s</strong></li>"
             "<li><span>Agora</span><strong>%s</strong></li>"
             "<li><span>Limites</span><strong>%s</strong></li>"
             "</ul>"
             "</div>",
             umid_solo_avg_text,
             umid_solo_min_text,
             umid_solo_max_text,
             umid_solo_last_text,
             umid_solo_limits_text);

    snprintf(card_luminosidade, sizeof(card_luminosidade),
             "<div class='stats-card' id='card-luminosidade'>"
             "<h3>Luminosidade</h3>"
             "<ul>"
             "<li><span>M&eacute;dia</span><strong>%s</strong></li>"
             "<li><span>Menor</span><strong>%s</strong></li>"
             "<li><span>Maior</span><strong>%s</strong></li>"
             "<li><span>Agora</span><strong>%s</strong></li>"
             "<li><span>Limites</span><strong>%s</strong></li>"
             "</ul>"
             "</div>",
             luminosidade_avg_text,
             luminosidade_min_text,
             luminosidade_max_text,
             luminosidade_last_text,
             luminosidade_limits_text);

    snprintf(card_dpv, sizeof(card_dpv),
             "<div class='stats-card' id='card-dpv'>"
             "<h3>DPV</h3>"
             "<ul>"
             "<li><span>M&eacute;dia</span><strong>%s</strong></li>"
             "<li><span>Menor</span><strong>%s</strong></li>"
             "<li><span>Maior</span><strong>%s</strong></li>"
             "<li><span>Agora</span><strong>%s</strong></li>"
             "<li><span>Limites</span><strong>%s</strong></li>"
             "</ul>"
             "</div>",
             dpv_avg_text,
             dpv_min_text,
             dpv_max_text,
             dpv_last_text,
             dpv_limits_text);

    char stats_grid_html[3500];
    snprintf(stats_grid_html,
             sizeof(stats_grid_html),
             "<div class='stats-grid'>%s%s%s%s%s%s</div>",
             card_temp_ar,
             card_umid_ar,
             card_temp_solo,
             card_umid_solo,
             card_luminosidade,
             card_dpv);

    char stats_meta_html[640];
    snprintf(stats_meta_html,
             sizeof(stats_meta_html),
             "<div class='meta-grid'>"
             "<div class='meta-item'><strong>Per&iacute;odo</strong>%s</div>"
             "<div class='meta-item'><strong>Total</strong>%s</div>"
             "<div class='meta-item'><strong>Mem&oacute;ria</strong>%s</div>"
             "</div>",
             window_summary_text,
             total_samples_text,
             memory_text);

    /* Gera menu de navegação */
    char nav_menu[512];
    build_nav_menu("/", nav_menu, sizeof(nav_menu));

    /* Buffer alocado no heap para evitar stack overflow */
    char *page = (char *)malloc(20000);
    if (!page) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro de memória");
        return ESP_FAIL;
    }
    
    int len = snprintf(
        page, 20000,
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset='utf-8'/>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
        "<title>Sensor de Campo</title>"
        "<style>"
        "*{box-sizing:border-box}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,'Helvetica Neue',Arial,sans-serif;margin:0;padding:20px;"
        "background:linear-gradient(180deg,#f0f7f2 0%%,#fafcfa 50%%,#ffffff 100%%);color:#1a2e1f;line-height:1.6}"
        ".content{max-width:1200px;margin:0 auto}"
        ".card{background:#ffffff;border-radius:20px;padding:28px 32px;margin-bottom:24px;"
        "box-shadow:0 2px 8px rgba(0,0,0,0.04),0 8px 24px rgba(0,0,0,0.06);border:1px solid rgba(0,0,0,0.04);transition:transform 0.2s,box-shadow 0.2s}"
        ".card:hover{transform:translateY(-2px);box-shadow:0 4px 12px rgba(0,0,0,0.08),0 12px 32px rgba(0,0,0,0.1)}"
        ".hero{background:linear-gradient(135deg,#e8f5e9 0%%,#f1f8e9 100%%);border:1px solid rgba(76,175,80,0.1)}"
        "h1{margin:0 0 8px;font-size:32px;font-weight:700;color:#2e7d32;letter-spacing:-0.5px}"
        "h2{margin:0 0 12px;font-size:24px;font-weight:600;color:#388e3c;letter-spacing:-0.3px}"
        ".subtitle{color:#5a6c5e;margin:0 0 20px;font-size:15px;line-height:1.6;font-weight:400}"
        ".info-text{font-size:14px;color:#6b7c6f;margin:16px 0;line-height:1.6}"
        ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px;margin-top:20px}"
        "canvas{max-width:100%%;height:200px;border:1px solid #e8ede9;border-radius:12px;background:#fafbfa;transition:border-color 0.2s}"
        "canvas:hover{border-color:#c8e6c9}"
        "a.button{display:inline-flex;align-items:center;justify-content:center;padding:12px 24px;"
        "background:linear-gradient(135deg,#4caf50 0%%,#388e3c 100%%);color:#fff;border-radius:10px;text-decoration:none;"
        "font-size:14px;font-weight:600;box-shadow:0 4px 12px rgba(76,175,80,0.3);transition:all 0.3s;border:none}"
        "a.button:hover{transform:translateY(-2px);box-shadow:0 6px 16px rgba(76,175,80,0.4)}"
        "a.button:active{transform:translateY(0)}"
        ".badge{display:inline-flex;align-items:center;gap:6px;background:linear-gradient(135deg,#c8e6c9 0%%,#a5d6a7 100%%);"
        "color:#1b5e20;font-size:11px;font-weight:700;border-radius:20px;padding:6px 14px;text-transform:uppercase;letter-spacing:0.5px}"
        ".preset-badge{display:inline-flex;align-items:center;gap:6px;background:linear-gradient(135deg,#fff9c4 0%%,#fff59d 100%%);"
        "color:#7d6608;font-size:11px;font-weight:600;border-radius:20px;padding:6px 14px;margin-left:10px;"
        "border:1px solid #ffd54f;box-shadow:0 2px 4px rgba(255,193,7,0.2)}"
        ".caption{font-size:12px;color:#8a9b8d;margin-top:12px;font-style:italic}"
        "table{font-size:14px;width:100%%;border-collapse:separate;border-spacing:0;margin-top:16px;border-radius:10px;overflow:hidden}"
        "table tr:first-child td{background:linear-gradient(135deg,#e8f5e9 0%%,#c8e6c9 100%%);font-weight:600;color:#1b5e20;padding:14px 16px;border-bottom:2px solid #a5d6a7}"
        "table tr:not(:first-child) td{padding:12px 16px;border-bottom:1px solid #f0f4f1;transition:background 0.2s}"
        "table tr:not(:first-child):hover td{background:#f8fbf9}"
        "td:first-child{font-weight:500;color:#2e4a34;min-width:140px}"
        "td:nth-child(2){color:#388e3c;font-weight:500}"
        "td:nth-child(3){color:#6b7c6f;font-size:13px}"
        ".stats-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:16px;margin-top:20px}"
        ".stats-card{border:1px solid #e8ede9;border-radius:16px;padding:20px;background:#fafbfa;transition:all 0.3s;position:relative;overflow:hidden}"
        ".stats-card::before{content:'';position:absolute;top:0;left:0;right:0;height:3px;background:linear-gradient(90deg,#4caf50 0%%,#8bc34a 100%%)}"
        ".stats-card:hover{transform:translateY(-4px);box-shadow:0 8px 20px rgba(0,0,0,0.1);border-color:#c8e6c9}"
        ".stats-card h3{margin:0 0 16px;font-size:16px;font-weight:600;color:#2e7d32}"
        ".stats-card ul{list-style:none;padding:0;margin:0}"
        ".stats-card li{display:flex;justify-content:space-between;align-items:center;font-size:13px;margin:8px 0;padding:6px 0;color:#5a6c5e;border-bottom:1px solid #f0f4f1}"
        ".stats-card li:last-child{border-bottom:none}"
        ".stats-card li strong{color:#1a2e1f;font-weight:600;font-size:14px}"
        ".meta-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px;margin-top:20px}"
        ".meta-item{background:linear-gradient(135deg,#f1f8e9 0%%,#e8f5e9 100%%);border:1px solid #c8e6c9;border-radius:14px;padding:16px;font-size:13px;color:#2e4a34;transition:all 0.2s}"
        ".meta-item:hover{transform:translateY(-2px);box-shadow:0 4px 12px rgba(0,0,0,0.08)}"
        ".meta-item strong{display:block;font-size:10px;color:#5a6c57;text-transform:uppercase;letter-spacing:1px;margin-bottom:8px;font-weight:700}"
        ".warn{background:linear-gradient(135deg,#ffebee 0%%,#ffcdd2 100%%) !important;border-color:#ef5350 !important;box-shadow:0 4px 12px rgba(244,67,54,0.15) !important}"
        ".warn::before{background:linear-gradient(90deg,#f44336 0%%,#e53935 100%%) !important}"
        "canvas.warn{background:#fff5f5 !important;border-color:#ef9a9a !important}"
        ".main-nav{background:#ffffff;border-radius:16px;padding:12px;margin-bottom:24px;box-shadow:0 2px 8px rgba(0,0,0,0.06),0 4px 16px rgba(0,0,0,0.04);border:1px solid rgba(0,0,0,0.04)}"
        ".nav-container{display:flex;gap:6px;flex-wrap:wrap;justify-content:center}"
        ".nav-item{padding:10px 20px;border-radius:10px;text-decoration:none;font-size:14px;font-weight:500;color:#6b7c6f;transition:all 0.3s;background:transparent;position:relative}"
        ".nav-item:hover{background:#f1f8e9;color:#2e7d32;transform:translateY(-2px)}"
        ".nav-item.active{background:linear-gradient(135deg,#4caf50 0%%,#388e3c 100%%);color:#fff;font-weight:600;box-shadow:0 4px 12px rgba(76,175,80,0.3)}"
        ".footer-note{margin-top:32px;font-size:12px;color:#8a9b8d;font-style:italic;text-align:center}"
        "</style>"
        "</head>"
        "<body>"
        "<div class='content'>"
        "%s"
        "<div style='text-align:center;margin-bottom:20px'>%s</div>"
        "<div class='card hero'>"
        "<div style='display:flex;align-items:center;flex-wrap:wrap;gap:8px;margin-bottom:8px'>"
        "<div class='badge'>greenSe Campo</div>"
        "<div class='preset-badge'>%s</div>"
        "</div>"
        "<h1>Monitoramento</h1>"
        "<p class='subtitle'>Veja temperatura, umidade e luz do seu cultivo em tempo real.</p>"
        "<p class='info-text'>Os gr&aacute;ficos mostram as condi&ccedil;&otilde;es do seu cultivo. "
        "As linhas laranja mostram os valores ideais. "
        "Se ficar amarelo, algo precisa de aten&ccedil;&atilde;o.</p>"
        "</div>"

        "<div class='card stats'>"
        "<h2>Resumo</h2>"
        "<p class='info-text'>M&eacute;dia, menor, maior, valor atual e limites predefinidos de cada medida.</p>"
        "<p style='background:#fff3cd;border:1px solid #ffc107;border-radius:8px;padding:12px;margin:16px 0;color:#856404;font-size:13px'><strong>ℹ️ Observa&ccedil;&atilde;o:</strong> O alarme (fundo vermelho) acontece quando 3 entre as 4 &uacute;ltimas medidas ficam fora dos limites predefinidos.</p>"
        "<p class='info-text'>%s</p>"
        "%s"
        "%s"
        "</div>"

        "<div class='card'>"
        "<h2>Gr&aacute;ficos</h2>"
        "<p class='info-text'>Evolu&ccedil;&atilde;o das condi&ccedil;&otilde;es ao longo do tempo. "
        "Linhas laranja mostram os valores ideais.</p>"
        "<div class='grid'>"
        "<div><canvas id='chart_temp_ar' width='320' height='180'></canvas></div>"
        "<div><canvas id='chart_umid_ar' width='320' height='180'></canvas></div>"
        "<div><canvas id='chart_temp_solo' width='320' height='180'></canvas></div>"
        "<div><canvas id='chart_umid_solo' width='320' height='180'></canvas></div>"
        "<div><canvas id='chart_luminosidade' width='320' height='180'></canvas></div>"
        "<div><canvas id='chart_dpv' width='320' height='180'></canvas></div>"
        "</div>"
        "<p class='caption'>Mostrando as &uacute;ltimas %d medi&ccedil;&otilde;es.</p>"
        "</div>"


        /* Chart mínimo inline. Só inclua este bloco se você NÃO
           já estiver servindo Chart.js de CDN. Se você já tem Chart.js
           carregado em outro lugar, remova este bloco inteiro. */
        "<script>"
        "!function(){"
        "function Chart(c,o){this.ctx=c;this.data=o.data;this.type=o.type;this.options=o.options||{};this._init()}"
        "Chart.prototype._init=function(){this._draw()};"
        "Chart.prototype.update=function(){this._draw()};"
        "Chart.prototype._draw=function(){"
        " const ctx=this.ctx;"
        " const w=ctx.canvas.width;const h=ctx.canvas.height;"
        " ctx.clearRect(0,0,w,h);"
        " ctx.strokeStyle='#000';ctx.lineWidth=1;"
        " ctx.strokeRect(0,0,w,h);"
        " const labels=this.data.labels||[];"
        " const vals=(this.data.datasets&&this.data.datasets[0]&&this.data.datasets[0].data)||[];"
        " const lbl=(this.data.datasets&&this.data.datasets[0]&&this.data.datasets[0].label)||'';"
        " let minVal=vals.length?Math.min.apply(null,vals):0;"
        " let maxVal=vals.length?Math.max.apply(null,vals):1;"
        " if(isFinite(this.options.fixedMin)) minVal=this.options.fixedMin;"
        " if(isFinite(this.options.fixedMax)) maxVal=this.options.fixedMax;"
        " if(minVal===maxVal){maxVal=minVal+1;}"
        " const range=(maxVal-minVal)||1;"
        " const ticks=4;"
        " ctx.strokeStyle='#ccc';"
        " ctx.fillStyle='#555';"
        " ctx.font='9px sans-serif';"
        " for(let t=0;t<=ticks;t++){"
        "   const value=minVal+(range/ticks)*t;"
        "   const y=h-((value-minVal)/range)*h;"
        "   ctx.beginPath();"
        "   ctx.moveTo(0,y);ctx.lineTo(6,y);"
        "   ctx.stroke();"
        "   ctx.fillText(value.toFixed(0),8,y-2);"
        " }"
        " if(isFinite(this.options.toleranceMin)&&isFinite(this.options.toleranceMax)){"
        "   const tolMin=this.options.toleranceMin;"
        "   const tolMax=this.options.toleranceMax;"
        "   if(tolMin>=minVal&&tolMin<=maxVal){"
        "     const yMin=h-((tolMin-minVal)/range)*h;"
        "     ctx.setLineDash([4,4]);"
        "     ctx.strokeStyle='#ff9800';"
        "     ctx.lineWidth=1.5;"
        "     ctx.beginPath();"
        "     ctx.moveTo(0,yMin);ctx.lineTo(w,yMin);"
        "     ctx.stroke();"
        "     ctx.setLineDash([]);"
        "     ctx.fillStyle='#ff9800';"
        "     ctx.font='8px sans-serif';"
        "     ctx.fillText('Min: '+tolMin.toFixed(1),w-50,yMin-2);"
        "   }"
        "   if(tolMax>=minVal&&tolMax<=maxVal){"
        "     const yMax=h-((tolMax-minVal)/range)*h;"
        "     ctx.setLineDash([4,4]);"
        "     ctx.strokeStyle='#ff9800';"
        "     ctx.lineWidth=1.5;"
        "     ctx.beginPath();"
        "     ctx.moveTo(0,yMax);ctx.lineTo(w,yMax);"
        "     ctx.stroke();"
        "     ctx.setLineDash([]);"
        "     ctx.fillStyle='#ff9800';"
        "     ctx.font='8px sans-serif';"
        "     ctx.fillText('Max: '+tolMax.toFixed(1),w-50,yMax+10);"
        "   }"
        " }"
        " ctx.beginPath();ctx.strokeStyle='#1976d2';ctx.lineWidth=2;"
        " if(vals.length===0){ctx.moveTo(0,h);ctx.lineTo(w,h);}"
        " for(let i=0;i<vals.length;i++){"
        "   const clamped=Math.min(Math.max(vals[i],minVal),maxVal);"
        "   const x=(i/(vals.length-1||1))*w;"
        "   const y=h-((clamped-minVal)/range)*h;"
        "   if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);"
        " }"
        " ctx.stroke();"
        " ctx.fillStyle='#000';ctx.font='10px sans-serif';"
        " ctx.fillText(lbl,4,12);"
        "};"
        "window.Chart=Chart;"
        "}();"
        "</script>"

        "<script>"
        "async function atualiza(){"
        " try{"
        "  const r=await fetch('/history');"
        "  const j=await r.json();"

        "  function extrairXY(arr){"
        "    const xs=[];const ys=[];"
        "    for(let i=0;i<arr.length;i++){"
        "      xs.push(arr[i][0]);"
        "      ys.push(arr[i][1]);"
        "    }"
        "    return {xs,ys};"
        "  }"

        "  const tAr   = extrairXY(j.temp_ar_points||[]);"
        "  const uAr   = extrairXY(j.umid_ar_points||[]);"
        "  const tSolo = extrairXY(j.temp_solo_points||[]);"
        "  const uSolo = extrairXY(j.umid_solo_points||[]);"
        "  const lum   = extrairXY(j.luminosidade_points||[]);"
        "  const dpv   = extrairXY(j.dpv_points||[]);"

        "  const tol={temp_ar_min:%.1f,temp_ar_max:%.1f,umid_ar_min:%.1f,umid_ar_max:%.1f,"
        "temp_solo_min:%.1f,temp_solo_max:%.1f,umid_solo_min:%.1f,umid_solo_max:%.1f,"
        "luminosidade_min:%.0f,luminosidade_max:%.0f,dpv_min:%.1f,dpv_max:%.1f};"
        "  function countOutliers(values,min,max){"
        "    const last=values.slice(-4);"
        "    let out=0;"
        "    for(const v of last){"
        "      if(!isFinite(v)) continue;"
        "      if((isFinite(min)&&v<min)||(isFinite(max)&&v>max)) out++;"
        "    }"
        "    return {out,lastCount:last.length};"
        "  }"
        "  function applyWarn(cardId,canvasId,values,min,max){"
        "    const {out,lastCount}=countOutliers(values,min,max);"
        "    const warn=(lastCount>=4)&&(out>=3);"
        "    [document.getElementById(cardId),document.getElementById(canvasId)].forEach(el=>{"
        "      if(!el)return;"
        "      el.classList.toggle('warn',warn);"
        "    });"
        "  }"
        "  desenha('chart_temp_ar','Temp. Ar',tAr.xs,tAr.ys,10,40,tol.temp_ar_min,tol.temp_ar_max);"
        "  desenha('chart_umid_ar','Umidade Ar',uAr.xs,uAr.ys,0,100,tol.umid_ar_min,tol.umid_ar_max);"
        "  desenha('chart_temp_solo','Temp. Solo',tSolo.xs,tSolo.ys,10,40,tol.temp_solo_min,tol.temp_solo_max);"
        "  desenha('chart_umid_solo','Umidade Solo',uSolo.xs,uSolo.ys,0,100,tol.umid_solo_min,tol.umid_solo_max);"
        "  desenha('chart_luminosidade','Luz',lum.xs,lum.ys,0,2500,tol.luminosidade_min,tol.luminosidade_max);"
        "  desenha('chart_dpv','DPV',dpv.xs,dpv.ys,0,3,tol.dpv_min,tol.dpv_max);"
        "  applyWarn('card-temp-ar','chart_temp_ar',tAr.ys,tol.temp_ar_min,tol.temp_ar_max);"
        "  applyWarn('card-umid-ar','chart_umid_ar',uAr.ys,tol.umid_ar_min,tol.umid_ar_max);"
        "  applyWarn('card-temp-solo','chart_temp_solo',tSolo.ys,tol.temp_solo_min,tol.temp_solo_max);"
        "  applyWarn('card-umid-solo','chart_umid_solo',uSolo.ys,tol.umid_solo_min,tol.umid_solo_max);"
        "  applyWarn('card-luminosidade','chart_luminosidade',lum.ys,tol.luminosidade_min,tol.luminosidade_max);"
        "  applyWarn('card-dpv','chart_dpv',dpv.ys,tol.dpv_min,tol.dpv_max);"
        " }catch(e){console.log('erro /history',e);}"
        "}"

        "function desenha(id,titulo,labels,data,minY,maxY,tolMin,tolMax){"
        "  const ctx=document.getElementById(id).getContext('2d');"
        "  if(!ctx._chartRef){"
        "    ctx._chartRef=new Chart(ctx,{"
        "      type:'line',"
        "      data:{labels:labels,datasets:[{label:titulo,data:data,fill:false}]},"
        "      options:{fixedMin:minY,fixedMax:maxY,toleranceMin:tolMin,toleranceMax:tolMax}"
        "    });"
        "  }else{"
        "    ctx._chartRef.data.labels=labels;"
        "    ctx._chartRef.data.datasets[0].data=data;"
        "    ctx._chartRef.data.datasets[0].label=titulo;"
        "    ctx._chartRef.options.fixedMin=minY;"
        "    ctx._chartRef.options.fixedMax=maxY;"
        "    if(isFinite(tolMin)) ctx._chartRef.options.toleranceMin=tolMin;"
        "    if(isFinite(tolMax)) ctx._chartRef.options.toleranceMax=tolMax;"
        "    ctx._chartRef.update();"
        "  }"
        "}"

        "setInterval(atualiza,5000);"
        "atualiza();"
        "</script>"

        "</div>"
        "<p class='footer-note'>greenSe Campo | Tecnologia desenhada para agricultura conectada.</p>"
        "</body>"
        "</html>",
        nav_menu,
        greense_logo_svg,
        preset_text,
        stats_info_text,
        stats_grid_html,
        stats_meta_html,
        stats_window,
        temp_ar_min, temp_ar_max,
        umid_ar_min, umid_ar_max,
        temp_solo_min, temp_solo_max,
        umid_solo_min, umid_solo_max,
        luminosidade_min, luminosidade_max,
        dpv_min, dpv_max
    );

    httpd_resp_set_type(req, "text/html");
    esp_err_t ret = httpd_resp_send(req, page, len);
    /* Libera memória após enviar resposta */
    if (page) {
        free(page);
    }
    return ret;
}

 
/* /history -> últimos pontos em JSON */
static esp_err_t handle_history(httpd_req_t *req)
{
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
    }
    
    char *json = svc->build_history_json();
    if (!json) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
    }
 
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ret;
}

/* Página de configurações */
static esp_err_t handle_config(httpd_req_t *req)
{
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL || svc->get_cultivation_tolerance == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Serviços não disponíveis");
        return ESP_FAIL;
    }

    /* Obtém valores de tolerância para detectar preset */
    float temp_ar_min = 20.0f, temp_ar_max = 30.0f;
    float umid_ar_min = 50.0f, umid_ar_max = 80.0f;
    float temp_solo_min = 18.0f, temp_solo_max = 25.0f;
    float umid_solo_min = 40.0f, umid_solo_max = 80.0f;
    float luminosidade_min = 500.0f, luminosidade_max = 2000.0f;
    float dpv_min = 0.5f, dpv_max = 2.0f;
    svc->get_cultivation_tolerance(&temp_ar_min, &temp_ar_max,
                                    &umid_ar_min, &umid_ar_max,
                                    &temp_solo_min, &temp_solo_max,
                                    &umid_solo_min, &umid_solo_max,
                                    &luminosidade_min, &luminosidade_max,
                                    &dpv_min, &dpv_max);
    
    /* Detecta qual preset está ativo */
    const char *active_preset = detect_active_preset(temp_ar_min, temp_ar_max,
                                                     umid_ar_min, umid_ar_max,
                                                     temp_solo_min, temp_solo_max,
                                                     umid_solo_min, umid_solo_max,
                                                     luminosidade_min, luminosidade_max,
                                                     dpv_min, dpv_max);
    char preset_text[64];
    snprintf(preset_text, sizeof(preset_text), "Preset: %s", active_preset);

    /* Gera menu de navegação */
    char nav_menu[512];
    build_nav_menu("/config", nav_menu, sizeof(nav_menu));

    /* Constrói página dinamicamente */
    const size_t page_capacity = 12288;
    char *page = (char *)malloc(page_capacity);
    if (!page) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro de memória");
        return ESP_FAIL;
    }

    int len = snprintf(page, page_capacity,
        "<!DOCTYPE html>"
        "<html><head><meta charset='utf-8'/>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
        "<title>Configurações - greenSe Campo</title>"
        "<style>"
        "*{box-sizing:border-box}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,'Helvetica Neue',Arial,sans-serif;background:linear-gradient(180deg,#f0f7f2 0%%,#fafcfa 50%%,#ffffff 100%%);color:#1a2e1f;margin:0;padding:20px;line-height:1.6}"
        ".wrapper{max-width:800px;margin:0 auto}"
        ".card{background:#ffffff;border-radius:20px;padding:32px;box-shadow:0 2px 8px rgba(0,0,0,0.04),0 8px 24px rgba(0,0,0,0.06);border:1px solid rgba(0,0,0,0.04);transition:transform 0.2s,box-shadow 0.2s}"
        ".card:hover{transform:translateY(-2px);box-shadow:0 4px 12px rgba(0,0,0,0.08),0 12px 32px rgba(0,0,0,0.1)}"
        ".tag{display:inline-block;padding:6px 14px;border-radius:20px;background:linear-gradient(135deg,#c8e6c9 0%%,#a5d6a7 100%%);color:#1b5e20;font-weight:700;font-size:11px;text-transform:uppercase;letter-spacing:0.5px}"
        "h1{margin:12px 0 12px;font-size:32px;font-weight:700;color:#2e7d32;letter-spacing:-0.5px}"
        ".lead{color:#5a6c5e;margin-bottom:28px;line-height:1.6;font-size:15px}"
        ".actions{display:flex;flex-direction:column;gap:16px}"
        ".action{background:linear-gradient(135deg,#fafbfa 0%%,#f5f7f6 100%%);border-radius:16px;padding:24px;border:1px solid #e8ede9;transition:all 0.3s}"
        ".action:hover{transform:translateY(-2px);box-shadow:0 4px 12px rgba(0,0,0,0.08);border-color:#c8e6c9}"
        ".action p{margin:12px 0 0;font-size:13px;color:#6b7c6f;line-height:1.6}"
        "a.button,button{display:inline-flex;align-items:center;justify-content:center;padding:12px 24px;border:none;border-radius:10px;font-weight:600;color:#fff;text-decoration:none;box-shadow:0 4px 12px rgba(76,175,80,0.3);transition:all 0.3s;font-size:14px}"
        "a.button{background:linear-gradient(135deg,#4caf50 0%%,#388e3c 100%%)}"
        "a.button:hover{transform:translateY(-2px);box-shadow:0 6px 16px rgba(76,175,80,0.4)}"
        "a.button.secondary{background:linear-gradient(135deg,#42a5f5 0%%,#1976d2 100%%);box-shadow:0 4px 12px rgba(25,118,210,0.3)}"
        "a.button.secondary:hover{box-shadow:0 6px 16px rgba(25,118,210,0.4)}"
        "a.button.neutral{background:linear-gradient(135deg,#78909c 0%%,#546e7a 100%%);box-shadow:0 4px 12px rgba(84,110,122,0.3)}"
        "a.button.neutral:hover{box-shadow:0 6px 16px rgba(84,110,122,0.4)}"
        "button.delete{background:linear-gradient(135deg,#ef5350 0%%,#c62828 100%%);width:100%%;box-shadow:0 4px 12px rgba(198,40,40,0.3)}"
        "button.delete:hover{transform:translateY(-2px);box-shadow:0 6px 16px rgba(198,40,40,0.4)}"
        ".footer-note{margin-top:32px;font-size:12px;color:#8a9b8d;font-style:italic;text-align:center}"
        ".preset-badge{display:inline-flex;align-items:center;gap:6px;background:linear-gradient(135deg,#fff9c4 0%%,#fff59d 100%%);"
        "color:#7d6608;font-size:11px;font-weight:600;border-radius:20px;padding:6px 14px;margin-left:10px;"
        "border:1px solid #ffd54f;box-shadow:0 2px 4px rgba(255,193,7,0.2)}"
        ".main-nav{background:#ffffff;border-radius:16px;padding:12px;margin-bottom:24px;box-shadow:0 2px 8px rgba(0,0,0,0.06),0 4px 16px rgba(0,0,0,0.04);border:1px solid rgba(0,0,0,0.04)}"
        ".nav-container{display:flex;gap:6px;flex-wrap:wrap;justify-content:center}"
        ".nav-item{padding:10px 20px;border-radius:10px;text-decoration:none;font-size:14px;font-weight:500;color:#6b7c6f;transition:all 0.3s;background:transparent;position:relative}"
        ".nav-item:hover{background:#f1f8e9;color:#2e7d32;transform:translateY(-2px)}"
        ".nav-item.active{background:linear-gradient(135deg,#4caf50 0%%,#388e3c 100%%);color:#fff;font-weight:600;box-shadow:0 4px 12px rgba(76,175,80,0.3)}"
        "</style>"
        "<script>"
        "function confirmaApagar(){"
        "  if(!confirm('Deseja realmente apagar os dados gravados?')){return false;}"
        "  document.getElementById('clearForm').submit();"
        "  return false;"
        "}"
        "</script>"
        "</head><body>"
        "<div class='wrapper'>"
        "%s"
        "<div style='text-align:center;margin-bottom:20px'>%s</div>"
        "<div class='card'>"
        "<div style='display:flex;align-items:center;flex-wrap:wrap;gap:8px;margin-bottom:8px'>"
        "<span class='tag'>greenSe Campo</span>"
        "<div class='preset-badge'>%s</div>"
        "</div>"
        "<h1>Configura&ccedil;&otilde;es</h1>"
        "<p class='lead'>Ajuste como o sensor coleta dados, defina valores ideais para seu cultivo e baixe os dados quando precisar.</p>"
        "<div class='actions'>"
        "<div class='action'>"
        "<a class='button secondary' href='/sampling'>Amostragem</a>"
        "<p>Defina de quanto em quanto tempo o sensor vai medir e quantas medidas usar nos gr&aacute;ficos.</p>"
        "</div>"
        "<div class='action'>"
        "<a class='button secondary' href='/calibra'>Cultivo</a>"
        "<p>Defina os valores ideais de temperatura, umidade e luz para sua planta e ajuste o sensor de solo.</p>"
        "</div>"
        "<div class='action'>"
        "<a class='button neutral' href='/download'>Baixar dados</a>"
        "<p>Baixe todos os dados coletados para abrir em planilha ou an&aacute;lise.</p>"
        "</div>"
        "<div class='action'>"
        "<form id='clearForm' method='post' action='/clear_data' onsubmit='return confirmaApagar();'>"
        "<button class='delete' type='submit'>Apagar dados</button>"
        "<p>Remove todos os dados salvos. Use apenas quando come&ccedil;ar um novo ciclo de cultivo.</p>"
        "</form>"
        "</div>"
        "</div>"
        "<p class='footer-note'>greenSe Campo | Tecnologia desenhada para agricultura conectada.</p>"
        "</div>"
        "</div>"
        "</body></html>",
        nav_menu,
        greense_logo_svg,
        preset_text);

    if (len < 0 || len >= (int)page_capacity) {
        free(page);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro gerando página");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    esp_err_t ret = httpd_resp_send(req, page, len);
    free(page);
    return ret;
}

static esp_err_t handle_sampling_page(httpd_req_t *req)
{
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL || svc->get_sampling_period_ms == NULL || svc->get_stats_window_count == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Serviços não disponíveis");
        return ESP_FAIL;
    }

    uint32_t current_ms = svc->get_sampling_period_ms();
    const sampling_option_def_t *opt = find_sampling_option(current_ms);

    char current_label[64];
    if (opt) {
        snprintf(current_label, sizeof(current_label), "%s", opt->label);
    } else {
        snprintf(current_label, sizeof(current_label), "%lu ms", (unsigned long)current_ms);
    }

    int current_stats_window = svc->get_stats_window_count();

    char radios[1024] = {0};
    size_t used = 0;
    for (size_t i = 0; i < SAMPLING_OPTION_COUNT && used < sizeof(radios) - 1; ++i) {
        const sampling_option_def_t *item = &SAMPLING_OPTIONS[i];
        int written = snprintf(
            radios + used,
            sizeof(radios) - used,
            "<label class='option'>"
            "<input type='radio' name='periodo' value='%lu' %s>"
            "<span>%s</span>"
            "</label>",
            (unsigned long)item->ms,
            (current_ms == item->ms) ? "checked" : "",
            item->label
        );
        if (written < 0) {
            break;
        }
        used += (size_t)written;
    }

    /* Opções de período estatístico (número de amostras) */
    static const int stats_window_options[] = { 5, 10, 15, 20 };
    static const size_t stats_window_count = sizeof(stats_window_options) / sizeof(stats_window_options[0]);
    
    char stats_radios[512] = {0};
    size_t stats_used = 0;
    for (size_t i = 0; i < stats_window_count && stats_used < sizeof(stats_radios) - 1; ++i) {
        int value = stats_window_options[i];
        int written = snprintf(
            stats_radios + stats_used,
            sizeof(stats_radios) - stats_used,
            "<label class='option'>"
            "<input type='radio' name='stats_window' value='%d' %s>"
            "<span>%d amostras</span>"
            "</label>",
            value,
            (current_stats_window == value) ? "checked" : "",
            value
        );
        if (written < 0) {
            break;
        }
        stats_used += (size_t)written;
    }

    /* Obtém valores de tolerância para detectar preset */
    float temp_ar_min = 20.0f, temp_ar_max = 30.0f;
    float umid_ar_min = 50.0f, umid_ar_max = 80.0f;
    float temp_solo_min = 18.0f, temp_solo_max = 25.0f;
    float umid_solo_min = 40.0f, umid_solo_max = 80.0f;
    float luminosidade_min = 500.0f, luminosidade_max = 2000.0f;
    float dpv_min = 0.5f, dpv_max = 2.0f;
    if (svc->get_cultivation_tolerance) {
        svc->get_cultivation_tolerance(&temp_ar_min, &temp_ar_max,
                                        &umid_ar_min, &umid_ar_max,
                                        &temp_solo_min, &temp_solo_max,
                                        &umid_solo_min, &umid_solo_max,
                                        &luminosidade_min, &luminosidade_max,
                                        &dpv_min, &dpv_max);
    }
    
    /* Detecta qual preset está ativo */
    const char *active_preset = detect_active_preset(temp_ar_min, temp_ar_max,
                                                     umid_ar_min, umid_ar_max,
                                                     temp_solo_min, temp_solo_max,
                                                     umid_solo_min, umid_solo_max,
                                                     luminosidade_min, luminosidade_max,
                                                     dpv_min, dpv_max);
    char preset_text[64];
    snprintf(preset_text, sizeof(preset_text), "Preset: %s", active_preset);

    /* Gera menu de navegação */
    char nav_menu[512];
    build_nav_menu("/sampling", nav_menu, sizeof(nav_menu));

    const size_t page_capacity = 14336;
    char *page = malloc(page_capacity);
    if (!page) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro de memória");
        return ESP_FAIL;
    }

    int len = snprintf(
        page,
        page_capacity,
        "<!DOCTYPE html>"
        "<html><head><meta charset='utf-8'/>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
        "<title>Amostragem - greenSe Campo</title>"
        "<style>"
        "*{box-sizing:border-box}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,'Helvetica Neue',Arial,sans-serif;background:linear-gradient(180deg,#f0f7f2 0%%,#fafcfa 50%%,#ffffff 100%%);color:#1a2e1f;margin:0;padding:20px;line-height:1.6}"
        ".card{background:#ffffff;border-radius:20px;padding:32px;max-width:600px;margin:0 auto;"
        "box-shadow:0 2px 8px rgba(0,0,0,0.04),0 8px 24px rgba(0,0,0,0.06);border:1px solid rgba(0,0,0,0.04);transition:transform 0.2s,box-shadow 0.2s}"
        ".card:hover{transform:translateY(-2px);box-shadow:0 4px 12px rgba(0,0,0,0.08),0 12px 32px rgba(0,0,0,0.1)}"
        ".tag{display:inline-block;padding:6px 14px;border-radius:20px;background:linear-gradient(135deg,#c8e6c9 0%%,#a5d6a7 100%%);"
        "color:#1b5e20;font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:0.5px}"
        "h1{margin:12px 0 12px;font-size:32px;font-weight:700;color:#2e7d32;letter-spacing:-0.5px}"
        "h2{margin:24px 0 12px;font-size:22px;font-weight:600;color:#388e3c;letter-spacing:-0.3px}"
        ".lead{color:#5a6c5e;line-height:1.6;margin-bottom:20px;font-size:15px}"
        ".options{display:flex;flex-direction:column;gap:10px;margin:20px 0}"
        ".option{display:flex;align-items:center;gap:12px;font-size:15px;background:linear-gradient(135deg,#fafbfa 0%%,#f5f7f6 100%%);"
        "border-radius:12px;padding:14px 18px;border:1px solid #e8ede9;transition:all 0.3s;cursor:pointer}"
        ".option:hover{transform:translateX(4px);box-shadow:0 2px 8px rgba(0,0,0,0.06);border-color:#c8e6c9}"
        ".option input[type='radio']{width:20px;height:20px;cursor:pointer;accent-color:#4caf50}"
        "button{padding:14px 24px;border:none;border-radius:10px;background:linear-gradient(135deg,#4caf50 0%%,#388e3c 100%%);"
        "color:#fff;font-size:15px;font-weight:600;cursor:pointer;width:100%%;"
        "box-shadow:0 4px 12px rgba(76,175,80,0.3);transition:all 0.3s;margin-top:8px}"
        "button:hover{transform:translateY(-2px);box-shadow:0 6px 16px rgba(76,175,80,0.4)}"
        "button:active{transform:translateY(0)}"
        "p.hint{font-size:13px;color:#6b7c6f;margin-top:16px;font-style:italic}"
        "a{color:#1976d2;text-decoration:none;transition:color 0.2s}"
        "a:hover{color:#1565c0}"
        ".section{margin-bottom:32px;padding-bottom:24px;border-bottom:2px solid #f0f4f1}"
        ".section:last-child{border-bottom:none;margin-bottom:0;padding-bottom:0}"
        ".preset-badge{display:inline-flex;align-items:center;gap:6px;background:linear-gradient(135deg,#fff9c4 0%%,#fff59d 100%%);"
        "color:#7d6608;font-size:11px;font-weight:600;border-radius:20px;padding:6px 14px;margin-left:10px;"
        "border:1px solid #ffd54f;box-shadow:0 2px 4px rgba(255,193,7,0.2)}"
        ".main-nav{background:#ffffff;border-radius:16px;padding:12px;margin-bottom:24px;box-shadow:0 2px 8px rgba(0,0,0,0.06),0 4px 16px rgba(0,0,0,0.04);border:1px solid rgba(0,0,0,0.04);max-width:600px;margin-left:auto;margin-right:auto}"
        ".nav-container{display:flex;gap:6px;flex-wrap:wrap;justify-content:center}"
        ".nav-item{padding:10px 20px;border-radius:10px;text-decoration:none;font-size:14px;font-weight:500;color:#6b7c6f;transition:all 0.3s;background:transparent;position:relative}"
        ".nav-item:hover{background:#f1f8e9;color:#2e7d32;transform:translateY(-2px)}"
        ".nav-item.active{background:linear-gradient(135deg,#4caf50 0%%,#388e3c 100%%);color:#fff;font-weight:600;box-shadow:0 4px 12px rgba(76,175,80,0.3)}"
        ".footer-note{margin-top:32px;font-size:12px;color:#8a9b8d;font-style:italic;text-align:center}"
        "</style>"
        "</head><body>"
        "<div style='max-width:600px;margin:0 auto 24px'>%s</div>"
        "<div style='text-align:center;margin-bottom:20px'>%s</div>"
        "<div class='card'>"
        "<div style='display:flex;align-items:center;flex-wrap:wrap;gap:8px;margin-bottom:8px'>"
        "<span class='tag'>greenSe Campo</span>"
        "<div class='preset-badge'>%s</div>"
        "</div>"
        "<h1>Amostragem</h1>"
        "<p class='lead'>Defina de quanto em quanto tempo o sensor vai medir e quantas medidas aparecem nos gr&aacute;ficos.</p>"
        "<p style='background:#fff3cd;border:1px solid #ffc107;border-radius:8px;padding:12px;margin:16px 0;color:#856404;font-size:13px'><strong>⚠️ Atenção:</strong> Mudar o tempo de medida vai apagar todos os dados e reiniciar o aparelho.</p>"
        "<form action='/set_sampling' method='get' id='samplingForm' onsubmit='return confirmSamplingChange()'>"
        "<div class='section'>"
        "<h2>Tempo entre Medidas</h2>"
        "<p class='lead'>Escolha de quanto em quanto tempo o sensor vai medir. Tempos menores mostram mais detalhes, tempos maiores economizam bateria.</p>"
        "<div class='options'>%s</div>"
        "<p class='hint'>Tempo atual: <strong>%s</strong></p>"
        "</div>"
        "<div class='section'>"
        "<h2>Quantas Medidas nos Gr&aacute;ficos</h2>"
        "<p class='lead'>Escolha quantas medidas aparecem nos gr&aacute;ficos e no resumo. Mais medidas mostram tend&ecirc;ncias, menos medidas mostram o que aconteceu agora.</p>"
        "<div class='options'>%s</div>"
        "<p class='hint'>Quantidade atual: <strong>%d medidas</strong></p>"
        "</div>"
        "<button type='submit'>Salvar</button>"
        "</form>"
        "</div>"
        "<script>"
        "const currentPeriod = %lu;"
        "function confirmSamplingChange() {"
        "  const form = document.getElementById('samplingForm');"
        "  const formData = new FormData(form);"
        "  const newPeriod = formData.get('periodo');"
        "  if (newPeriod && parseInt(newPeriod) !== currentPeriod) {"
        "    return confirm('⚠️ ATENÇÃO: Alterar a frequência de amostragem apagará TODOS os dados gravados e reiniciará o dispositivo.\\n\\nDeseja continuar?');"
        "  }"
        "  return true;"
        "}"
        "</script>"
        "<p class='footer-note'>greenSe Campo | Tecnologia desenhada para agricultura conectada.</p>"
        "</body></html>",
        nav_menu,
        greense_logo_svg,
        preset_text,
        radios,
        current_label,
        stats_radios,
        current_stats_window,
        (unsigned long)current_ms
    );

    if (len < 0 || len >= (int)page_capacity) {
        free(page);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro gerando página");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    esp_err_t resp = httpd_resp_send(req, page, len);
    free(page);
    return resp;
}

static esp_err_t handle_set_sampling(httpd_req_t *req)
{
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL || svc->set_sampling_period_ms == NULL || svc->set_stats_window_count == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Serviços não disponíveis");
        return ESP_FAIL;
    }

    char qs[256];
    int qs_len = httpd_req_get_url_query_str(req, qs, sizeof(qs));
    if (qs_len < 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Parâmetro ausente");
        return ESP_FAIL;
    }
    if (qs_len >= (int)sizeof(qs) - 1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Query string muito longa");
        return ESP_FAIL;
    }

    char buf_period[32];
    char buf_stats_window[32];
    bool has_period = (httpd_query_key_value(qs, "periodo", buf_period, sizeof(buf_period)) == ESP_OK);
    bool has_stats_window = (httpd_query_key_value(qs, "stats_window", buf_stats_window, sizeof(buf_stats_window)) == ESP_OK);

    if (!has_period && !has_stats_window) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Nenhum parâmetro fornecido");
        return ESP_FAIL;
    }

    char period_label[128] = "";
    char stats_label[64] = "";
    bool period_changed = false;
    uint32_t old_period = 0;

    if (has_period) {
        uint32_t new_period = (uint32_t)strtoul(buf_period, NULL, 10);
        old_period = svc->get_sampling_period_ms();
        
        // Verifica se a frequência realmente mudou
        if (new_period != old_period) {
            period_changed = true;
            ESP_LOGI(TAG, "Frequência de amostragem mudou de %lu ms para %lu ms", 
                     (unsigned long)old_period, (unsigned long)new_period);
        }
        
        if (svc->set_sampling_period_ms(new_period) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Frequência de amostragem não suportada");
            return ESP_FAIL;
        }

        const sampling_option_def_t *opt = find_sampling_option(new_period);
        if (opt) {
            snprintf(period_label, sizeof(period_label), "%s", opt->label);
        } else {
            snprintf(period_label, sizeof(period_label), "%lu ms", (unsigned long)new_period);
        }
    } else {
        // Mantém o valor atual
        uint32_t current_ms = svc->get_sampling_period_ms();
        const sampling_option_def_t *opt = find_sampling_option(current_ms);
        if (opt) {
            snprintf(period_label, sizeof(period_label), "%s", opt->label);
        } else {
            snprintf(period_label, sizeof(period_label), "%lu ms", (unsigned long)current_ms);
        }
    }

    if (has_stats_window) {
        int new_stats_window = (int)strtol(buf_stats_window, NULL, 10);
        if (svc->set_stats_window_count(new_stats_window) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Janela de análise estatística não suportada");
            return ESP_FAIL;
        }
        snprintf(stats_label, sizeof(stats_label), "%d amostras", new_stats_window);
    } else {
        // Mantém o valor atual
        int current_stats = svc->get_stats_window_count();
        snprintf(stats_label, sizeof(stats_label), "%d amostras", current_stats);
    }

    // Se a frequência mudou, limpar dados e reiniciar
    if (period_changed) {
        ESP_LOGI(TAG, "Frequência de amostragem alterada. Limpando dados e reiniciando...");
        
        // Limpar dados gravados
        if (svc->clear_logged_data != NULL) {
            esp_err_t clear_err = svc->clear_logged_data();
            if (clear_err != ESP_OK) {
                ESP_LOGW(TAG, "Erro ao limpar dados: %s", esp_err_to_name(clear_err));
            } else {
                ESP_LOGI(TAG, "Dados limpos com sucesso");
            }
        }
        
        // Enviar página de resposta antes de reiniciar
        const size_t page_capacity = 1024;
        char *page = malloc(page_capacity);
        if (page) {
            int len = snprintf(
                page,
                page_capacity,
                "<!DOCTYPE html><html><head><meta charset='utf-8'/><meta name='viewport' content='width=device-width,initial-scale=1'/>"
                "<title>Reiniciando...</title>"
                "<meta http-equiv='refresh' content='5;url=/'>"
                "</head><body style='font-family:sans-serif;padding:20px;text-align:center'>"
                "<h2>Configuração aplicada com sucesso!</h2>"
                "<p>Frequência de amostragem alterada para: <strong>%s</strong></p>"
                "<p>Janela de análise estatística: <strong>%s</strong></p>"
                "<p style='margin-top:30px;padding:20px;background:#fff3cd;border:1px solid #ffc107;border-radius:8px;color:#856404'>"
                "<strong>⚠️ Dados apagados e dispositivo reiniciando...</strong><br>"
                "O dispositivo será reiniciado em instantes. Aguarde alguns segundos e recarregue a página."
                "</p>"
                "<p style='margin-top:20px;color:#666'>Redirecionando automaticamente em 5 segundos...</p>"
                "</body></html>",
                period_label,
                stats_label
            );
            
            if (len > 0 && len < (int)page_capacity) {
                httpd_resp_set_type(req, "text/html");
                httpd_resp_send(req, page, len);
            }
            free(page);
        }
        
        // Dar tempo para enviar a resposta antes de reiniciar
        vTaskDelay(pdMS_TO_TICKS(500));
        
        ESP_LOGI(TAG, "Reiniciando dispositivo...");
        esp_restart();
        // Nunca chega aqui, mas para evitar warning do compilador
        return ESP_OK;
    }
    
    // Se apenas a janela estatística mudou ou nenhuma mudança na frequência
    const size_t page_capacity = 1024;
    char *page = malloc(page_capacity);
    if (!page) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro de memória");
        return ESP_FAIL;
    }
    
    int len = snprintf(
        page,
        page_capacity,
        "<!DOCTYPE html><html><head><meta charset='utf-8'/><meta name='viewport' content='width=device-width,initial-scale=1'/>"
        "<title>Configuração Salva</title></head><body style='font-family:sans-serif;padding:20px'>"
        "<h2>Configurações salvas com sucesso!</h2>"
        "<p>Frequência de amostragem: <strong>%s</strong></p>"
        "<p>Janela de análise estatística: <strong>%s</strong></p>"
        "<p style='margin-top:20px'><a href='/sampling'>Voltar às configurações</a> | <a href='/'>Monitoramento</a></p>"
        "</body></html>",
        period_label,
        stats_label
    );

    if (len < 0) {
        ESP_LOGE(TAG, "Erro no snprintf: %d", len);
        free(page);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro gerando resposta");
        return ESP_FAIL;
    }
    
    if (len >= (int)page_capacity) {
        ESP_LOGE(TAG, "Buffer insuficiente: necessário %d, disponível %zu", len, page_capacity);
        free(page);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro gerando resposta");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    esp_err_t resp = httpd_resp_send(req, page, len);
    free(page);
    return resp;
}

/* /download -> CSV inteiro */
static esp_err_t handle_download(httpd_req_t *req)
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/log_temp.csv", BSP_SPIFFS_MOUNT);
    
    FILE *f = fopen(filepath, "r");
    if (!f)
    {
        httpd_resp_send_err(req,
                            HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Erro abrindo log");
        return ESP_FAIL;
    }
 
    httpd_resp_set_type(req, "text/csv");
    httpd_resp_set_hdr(req, "Content-Disposition",
                       "attachment; filename=\"log_temp.csv\"");
 
    char buf[256];
    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        if (httpd_resp_sendstr_chunk(req, buf) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }
    fclose(f);
    httpd_resp_sendstr_chunk(req, NULL); /* fim chunked */
    return ESP_OK;
}

/* /calibra -> página HTML com configurações de cultivo e calibração */
static esp_err_t handle_calibra(httpd_req_t *req)
{
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL || svc->get_cultivation_tolerance == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Serviços não disponíveis");
        return ESP_FAIL;
    }
    
    float seco, molhado;
    svc->get_calibration(&seco, &molhado);
 
    int leitura_raw = svc->get_soil_raw();
    
    float temp_ar_min, temp_ar_max, umid_ar_min, umid_ar_max;
    float temp_solo_min, temp_solo_max, umid_solo_min, umid_solo_max;
    float luminosidade_min, luminosidade_max, dpv_min, dpv_max;
    svc->get_cultivation_tolerance(&temp_ar_min, &temp_ar_max,
                                    &umid_ar_min, &umid_ar_max,
                                    &temp_solo_min, &temp_solo_max,
                                    &umid_solo_min, &umid_solo_max,
                                    &luminosidade_min, &luminosidade_max,
                                    &dpv_min, &dpv_max);
    
    // Detecta qual preset está ativo
    const char *active_preset = detect_active_preset(temp_ar_min, temp_ar_max,
                                                     umid_ar_min, umid_ar_max,
                                                     temp_solo_min, temp_solo_max,
                                                     umid_solo_min, umid_solo_max,
                                                     luminosidade_min, luminosidade_max,
                                                     dpv_min, dpv_max);
    char preset_text[64];
    snprintf(preset_text, sizeof(preset_text), "Preset: %s", active_preset);
 
    /* Gera menu de navegação */
    char nav_menu[512];
    build_nav_menu("/calibra", nav_menu, sizeof(nav_menu));

    const size_t page_capacity = 20480;
    char *page = malloc(page_capacity);
    if (!page) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro de memória");
        return ESP_FAIL;
    }

    int len = snprintf(page, page_capacity,
             "<!DOCTYPE html><html><head>"
             "<meta charset='utf-8'/>"
             "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
             "<title>Cultivo - greenSe Campo</title>"
             "<style>"
             "*{box-sizing:border-box}"
             "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,'Helvetica Neue',Arial,sans-serif;background:linear-gradient(180deg,#f0f7f2 0%%,#fafcfa 50%%,#ffffff 100%%);color:#1a2e1f;margin:0;padding:20px;line-height:1.6}"
             ".card{background:#ffffff;border-radius:20px;padding:32px;max-width:600px;margin:0 auto 24px;"
             "box-shadow:0 2px 8px rgba(0,0,0,0.04),0 8px 24px rgba(0,0,0,0.06);border:1px solid rgba(0,0,0,0.04);transition:transform 0.2s,box-shadow 0.2s}"
             ".card:hover{transform:translateY(-2px);box-shadow:0 4px 12px rgba(0,0,0,0.08),0 12px 32px rgba(0,0,0,0.1)}"
             ".tag{display:inline-block;padding:6px 14px;border-radius:20px;background:linear-gradient(135deg,#c8e6c9 0%%,#a5d6a7 100%%);"
             "color:#1b5e20;font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:0.5px}"
             "h2{margin:12px 0 12px;font-size:28px;font-weight:700;color:#2e7d32;letter-spacing:-0.5px}"
             "h3{margin:28px 0 16px;font-size:20px;font-weight:600;color:#388e3c;border-top:2px solid #f0f4f1;padding-top:20px;letter-spacing:-0.3px}"
             "h3:first-of-type{border-top:none;padding-top:0;margin-top:0}"
             ".lead{color:#5a6c5e;line-height:1.6;margin-bottom:24px;font-size:15px}"
             ".info{display:flex;gap:12px;margin:20px 0}"
             ".info-box{flex:1;background:linear-gradient(135deg,#fafbfa 0%%,#f5f7f6 100%%);border:1px solid #e8ede9;border-radius:14px;"
             "padding:16px;text-align:center;font-size:14px;transition:all 0.3s}"
             ".info-box:hover{transform:translateY(-2px);box-shadow:0 4px 12px rgba(0,0,0,0.08);border-color:#c8e6c9}"
             "form label{display:block;margin:16px 0 8px;font-weight:600;font-size:14px;color:#2e4a34}"
             ".tolerance-row{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:16px}"
             ".tolerance-row label{grid-column:1/-1;margin-bottom:8px}"
             ".tolerance-row input{width:100%%}"
             "input{width:100%%;padding:12px;border:1px solid #e8ede9;border-radius:10px;"
             "font-size:15px;box-sizing:border-box;transition:all 0.2s;background:#fafbfa}"
             "input:focus{outline:none;border-color:#4caf50;background:#fff;box-shadow:0 0 0 3px rgba(76,175,80,0.1)}"
             "select{width:100%%;padding:12px;border:1px solid #e8ede9;border-radius:10px;"
             "font-size:15px;box-sizing:border-box;background:#fafbfa;color:#1a2e1f;cursor:pointer;transition:all 0.2s}"
             "select:hover{border-color:#4caf50}"
             "select:focus{outline:none;border-color:#4caf50;background:#fff;box-shadow:0 0 0 3px rgba(76,175,80,0.1)}"
             ".preset-box{background:linear-gradient(135deg,#fafbfa 0%%,#f5f7f6 100%%);border:1px solid #e8ede9;border-radius:16px;"
             "padding:20px;margin-bottom:24px;transition:all 0.3s}"
             ".preset-box:hover{border-color:#c8e6c9;box-shadow:0 2px 8px rgba(0,0,0,0.06)}"
             ".preset-label{display:block;margin-bottom:12px;font-weight:600;font-size:14px;color:#2e4a34}"
             "button{width:100%%;padding:14px 24px;border:none;border-radius:10px;background:linear-gradient(135deg,#4caf50 0%%,#388e3c 100%%);"
             "color:#fff;font-size:15px;font-weight:600;margin-top:24px;box-shadow:0 4px 12px rgba(76,175,80,0.3);transition:all 0.3s;cursor:pointer}"
             "button:hover{transform:translateY(-2px);box-shadow:0 6px 16px rgba(76,175,80,0.4)}"
             "button:active{transform:translateY(0)}"
             "a.button{display:inline-block;width:100%%;text-align:center;padding:14px 24px;"
             "border-radius:10px;background:linear-gradient(135deg,#4caf50 0%%,#388e3c 100%%);color:#fff;text-decoration:none;font-weight:600;"
             "margin-top:16px;box-shadow:0 4px 12px rgba(76,175,80,0.3);box-sizing:border-box;transition:all 0.3s}"
             "a.button:hover{transform:translateY(-2px);box-shadow:0 6px 16px rgba(76,175,80,0.4)}"
             ".tip{font-size:13px;color:#6b7c6f;margin-top:16px;font-style:italic;line-height:1.6}"
             ".preset-badge{display:inline-flex;align-items:center;gap:6px;background:linear-gradient(135deg,#fff9c4 0%%,#fff59d 100%%);"
             "color:#7d6608;font-size:11px;font-weight:600;border-radius:20px;padding:6px 14px;margin-left:10px;"
             "border:1px solid #ffd54f;box-shadow:0 2px 4px rgba(255,193,7,0.2)}"
             ".main-nav{background:#ffffff;border-radius:16px;padding:12px;margin-bottom:24px;box-shadow:0 2px 8px rgba(0,0,0,0.06),0 4px 16px rgba(0,0,0,0.04);border:1px solid rgba(0,0,0,0.04);max-width:600px;margin-left:auto;margin-right:auto}"
             ".nav-container{display:flex;gap:6px;flex-wrap:wrap;justify-content:center}"
             ".nav-item{padding:10px 20px;border-radius:10px;text-decoration:none;font-size:14px;font-weight:500;color:#6b7c6f;transition:all 0.3s;background:transparent;position:relative}"
             ".nav-item:hover{background:#f1f8e9;color:#2e7d32;transform:translateY(-2px)}"
             ".nav-item.active{background:linear-gradient(135deg,#4caf50 0%%,#388e3c 100%%);color:#fff;font-weight:600;box-shadow:0 4px 12px rgba(76,175,80,0.3)}"
             ".footer-note{margin-top:32px;font-size:12px;color:#8a9b8d;font-style:italic;text-align:center}"
             "</style></head><body>"
             "<div style='max-width:600px;margin:0 auto 20px'>%s</div>"
             "<div style='text-align:center;margin-bottom:20px'>%s</div>"
             "<div class='card'>"
             "<div style='display:flex;align-items:center;flex-wrap:wrap;gap:8px;margin-bottom:8px'>"
             "<span class='tag'>greenSe Campo</span>"
             "<div class='preset-badge'>%s</div>"
             "</div>"
             "<h2>Cultivo</h2>"
             "<p class='lead'>Defina os valores ideais de temperatura, umidade e luz para sua planta. "
             "Esses valores aparecem como linhas nos gr&aacute;ficos para voc&ecirc; ver quando est&aacute; bom ou precisa ajustar.</p>"
             
             "<form action='/set_tolerance' method='get' id='toleranceForm'>"
             "<h3>Valores Ideais</h3>"
             
             "<div class='preset-box'>"
             "<label class='preset-label' for='presetSelect'>Escolha um tipo de planta:</label>"
             "<select id='presetSelect' onchange='applyPreset()'>"
             "<option value=''>Carregando presets...</option>"
             "</select>"
             "<p class='tip' style='margin-top:8px'>Ao escolher um tipo, os valores abaixo s&atilde;o preenchidos automaticamente. Voc&ecirc; pode ajustar depois se precisar.</p>"
             "</div>"
             
             "<div class='tolerance-row'>"
             "<label>Temperatura do Ar (°C)</label>"
             "<input type='number' step='0.1' name='temp_ar_min' value='%.1f' placeholder='Mínimo'>"
             "<input type='number' step='0.1' name='temp_ar_max' value='%.1f' placeholder='Máximo'>"
             "</div>"
             
             "<div class='tolerance-row'>"
             "<label>Umidade do Ar (%%)</label>"
             "<input type='number' step='0.1' name='umid_ar_min' value='%.1f' placeholder='Mínimo'>"
             "<input type='number' step='0.1' name='umid_ar_max' value='%.1f' placeholder='Máximo'>"
             "</div>"
             
             "<div class='tolerance-row'>"
             "<label>Temperatura do Solo (°C)</label>"
             "<input type='number' step='0.1' name='temp_solo_min' value='%.1f' placeholder='Mínimo'>"
             "<input type='number' step='0.1' name='temp_solo_max' value='%.1f' placeholder='Máximo'>"
             "</div>"
             
             "<div class='tolerance-row'>"
             "<label>Umidade do Solo (%%)</label>"
             "<input type='number' step='0.1' name='umid_solo_min' value='%.1f' placeholder='Mínimo'>"
             "<input type='number' step='0.1' name='umid_solo_max' value='%.1f' placeholder='Máximo'>"
             "</div>"
             
             "<div class='tolerance-row'>"
             "<label>Luminosidade (lux)</label>"
             "<input type='number' step='1' name='luminosidade_min' value='%.0f' placeholder='Mínimo'>"
             "<input type='number' step='1' name='luminosidade_max' value='%.0f' placeholder='Máximo'>"
             "</div>"
             
             "<div class='tolerance-row'>"
             "<label>DPV (kPa)</label>"
             "<input type='number' step='0.1' name='dpv_min' value='%.1f' placeholder='Mínimo'>"
             "<input type='number' step='0.1' name='dpv_max' value='%.1f' placeholder='Máximo'>"
             "</div>"
             
             "<button type='submit'>Salvar Preset</button>"
             "</form>"
             "<form action='/reset_tolerance' method='get' style='margin-top:12px'>"
             "<button type='submit' style='background:#ff9800;box-shadow:0 10px 24px rgba(255,152,0,0.25)'>Voltar ao Padrão</button>"
             "</form>"
             "<p class='tip' style='margin-top:8px'><strong>Voltar ao padrão:</strong> Restaura os valores iniciais recomendados para a maioria dos cultivos. "
             "Use quando quiser come&ccedil;ar de novo.</p>"
             
             "<form id='uploadPresetsForm' enctype='multipart/form-data' style='margin-top:16px'>"
             "<label style='display:block;margin-bottom:8px;font-weight:600;font-size:14px;color:#2e4a34'>Carregar Presets</label>"
             "<input type='file' id='presetsFile' name='presets' accept='.json' style='width:100%%;padding:8px;border:1px solid #e8ede9;border-radius:8px;margin-bottom:8px'>"
             "<button type='submit' style='width:100%%;padding:12px;border:none;border-radius:10px;background:linear-gradient(135deg,#42a5f5 0%%,#1976d2 100%%);color:#fff;font-weight:600;cursor:pointer;box-shadow:0 4px 12px rgba(25,118,210,0.3)'>Enviar Arquivo de Presets</button>"
             "</form>"
             "<p class='tip' style='margin-top:8px'><strong>Carregar Presets:</strong> Faça upload de um arquivo JSON com presets personalizados. "
             "O arquivo deve seguir o formato do exemplo fornecido.</p>"
             
             "<h3>Ajustar Sensor de Solo</h3>"
             "<p class='lead'>Ajuste os valores para quando o solo est&aacute; seco e quando est&aacute; molhado no seu canteiro. "
             "Isso faz o sensor mostrar a umidade correta do seu solo.</p>"
             "<div class='info'>"
             "<div class='info-box'><strong>Valor atual</strong><br>%d</div>"
             "<div class='info-box'><strong>Configurado</strong><br>Seco %.0f | Molhado %.0f</div>"
             "</div>"
             "<form action='/set_calibra' method='get'>"
             "<label>Valor quando solo est&aacute; seco</label>"
             "<input type='number' name='seco' value='%.0f'>"
             "<label>Valor quando solo est&aacute; molhado</label>"
             "<input type='number' name='molhado' value='%.0f'>"
             "<button type='submit'>Salvar Calibração</button>"
             "</form>"
             "<p class='tip'><strong>Dica:</strong> Para ajustar bem, anote o valor logo depois de regar (solo molhado) "
             "e depois de alguns dias sem regar (solo seco). Assim o sensor vai funcionar melhor no seu solo.</p>"
             "<a class='button' href='/'>Voltar</a>"
             "</div>"
             "<script>"
             "let presets = {};"
             "let presetsList = [];"
             ""
             "async function loadPresets() {"
             "  try {"
             "    const response = await fetch('/presets.json');"
             "    const data = await response.json();"
             "    presets = {};"
             "    presetsList = data.presets || [];"
             "    "
             "    presetsList.forEach(p => {"
             "      presets[p.id] = {"
             "        temp_ar_min: p.temp_ar_min, temp_ar_max: p.temp_ar_max,"
             "        umid_ar_min: p.umid_ar_min, umid_ar_max: p.umid_ar_max,"
             "        temp_solo_min: p.temp_solo_min, temp_solo_max: p.temp_solo_max,"
             "        umid_solo_min: p.umid_solo_min, umid_solo_max: p.umid_solo_max,"
             "        luminosidade_min: p.luminosidade_min, luminosidade_max: p.luminosidade_max,"
             "        dpv_min: p.dpv_min, dpv_max: p.dpv_max"
             "      };"
             "    });"
             "    "
             "    const select = document.getElementById('presetSelect');"
             "    select.innerHTML = '<option value=\"\">-- Escolha um preset --</option>';"
             "    presetsList.forEach(p => {"
             "      const option = document.createElement('option');"
             "      option.value = p.id;"
             "      option.textContent = p.name;"
             "      select.appendChild(option);"
             "    });"
             "  } catch(e) {"
             "    console.error('Erro ao carregar presets:', e);"
             "  }"
             "}"
             ""
             "function applyPreset() {"
             "  const select = document.getElementById('presetSelect');"
             "  const preset = select.value;"
             "  if (!preset || !presets[preset]) return;"
             "  const values = presets[preset];"
             "  document.querySelector('input[name=\"temp_ar_min\"]').value = values.temp_ar_min;"
             "  document.querySelector('input[name=\"temp_ar_max\"]').value = values.temp_ar_max;"
             "  document.querySelector('input[name=\"umid_ar_min\"]').value = values.umid_ar_min;"
             "  document.querySelector('input[name=\"umid_ar_max\"]').value = values.umid_ar_max;"
             "  document.querySelector('input[name=\"temp_solo_min\"]').value = values.temp_solo_min;"
             "  document.querySelector('input[name=\"temp_solo_max\"]').value = values.temp_solo_max;"
             "  document.querySelector('input[name=\"umid_solo_min\"]').value = values.umid_solo_min;"
             "  document.querySelector('input[name=\"umid_solo_max\"]').value = values.umid_solo_max;"
             "  document.querySelector('input[name=\"luminosidade_min\"]').value = values.luminosidade_min;"
             "  document.querySelector('input[name=\"luminosidade_max\"]').value = values.luminosidade_max;"
             "  document.querySelector('input[name=\"dpv_min\"]').value = parseFloat(values.dpv_min).toFixed(1);"
             "  document.querySelector('input[name=\"dpv_max\"]').value = parseFloat(values.dpv_max).toFixed(1);"
             "}"
             ""
             "document.getElementById('uploadPresetsForm').addEventListener('submit', async function(e) {"
             "  e.preventDefault();"
             "  const fileInput = document.getElementById('presetsFile');"
             "  if (!fileInput.files || !fileInput.files[0]) {"
             "    alert('Por favor, selecione um arquivo JSON');"
             "    return;"
             "  }"
             "  "
             "  const formData = new FormData();"
             "  formData.append('presets', fileInput.files[0]);"
             "  "
             "  try {"
             "    const response = await fetch('/upload_presets', {"
             "      method: 'POST',"
             "      body: formData"
             "    });"
             "    "
             "    if (response.ok) {"
             "      alert('Presets carregados com sucesso! Recarregando página...');"
             "      await loadPresets();"
             "      location.reload();"
             "    } else {"
             "      const error = await response.text();"
             "      alert('Erro ao carregar presets: ' + error);"
             "    }"
             "  } catch(e) {"
             "    alert('Erro ao enviar arquivo: ' + e.message);"
             "  }"
             "});"
             ""
             "loadPresets();"
             "</script>"
             "<p class='footer-note'>greenSe Campo | Tecnologia desenhada para agricultura conectada.</p>"
             "</body></html>",
             nav_menu,
             greense_logo_svg,
             preset_text,
             temp_ar_min, temp_ar_max,
             umid_ar_min, umid_ar_max,
             temp_solo_min, temp_solo_max,
             umid_solo_min, umid_solo_max,
             luminosidade_min, luminosidade_max,
             dpv_min, dpv_max,
             leitura_raw,
             seco, molhado,
             seco, molhado);
 
    if (len < 0 || len >= (int)page_capacity) {
        free(page);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro gerando página");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    esp_err_t resp = httpd_resp_send(req, page, len);
    free(page);
    return resp;
}

/* /set_tolerance -> salva tolerâncias de cultivo */
static esp_err_t handle_set_tolerance(httpd_req_t *req)
{
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL || svc->set_cultivation_tolerance == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Serviços não disponíveis");
        return ESP_FAIL;
    }

    char qs[512];
    if (httpd_req_get_url_query_str(req, qs, sizeof(qs)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Parâmetros ausentes");
        return ESP_FAIL;
    }

    char buf[32];
    float temp_ar_min = 20.0f, temp_ar_max = 30.0f;
    float umid_ar_min = 50.0f, umid_ar_max = 80.0f;
    float temp_solo_min = 18.0f, temp_solo_max = 25.0f;
    float umid_solo_min = 40.0f, umid_solo_max = 80.0f;
    float luminosidade_min = 500.0f, luminosidade_max = 2000.0f;
    float dpv_min = 0.5f, dpv_max = 2.0f;

    if (httpd_query_key_value(qs, "temp_ar_min", buf, sizeof(buf)) == ESP_OK) temp_ar_min = atof(buf);
    if (httpd_query_key_value(qs, "temp_ar_max", buf, sizeof(buf)) == ESP_OK) temp_ar_max = atof(buf);
    if (httpd_query_key_value(qs, "umid_ar_min", buf, sizeof(buf)) == ESP_OK) umid_ar_min = atof(buf);
    if (httpd_query_key_value(qs, "umid_ar_max", buf, sizeof(buf)) == ESP_OK) umid_ar_max = atof(buf);
    if (httpd_query_key_value(qs, "temp_solo_min", buf, sizeof(buf)) == ESP_OK) temp_solo_min = atof(buf);
    if (httpd_query_key_value(qs, "temp_solo_max", buf, sizeof(buf)) == ESP_OK) temp_solo_max = atof(buf);
    if (httpd_query_key_value(qs, "umid_solo_min", buf, sizeof(buf)) == ESP_OK) umid_solo_min = atof(buf);
    if (httpd_query_key_value(qs, "umid_solo_max", buf, sizeof(buf)) == ESP_OK) umid_solo_max = atof(buf);
    if (httpd_query_key_value(qs, "luminosidade_min", buf, sizeof(buf)) == ESP_OK) luminosidade_min = atof(buf);
    if (httpd_query_key_value(qs, "luminosidade_max", buf, sizeof(buf)) == ESP_OK) luminosidade_max = atof(buf);
    if (httpd_query_key_value(qs, "dpv_min", buf, sizeof(buf)) == ESP_OK) dpv_min = atof(buf);
    if (httpd_query_key_value(qs, "dpv_max", buf, sizeof(buf)) == ESP_OK) dpv_max = atof(buf);

    if (svc->set_cultivation_tolerance(temp_ar_min, temp_ar_max,
                                      umid_ar_min, umid_ar_max,
                                      temp_solo_min, temp_solo_max,
                                      umid_solo_min, umid_solo_max,
                                      luminosidade_min, luminosidade_max,
                                      dpv_min, dpv_max) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Valores inválidos");
        return ESP_FAIL;
    }

    const char *ok_page =
        "<!DOCTYPE html><html><head><meta charset='utf-8'/><meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<title>Configuração Salva</title></head><body style='font-family:sans-serif;padding:20px'>"
        "<h2>Faixas ideais salvas com sucesso!</h2>"
        "<p><a href='/calibra'>Voltar</a> | <a href='/'>Monitoramento</a></p>"
        "</body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, ok_page, HTTPD_RESP_USE_STRLEN);
}

/* /reset_tolerance -> restaura valores padrão de tolerância */
static esp_err_t handle_reset_tolerance(httpd_req_t *req)
{
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL || svc->set_cultivation_tolerance == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Serviços não disponíveis");
        return ESP_FAIL;
    }

    // Remove arquivo de presets customizado para voltar aos presets de fábrica
    if (remove(PRESETS_FILE_PATH) == 0) {
        ESP_LOGI(TAG, "Arquivo de presets customizado removido: %s", PRESETS_FILE_PATH);
    } else {
        // Arquivo pode não existir, não é erro crítico
        ESP_LOGD(TAG, "Arquivo de presets não encontrado ou já removido: %s", PRESETS_FILE_PATH);
    }

    // Valores padrão
    float temp_ar_min = 20.0f, temp_ar_max = 30.0f;
    float umid_ar_min = 50.0f, umid_ar_max = 80.0f;
    float temp_solo_min = 18.0f, temp_solo_max = 25.0f;
    float umid_solo_min = 40.0f, umid_solo_max = 80.0f;
    float luminosidade_min = 500.0f, luminosidade_max = 2000.0f;
    float dpv_min = 0.5f, dpv_max = 2.0f;

    if (svc->set_cultivation_tolerance(temp_ar_min, temp_ar_max,
                                      umid_ar_min, umid_ar_max,
                                      temp_solo_min, temp_solo_max,
                                      umid_solo_min, umid_solo_max,
                                      luminosidade_min, luminosidade_max,
                                      dpv_min, dpv_max) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro ao restaurar valores padrão");
        return ESP_FAIL;
    }

    const char *ok_page =
        "<!DOCTYPE html><html><head><meta charset='utf-8'/><meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<title>Valores Restaurados</title></head><body style='font-family:sans-serif;padding:20px'>"
        "<h2>Valores padrão restaurados com sucesso!</h2>"
        "<p>As faixas ideais e os presets foram restaurados para os valores de fábrica:</p>"
        "<ul>"
        "<li>Temp. do ar: 20-30&deg;C</li>"
        "<li>Umidade do ar: 50-80%%</li>"
        "<li>Temp. do solo: 18-25&deg;C</li>"
        "<li>Umidade do solo: 40-80%%</li>"
        "<li>Luminosidade: 500-2000 lux</li>"
        "<li>DPV: 0.5-2.0 kPa</li>"
        "</ul>"
        "<p><a href='/calibra'>Voltar às configurações</a> | <a href='/'>Monitoramento</a></p>"
        "</body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, ok_page, HTTPD_RESP_USE_STRLEN);
}

/* /set_calibra -> salva calibração e redireciona */
static esp_err_t handle_set_calibra(httpd_req_t *req)
{
    /* pega query string */
    char qs[128];
    int len = httpd_req_get_url_query_str(req, qs, sizeof(qs));
    if (len < 0)
    {
        httpd_resp_send_err(req,
                            HTTPD_400_BAD_REQUEST,
                            "sem query");
        return ESP_FAIL;
    }
 
    char buf_seco[32];
    char buf_molhado[32];
    if (httpd_query_key_value(qs, "seco",
                              buf_seco, sizeof(buf_seco)) != ESP_OK ||
        httpd_query_key_value(qs, "molhado",
                              buf_molhado, sizeof(buf_molhado)) != ESP_OK)
    {
        httpd_resp_send_err(req,
                            HTTPD_400_BAD_REQUEST,
                            "parametros faltando");
        return ESP_FAIL;
    }
 
    float novo_seco    = atof(buf_seco);
    float novo_molhado = atof(buf_molhado);
 
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Serviços não disponíveis");
        return ESP_FAIL;
    }
    
    if (svc->set_calibration(novo_seco, novo_molhado) != ESP_OK)
    {
        httpd_resp_send_err(req,
                            HTTPD_500_INTERNAL_SERVER_ERROR,
                            "falha salvando calib");
        return ESP_FAIL;
    }
 
    /* resposta simples tipo redirect manual */
    const char *ok_page =
        "<!DOCTYPE html><html><head><meta charset='utf-8'/><meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<title>Calibração Salva</title></head><body style='font-family:sans-serif;padding:20px'>"
        "<h2>Calibração salva com sucesso!</h2>"
        "<p>Os valores de calibra&ccedil;&atilde;o do sensor de umidade do solo foram atualizados. "
        "As leituras agora refletem com maior precis&atilde;o as condi&ccedil;&otilde;es reais do seu canteiro.</p>"
        "<p><a href='/calibra'>Voltar às configurações</a> | <a href='/'>Monitoramento</a></p>"
        "</body></html>";
 
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, ok_page, HTTPD_RESP_USE_STRLEN);
}

/* responde favicon.ico para parar 404 */
static esp_err_t handle_favicon(httpd_req_t *req)
{
    static const unsigned char favicon_png[] = {
        0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,
        0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x06,0x00,0x00,0x00,0x1F,0x15,0xC4,
        0x89,0x00,0x00,0x00,0x0A,0x49,0x44,0x41,
        0x54,0x78,0x9C,0x63,0x60,0x00,0x02,0x00,
        0x00,0x05,0x00,0x01,0x0D,0x0A,0x2D,0xB4,
        0x00,0x00,0x00,0x00,0x49,0x45,0x4E,0x44,
        0xAE,0x42,0x60,0x82
    };
 
    httpd_resp_set_type(req, "image/png");
    return httpd_resp_send(req, (const char *)favicon_png, sizeof(favicon_png));
}

/* atende /generate_204 (Android captive portal check) */
static esp_err_t handle_generate204(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, NULL, 0);
}

/* -------------------------------------------------------------------------- */
/* Gerenciamento de Presets via Arquivo                                      */
/* -------------------------------------------------------------------------- */

/* Gera JSON dos presets hardcoded (fallback) */
static char* generate_default_presets_json(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *presets_array = cJSON_CreateArray();
    
    for (size_t i = 0; i < PRESET_COUNT; i++) {
        cJSON *preset = cJSON_CreateObject();
        cJSON_AddStringToObject(preset, "id", (i == 0) ? "padrao" : 
            (i == 1) ? "tomate" : (i == 2) ? "morango" : (i == 3) ? "alface" : "rucula");
        cJSON_AddStringToObject(preset, "name", presets[i].name);
        cJSON_AddNumberToObject(preset, "temp_ar_min", presets[i].temp_ar_min);
        cJSON_AddNumberToObject(preset, "temp_ar_max", presets[i].temp_ar_max);
        cJSON_AddNumberToObject(preset, "umid_ar_min", presets[i].umid_ar_min);
        cJSON_AddNumberToObject(preset, "umid_ar_max", presets[i].umid_ar_max);
        cJSON_AddNumberToObject(preset, "temp_solo_min", presets[i].temp_solo_min);
        cJSON_AddNumberToObject(preset, "temp_solo_max", presets[i].temp_solo_max);
        cJSON_AddNumberToObject(preset, "umid_solo_min", presets[i].umid_solo_min);
        cJSON_AddNumberToObject(preset, "umid_solo_max", presets[i].umid_solo_max);
        cJSON_AddNumberToObject(preset, "luminosidade_min", presets[i].luminosidade_min);
        cJSON_AddNumberToObject(preset, "luminosidade_max", presets[i].luminosidade_max);
        cJSON_AddNumberToObject(preset, "dpv_min", presets[i].dpv_min);
        cJSON_AddNumberToObject(preset, "dpv_max", presets[i].dpv_max);
        cJSON_AddItemToArray(presets_array, preset);
    }
    
    cJSON_AddItemToObject(root, "presets", presets_array);
    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

/* Handler GET /presets.json - Retorna presets em JSON */
static esp_err_t handle_get_presets_json(httpd_req_t *req)
{
    FILE *f = fopen(PRESETS_FILE_PATH, "r");
    char *json_response = NULL;
    
    if (f) {
        // Arquivo existe, lê e retorna
        char buf[4096];
        size_t len = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        buf[len] = '\0';
        
        // Valida JSON
        cJSON *test = cJSON_Parse(buf);
        if (test) {
            cJSON_Delete(test);
            json_response = malloc(len + 1);
            if (json_response) {
                memcpy(json_response, buf, len);
                json_response[len] = '\0';
            }
        } else {
            // JSON inválido, usa fallback
            json_response = generate_default_presets_json();
        }
    } else {
        // Arquivo não existe, retorna presets hardcoded
        json_response = generate_default_presets_json();
    }
    
    if (!json_response) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro ao gerar JSON");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    esp_err_t ret = httpd_resp_send(req, json_response, strlen(json_response));
    free(json_response);
    return ret;
}

/* Handler POST /upload_presets - Recebe arquivo JSON e salva */
static esp_err_t handle_upload_presets(httpd_req_t *req)
{
    if (req->content_len > 4096) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Arquivo muito grande (máx 4KB)");
        return ESP_FAIL;
    }
    
    char *buf = malloc(req->content_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro de memória");
        return ESP_FAIL;
    }
    
    int received = 0;
    int remaining = req->content_len;
    
    while (remaining > 0) {
        int to_read = remaining < 1024 ? remaining : 1024;
        int ret = httpd_req_recv(req, buf + received, to_read);
        if (ret <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro ao receber dados");
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    buf[received] = '\0';
    
    // Extrai JSON do multipart/form-data (busca por boundary e JSON)
    char *json_start = strstr(buf, "{");
    char *json_end = strrchr(buf, '}');
    
    if (!json_start || !json_end || json_end <= json_start) {
        // Tenta como JSON direto
        json_start = buf;
        json_end = buf + received;
    } else {
        json_end++;
    }
    
    size_t json_len = json_end - json_start;
    char *json_content = malloc(json_len + 1);
    if (!json_content) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro de memória");
        return ESP_FAIL;
    }
    memcpy(json_content, json_start, json_len);
    json_content[json_len] = '\0';
    
    // Valida JSON
    cJSON *root = cJSON_Parse(json_content);
    if (!root) {
        free(buf);
        free(json_content);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON inválido");
        return ESP_FAIL;
    }
    
    // Valida estrutura básica
    cJSON *presets_array = cJSON_GetObjectItem(root, "presets");
    if (!cJSON_IsArray(presets_array) || cJSON_GetArraySize(presets_array) == 0) {
        cJSON_Delete(root);
        free(buf);
        free(json_content);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON deve conter array 'presets' não vazio");
        return ESP_FAIL;
    }
    
    // Valida cada preset
    int array_size = cJSON_GetArraySize(presets_array);
    for (int i = 0; i < array_size; i++) {
        cJSON *preset = cJSON_GetArrayItem(presets_array, i);
        cJSON *temp_ar_min = cJSON_GetObjectItem(preset, "temp_ar_min");
        cJSON *temp_ar_max = cJSON_GetObjectItem(preset, "temp_ar_max");
        
        if (!cJSON_IsNumber(temp_ar_min) || !cJSON_IsNumber(temp_ar_max)) {
            cJSON_Delete(root);
            free(buf);
            free(json_content);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Preset inválido: valores numéricos obrigatórios");
            return ESP_FAIL;
        }
        
        // Valida min < max
        if (temp_ar_min->valuedouble >= temp_ar_max->valuedouble) {
            cJSON_Delete(root);
            free(buf);
            free(json_content);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Preset inválido: min deve ser < max");
            return ESP_FAIL;
        }
    }
    
    cJSON_Delete(root);
    
    // Salva arquivo
    FILE *f = fopen(PRESETS_FILE_PATH, "w");
    if (!f) {
        free(buf);
        free(json_content);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro ao criar arquivo");
        return ESP_FAIL;
    }
    
    size_t written = fwrite(json_content, 1, json_len, f);
    fclose(f);
    
    free(buf);
    free(json_content);
    
    if (written != json_len) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro ao escrever arquivo");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Presets atualizados via upload (%d bytes)", (int)written);
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* Endpoint para limpar dados via GUI */
static esp_err_t handle_clear_data(httpd_req_t *req)
{
    const gui_services_t *svc = gui_services_get();
    if (svc == NULL || svc->clear_logged_data == NULL) {
        httpd_resp_send_err(req,
                            HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Serviço indisponível");
        return ESP_FAIL;
    }

    /* descarta payload (se houver) */
    int remaining = req->content_len;
    char buf[64];
    while (remaining > 0) {
        int to_read = remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf);
        int received = httpd_req_recv(req, buf, to_read);
        if (received <= 0) {
            httpd_resp_send_err(req,
                                HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Falha ao receber requisição");
            return ESP_FAIL;
        }
        remaining -= received;
    }

    if (svc->clear_logged_data() != ESP_OK) {
        httpd_resp_send_err(req,
                            HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Não foi possível apagar os dados");
        return ESP_FAIL;
    }

    const char *resp =
        "<!DOCTYPE html><html><head><meta charset='utf-8'/>"
        "<meta http-equiv='refresh' content='2; url=/config'/>"
        "<title>Dados apagados</title></head>"
        "<body><p>Dados apagados com sucesso. Você será redirecionado "
        "para a página de configurações.</p></body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}


 
/* -------------------------------------------------------------------------- */
/* Inicialização/parada do servidor HTTP                                      */
/* -------------------------------------------------------------------------- */
 
esp_err_t http_server_start(void)
{
    if (server_handle != NULL) {
        return ESP_OK;
    }
 
    /* Inicializa Wi-Fi AP sem callbacks (serão registrados por app_main) */
    ESP_ERROR_CHECK(wifi_ap_init());
    
    // Aguarda a interface de rede estar pronta antes de inicializar mDNS
    // Isso garante que o DHCP server já tenha atribuído o IP
    vTaskDelay(pdMS_TO_TICKS(2000)); // Aguarda 2 segundos para rede estabilizar
    
    // Inicializa mDNS para acesso via nome de domínio
    esp_err_t mdns_ret = mdns_init();
    if (mdns_ret != ESP_OK) {
        ESP_LOGW(TAG, "Falha ao inicializar mDNS (código: %d), tentando novamente...", mdns_ret);
        vTaskDelay(pdMS_TO_TICKS(1000));
        mdns_ret = mdns_init();
        if (mdns_ret != ESP_OK) {
            ESP_LOGE(TAG, "Falha crítica ao inicializar mDNS após retry");
            // Continua mesmo assim, mas sem mDNS
        }
    }
    
    if (mdns_ret == ESP_OK) {
        // Obtém a interface de rede do AP
        esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        if (ap_netif != NULL) {
            // Registra a interface de rede no mDNS
            esp_err_t netif_ret = mdns_register_netif(ap_netif);
            if (netif_ret != ESP_OK) {
                ESP_LOGW(TAG, "Falha ao registrar interface de rede no mDNS (código: %d)", netif_ret);
            }
        } else {
            ESP_LOGW(TAG, "Interface de rede AP não encontrada");
        }
        
        // Define o hostname (nome do dispositivo na rede)
        mdns_ret = mdns_hostname_set("greense");
        if (mdns_ret != ESP_OK) {
            ESP_LOGW(TAG, "Falha ao definir hostname mDNS, tentando novamente...");
            vTaskDelay(pdMS_TO_TICKS(500));
            mdns_ret = mdns_hostname_set("greense");
        }
        
        if (mdns_ret == ESP_OK) {
            // Define o nome do serviço
            ESP_ERROR_CHECK(mdns_instance_name_set("greenSe Campo Sensor"));
            
            ESP_LOGI(TAG, "mDNS inicializado: greense.local");
        } else {
            ESP_LOGE(TAG, "Falha ao configurar hostname mDNS");
        }
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    uint16_t http_port = config.server_port;  // Guarda a porta para usar no mDNS depois

    /* Robustez - stack maior para evitar overflow */
    config.stack_size        = 16384;  /* aumentado para evitar stack overflow */
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    config.max_open_sockets  = 4;
    config.lru_purge_enable  = true;
    config.max_uri_handlers  = 16;    /* garante espaço para todos os handlers (atual: 14) */
 
    if (httpd_start(&server_handle, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao iniciar httpd");
        return ESP_FAIL;
    }
 
    /* registrar rotas principais */
    httpd_uri_t uri_root = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = handle_dashboard,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_root) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /");
        return ESP_FAIL;
    }

    httpd_uri_t uri_hist = {
        .uri      = "/history",
        .method   = HTTP_GET,
        .handler  = handle_history,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_hist) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /history");
        return ESP_FAIL;
    }

    httpd_uri_t uri_config = {
        .uri      = "/config",
        .method   = HTTP_GET,
        .handler  = handle_config,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_config) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /config");
        return ESP_FAIL;
    }

    httpd_uri_t uri_sampling = {
        .uri      = "/sampling",
        .method   = HTTP_GET,
        .handler  = handle_sampling_page,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_sampling) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /sampling");
        return ESP_FAIL;
    }

    httpd_uri_t uri_sampling_set = {
        .uri      = "/set_sampling",
        .method   = HTTP_GET,
        .handler  = handle_set_sampling,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_sampling_set) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /set_sampling");
        return ESP_FAIL;
    }

    httpd_uri_t uri_down = {
        .uri      = "/download",
        .method   = HTTP_GET,
        .handler  = handle_download,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_down) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /download");
        return ESP_FAIL;
    }

    httpd_uri_t uri_cal = {
        .uri      = "/calibra",
        .method   = HTTP_GET,
        .handler  = handle_calibra,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_cal) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /calibra");
        return ESP_FAIL;
    }

    httpd_uri_t uri_tolerance = {
        .uri      = "/set_tolerance",
        .method   = HTTP_GET,
        .handler  = handle_set_tolerance,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_tolerance) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /set_tolerance");
        return ESP_FAIL;
    }

    httpd_uri_t uri_reset_tolerance = {
        .uri      = "/reset_tolerance",
        .method   = HTTP_GET,
        .handler  = handle_reset_tolerance,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_reset_tolerance) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /reset_tolerance");
        return ESP_FAIL;
    }

    httpd_uri_t uri_set = {
        .uri      = "/set_calibra",
        .method   = HTTP_GET,
        .handler  = handle_set_calibra,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_set) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /set_calibra");
        return ESP_FAIL;
    }

    httpd_uri_t uri_clear = {
        .uri      = "/clear_data",
        .method   = HTTP_POST,
        .handler  = handle_clear_data,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_clear) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /clear_data");
        return ESP_FAIL;
    }

    /* favicon padrao */
    httpd_uri_t uri_icon = {
        .uri      = "/favicon.ico",
        .method   = HTTP_GET,
        .handler  = handle_favicon,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_icon) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /favicon.ico");
        return ESP_FAIL;
    }

    /* rota de teste captive portal Android */
    httpd_uri_t uri_gen204 = {
        .uri      = "/generate_204",
        .method   = HTTP_GET,
        .handler  = handle_generate204,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_gen204) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /generate_204");
        return ESP_FAIL;
    }

    httpd_uri_t uri_presets_json = {
        .uri      = "/presets.json",
        .method   = HTTP_GET,
        .handler  = handle_get_presets_json,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_presets_json) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /presets.json");
        return ESP_FAIL;
    }

    httpd_uri_t uri_upload_presets = {
        .uri      = "/upload_presets",
        .method   = HTTP_POST,
        .handler  = handle_upload_presets,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_upload_presets) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /upload_presets");
        return ESP_FAIL;
    }
 
    // Registra o serviço HTTP no mDNS (porta já obtida acima)
    if (mdns_ret == ESP_OK) {
        mdns_txt_item_t serviceTxtData[] = {
            {"path", "/"},
            {"version", "1.0"}
        };
        esp_err_t service_ret = mdns_service_add("greenSe Campo", "_http", "_tcp", http_port, serviceTxtData, 2);
        if (service_ret != ESP_OK) {
            ESP_LOGW(TAG, "Falha ao registrar serviço HTTP no mDNS (código: %d), tentando novamente...", service_ret);
            vTaskDelay(pdMS_TO_TICKS(1000));
            service_ret = mdns_service_add("greenSe Campo", "_http", "_tcp", http_port, serviceTxtData, 2);
            if (service_ret == ESP_OK) {
                ESP_LOGI(TAG, "Serviço HTTP registrado no mDNS após retry");
            } else {
                ESP_LOGW(TAG, "Falha ao registrar serviço HTTP no mDNS após retry (código: %d)", service_ret);
            }
        } else {
            ESP_LOGI(TAG, "Serviço HTTP registrado no mDNS");
        }
    } else {
        ESP_LOGW(TAG, "mDNS não disponível, serviço HTTP não registrado");
    }
    
    ESP_LOGI(TAG,
             "Servidor HTTP iniciado e rotas registradas. Acesse http://greense.local/ ou http://192.168.4.1/");
    return ESP_OK;
}
 
esp_err_t http_server_stop(void)
{
    if (server_handle)
    {
        httpd_stop(server_handle);
        server_handle = NULL;
        return ESP_OK;
    }
    return ESP_FAIL;
}

