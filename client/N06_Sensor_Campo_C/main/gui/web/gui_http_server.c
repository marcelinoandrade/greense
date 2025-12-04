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
#include "../../bsp/network/bsp_wifi_ap.h"
#include "../../app/gui_services.h"
#include "../../bsp/board.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_http_server.h"

static const char *TAG = "GUI_HTTP_SERVER";

static httpd_handle_t server_handle = NULL;

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
/* Handlers HTTP                                                              */
/* -------------------------------------------------------------------------- */

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
                 "An&aacute;lise baseada nas &uacute;ltimas %d amostras coletadas (per&iacute;odo aproximado: %s). "
                 "Frequ&ecirc;ncia de amostragem atual: %s.",
                 window_samples,
                 window_span_text,
                 sampling_period_text);
    } else {
        snprintf(stats_info_text,
                 sizeof(stats_info_text),
                 "Aguardando leituras suficientes para calcular estat&iacute;sticas. "
                 "As estat&iacute;sticas ficar&atilde;o dispon&iacute;veis assim que houver amostras registradas.");
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
                     "%d amostras (~%s)",
                     window_samples,
                     window_span_text);
        } else if (window_samples > 0) {
            snprintf(window_summary_text,
                     sizeof(window_summary_text),
                     "%d amostras na janela de an&aacute;lise",
                     window_samples);
        } else {
            snprintf(window_summary_text,
                     sizeof(window_summary_text),
                     "Nenhuma amostra registrada ainda");
        }

        snprintf(total_samples_text,
                 sizeof(total_samples_text),
                 "%d leituras acumuladas",
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

    char temp_ar_avg_text[32], temp_ar_min_text[32], temp_ar_max_text[32], temp_ar_last_text[32];
    char umid_ar_avg_text[32], umid_ar_min_text[32], umid_ar_max_text[32], umid_ar_last_text[32];
    char temp_solo_avg_text[32], temp_solo_min_text[32], temp_solo_max_text[32], temp_solo_last_text[32];
    char umid_solo_avg_text[32], umid_solo_min_text[32], umid_solo_max_text[32], umid_solo_last_text[32];
    char luminosidade_avg_text[32], luminosidade_min_text[32], luminosidade_max_text[32], luminosidade_last_text[32];
    char dpv_avg_text[32], dpv_min_text[32], dpv_max_text[32], dpv_last_text[32];

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
        snprintf(dpv_avg_text, sizeof(dpv_avg_text), "%.3f&nbsp;kPa", recent_stats.dpv.avg);
        snprintf(dpv_min_text, sizeof(dpv_min_text), "%.3f&nbsp;kPa", recent_stats.dpv.min);
        snprintf(dpv_max_text, sizeof(dpv_max_text), "%.3f&nbsp;kPa", recent_stats.dpv.max);
        snprintf(dpv_last_text, sizeof(dpv_last_text), "%.3f&nbsp;kPa", recent_stats.dpv.latest);
    } else {
        snprintf(dpv_avg_text, sizeof(dpv_avg_text), "--");
        snprintf(dpv_min_text, sizeof(dpv_min_text), "--");
        snprintf(dpv_max_text, sizeof(dpv_max_text), "--");
        snprintf(dpv_last_text, sizeof(dpv_last_text), "--");
    }

    char card_temp_ar[512];
    char card_umid_ar[512];
    char card_temp_solo[512];
    char card_umid_solo[512];
    char card_luminosidade[512];
    char card_dpv[512];

    snprintf(card_temp_ar, sizeof(card_temp_ar),
             "<div class='stats-card'>"
             "<h3>Temp. do ar</h3>"
             "<ul>"
             "<li><span>M&eacute;dia</span><strong>%s</strong></li>"
             "<li><span>M&iacute;nimo</span><strong>%s</strong></li>"
             "<li><span>M&aacute;ximo</span><strong>%s</strong></li>"
             "<li><span>&Uacute;ltima</span><strong>%s</strong></li>"
             "</ul>"
             "</div>",
             temp_ar_avg_text,
             temp_ar_min_text,
             temp_ar_max_text,
             temp_ar_last_text);

    snprintf(card_umid_ar, sizeof(card_umid_ar),
             "<div class='stats-card'>"
             "<h3>Umidade do ar</h3>"
             "<ul>"
             "<li><span>M&eacute;dia</span><strong>%s</strong></li>"
             "<li><span>M&iacute;nimo</span><strong>%s</strong></li>"
             "<li><span>M&aacute;ximo</span><strong>%s</strong></li>"
             "<li><span>&Uacute;ltima</span><strong>%s</strong></li>"
             "</ul>"
             "</div>",
             umid_ar_avg_text,
             umid_ar_min_text,
             umid_ar_max_text,
             umid_ar_last_text);

    snprintf(card_temp_solo, sizeof(card_temp_solo),
             "<div class='stats-card'>"
             "<h3>Temp. do solo</h3>"
             "<ul>"
             "<li><span>M&eacute;dia</span><strong>%s</strong></li>"
             "<li><span>M&iacute;nimo</span><strong>%s</strong></li>"
             "<li><span>M&aacute;ximo</span><strong>%s</strong></li>"
             "<li><span>&Uacute;ltima</span><strong>%s</strong></li>"
             "</ul>"
             "</div>",
             temp_solo_avg_text,
             temp_solo_min_text,
             temp_solo_max_text,
             temp_solo_last_text);

    snprintf(card_umid_solo, sizeof(card_umid_solo),
             "<div class='stats-card'>"
             "<h3>Umidade do solo</h3>"
             "<ul>"
             "<li><span>M&eacute;dia</span><strong>%s</strong></li>"
             "<li><span>M&iacute;nimo</span><strong>%s</strong></li>"
             "<li><span>M&aacute;ximo</span><strong>%s</strong></li>"
             "<li><span>&Uacute;ltima</span><strong>%s</strong></li>"
             "</ul>"
             "</div>",
             umid_solo_avg_text,
             umid_solo_min_text,
             umid_solo_max_text,
             umid_solo_last_text);

    snprintf(card_luminosidade, sizeof(card_luminosidade),
             "<div class='stats-card'>"
             "<h3>Luminosidade</h3>"
             "<ul>"
             "<li><span>M&eacute;dia</span><strong>%s</strong></li>"
             "<li><span>M&iacute;nimo</span><strong>%s</strong></li>"
             "<li><span>M&aacute;ximo</span><strong>%s</strong></li>"
             "<li><span>&Uacute;ltima</span><strong>%s</strong></li>"
             "</ul>"
             "</div>",
             luminosidade_avg_text,
             luminosidade_min_text,
             luminosidade_max_text,
             luminosidade_last_text);

    snprintf(card_dpv, sizeof(card_dpv),
             "<div class='stats-card'>"
             "<h3>DPV</h3>"
             "<ul>"
             "<li><span>M&eacute;dia</span><strong>%s</strong></li>"
             "<li><span>M&iacute;nimo</span><strong>%s</strong></li>"
             "<li><span>M&aacute;ximo</span><strong>%s</strong></li>"
             "<li><span>&Uacute;ltima</span><strong>%s</strong></li>"
             "</ul>"
             "</div>",
             dpv_avg_text,
             dpv_min_text,
             dpv_max_text,
             dpv_last_text);

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
             "<div class='meta-item'><strong>Janela de an&aacute;lise</strong>%s</div>"
             "<div class='meta-item'><strong>Total de leituras</strong>%s</div>"
             "<div class='meta-item'><strong>Armazenamento usado</strong>%s</div>"
             "</div>",
             window_summary_text,
             total_samples_text,
             memory_text);

    /* Buffer alocado no heap para evitar stack overflow */
    char *page = (char *)malloc(12000);
    if (!page) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro de memória");
        return ESP_FAIL;
    }
    
    int len = snprintf(
        page, 12000,
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset='utf-8'/>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
        "<title>Sensor de Campo</title>"
        "<style>"
        "body{font-family:'Inter',sans-serif;margin:0;padding:24px;"
        "background:linear-gradient(180deg,#e8f5e9 0%%,#f6fff5 65%%,#ffffff 100%%);color:#1f2a24}"
        ".content{max-width:960px;margin:auto}"
        ".card{background:#fff;border-radius:16px;padding:20px 24px;margin-bottom:20px;"
        "box-shadow:0 16px 30px rgba(0,0,0,0.08)}"
        ".hero{background:linear-gradient(135deg,#dcedc8,#f1f8e9)}"
        "h1{margin:0;font-size:28px;color:#1b5e20}"
        "h2{margin-top:0;color:#2e4a34}"
        ".subtitle{color:#4f5b62;margin:8px 0 18px;font-size:15px;line-height:1.4}"
        ".info-text{font-size:14px;color:#546e7a;margin:14px 0}"
        ".grid{display:grid;grid-template-columns:1fr 1fr;grid-gap:16px}"
        "canvas{max-width:100%%;height:190px;border:1px solid #dfe6e0;border-radius:10px;background:#fafafa}"
        "a.button{display:inline-block;margin-top:12px;padding:11px 18px;"
        "background:#388e3c;color:#fff;border-radius:8px;text-decoration:none;"
        "font-size:14px;font-weight:600;box-shadow:0 6px 18px rgba(56,142,60,0.35)}"
        ".badge{display:inline-flex;align-items:center;gap:6px;background:#c8e6c9;"
        "color:#1b5e20;font-size:12px;font-weight:700;border-radius:999px;padding:4px 12px}"
        ".caption{font-size:12px;color:#78909c;margin-top:10px}"
        "table{font-size:14px;width:100%%;border-collapse:separate;border-spacing:0;margin-top:12px}"
        "table tr:first-child td{background:#e8f5e9;font-weight:600;color:#1b5e20;padding:10px 12px;border-bottom:2px solid #a5d6a7}"
        "table tr:not(:first-child) td{padding:10px 12px;border-bottom:1px solid #e0e8e1}"
        "table tr:not(:first-child):hover td{background:#f1f8e9}"
        "td:first-child{font-weight:500;color:#2e4a34;min-width:140px}"
        "td:nth-child(2){color:#388e3c;font-weight:500}"
        "td:nth-child(3){color:#546e7a;font-size:13px}"
        ".stats-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px;margin-top:16px}"
        ".stats-card{border:1px solid #e0e8e1;border-radius:14px;padding:14px 16px;background:#f9fbf8}"
        ".stats-card h3{margin:0;font-size:15px;color:#1b5e20}"
        ".stats-card ul{list-style:none;padding:0;margin:12px 0 0}"
        ".stats-card li{display:flex;justify-content:space-between;font-size:13px;margin:4px 0;color:#455a64}"
        ".stats-card li strong{color:#1f2a24}"
        ".meta-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px;margin-top:16px}"
        ".meta-item{background:#f1f8e9;border:1px solid #dfe8d7;border-radius:12px;padding:10px 14px;font-size:13px;color:#2e4a34}"
        ".meta-item strong{display:block;font-size:11px;color:#5a6c57;text-transform:uppercase;letter-spacing:.5px;margin-bottom:4px}"
        "</style>"
        "</head>"
        "<body>"
        "<div class='content'>"

        "<div class='card hero'>"
        "<div class='badge'>greenSe Campo</div>"
        "<h1>Painel em tempo real</h1>"
        "<p class='subtitle'>Monitore o microclima e a umidade do solo diretamente no campo. "
        "Os dados são atualizados automaticamente conforme a frequência de amostragem configurada.</p>"
        "<p class='info-text'>Utilize o resumo estat&iacute;stico e os gr&aacute;ficos para identificar rapidamente variações nas condições ambientais. "
        "As linhas pontilhadas nos gr&aacute;ficos indicam os limites ideais configurados para seu cultivo. "
        "Para an&aacute;lises detalhadas, baixe o arquivo CSV completo.</p>"
        "<div style='display:flex;gap:12px;flex-wrap:wrap;margin-top:8px'>"
        "  <a class='button' href='/' style='background:#388e3c'>Voltar ao painel principal</a>"
        "</div>"
        "</div>"

        "<div class='card stats'>"
        "<h2>Estat&iacute;sticas recentes</h2>"
        "<p class='info-text'>Resumo das leituras baseado na janela de an&aacute;lise configurada. "
        "Os valores mostram m&eacute;dia, m&iacute;nimo, m&aacute;ximo e &uacute;ltima leitura de cada vari&aacute;vel medida.</p>"
        "<p class='info-text'>%s</p>"
        "%s"
        "%s"
        "</div>"

        "<div class='card'>"
        "<h2>Hist&oacute;rico recente</h2>"
        "<p class='info-text'>Gr&aacute;ficos mostrando a evolu&ccedil;&atilde;o temporal das vari&aacute;veis ambientais. "
        "As linhas pontilhadas laranja indicam os limites m&iacute;nimo e m&aacute;ximo ideais para cultivo. "
        "Use essas visualiza&ccedil;&otilde;es para identificar tend&ecirc;ncias e tomar decis&otilde;es r&aacute;pidas no campo.</p>"
        "<div class='grid'>"
        "<div><canvas id='chart_temp_ar' width='320' height='180'></canvas></div>"
        "<div><canvas id='chart_umid_ar' width='320' height='180'></canvas></div>"
        "<div><canvas id='chart_temp_solo' width='320' height='180'></canvas></div>"
        "<div><canvas id='chart_umid_solo' width='320' height='180'></canvas></div>"
        "<div><canvas id='chart_luminosidade' width='320' height='180'></canvas></div>"
        "<div><canvas id='chart_dpv' width='320' height='180'></canvas></div>"
        "</div>"
        "<p class='caption'>Gr&aacute;ficos exibindo at&eacute; %d amostras mais recentes conforme a janela de an&aacute;lise configurada.</p>"
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
        "  desenha('chart_temp_ar','Temp Ar (C)',tAr.xs,tAr.ys,20,40,tol.temp_ar_min,tol.temp_ar_max);"
        "  desenha('chart_umid_ar','UR Ar (%%)',uAr.xs,uAr.ys,0,100,tol.umid_ar_min,tol.umid_ar_max);"
        "  desenha('chart_temp_solo','Temp Solo (C)',tSolo.xs,tSolo.ys,20,40,tol.temp_solo_min,tol.temp_solo_max);"
        "  desenha('chart_umid_solo','Umid Solo (%%)',uSolo.xs,uSolo.ys,0,100,tol.umid_solo_min,tol.umid_solo_max);"
        "  desenha('chart_luminosidade','Luminosidade (lux)',lum.xs,lum.ys,0,2500,tol.luminosidade_min,tol.luminosidade_max);"
        "  desenha('chart_dpv','DPV (kPa)',dpv.xs,dpv.ys,0,3,tol.dpv_min,tol.dpv_max);"
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
        "</body>"
        "</html>",
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
    const char *page =
        "<!DOCTYPE html>"
        "<html><head><meta charset='utf-8'/>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
        "<title>Painel greenSe Campo</title>"
        "<style>"
        "body{font-family:'Inter',sans-serif;background:#edf3ee;color:#1f2a24;margin:0;padding:32px}"
        ".wrapper{max-width:720px;margin:auto}"
        ".card{background:#fff;border-radius:20px;padding:28px;box-shadow:0 20px 40px rgba(0,0,0,0.1)}"
        ".tag{display:inline-block;padding:4px 12px;border-radius:999px;background:#c8e6c9;color:#1b5e20;font-weight:600;font-size:12px}"
        "h1{margin:12px 0 8px;font-size:28px;color:#1b5e20}"
        ".lead{color:#4f5b62;margin-bottom:24px;line-height:1.5}"
        ".actions{display:flex;flex-direction:column;gap:18px}"
        ".action{background:#f7faf6;border-radius:16px;padding:18px 20px;border:1px solid #e0e8e1}"
        ".action p{margin:10px 0 0;font-size:13px;color:#546e7a}"
        "a.button,button{display:inline-flex;align-items:center;justify-content:center;padding:12px 18px;border:none;border-radius:12px;font-weight:600;color:#fff;text-decoration:none;box-shadow:0 10px 24px rgba(56,142,60,0.25)}"
        "a.button{background:#388e3c}"
        "a.button.secondary{background:#1976d2}"
        "a.button.neutral{background:#455a64}"
        "button.delete{background:#c62828;width:100%}"
        ".footer-note{margin-top:28px;font-size:12px;color:#78909c}"
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
        "<div class='card'>"
        "<span class='tag'>greenSe Campo</span>"
        "<h1>Painel principal</h1>"
        "<p class='lead'>Centralize o gerenciamento do seu sensor greenSe Campo. "
        "Acesse o dashboard para monitoramento em tempo real, configure par&acirc;metros de amostragem e cultivo, "
        "ou fa&ccedil;a o download dos dados para an&aacute;lises externas.</p>"
        "<div class='actions'>"
        "<div class='action'>"
        "<a class='button' href='/dashboard'>Ver dashboard</a>"
        "<p>Visualize gr&aacute;ficos em tempo real, estat&iacute;sticas recentes e a evolu&ccedil;&atilde;o das vari&aacute;veis ambientais medidas pelo sensor.</p>"
        "</div>"
        "<div class='action'>"
        "<a class='button secondary' href='/sampling'>Amostragem e Estatísticas</a>"
        "<p>Configure a frequência de coleta de dados e o número de amostras para análise estatística e gráficos.</p>"
        "</div>"
        "<div class='action'>"
        "<a class='button secondary' href='/calibra'>Configurações de Cultivo</a>"
        "<p>Configure as faixas ideais de cultivo para cada vari&aacute;vel e calibre o sensor de umidade do solo para leituras mais precisas.</p>"
        "</div>"
        "<div class='action'>"
        "<a class='button neutral' href='/download'>Baixar log em CSV</a>"
        "<p>Exporte todo o hist&oacute;rico de dados coletados para an&aacute;lise em planilhas ou ferramentas de an&aacute;lise de dados.</p>"
        "</div>"
        "<div class='action'>"
        "<form id='clearForm' method='post' action='/clear_data' onsubmit='return confirmaApagar();'>"
        "<button class='delete' type='submit'>Apagar dados gravados</button>"
        "<p>Remove todos os dados armazenados e reinicia o sistema de registro. Use apenas quando iniciar um novo ciclo de monitoramento.</p>"
        "</form>"
        "</div>"
        "</div>"
        "<p class='footer-note'>greenSe Campo | Tecnologia desenhada para agricultura conectada.</p>"
        "</div>"
        "</div>"
        "</body></html>";

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
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

    const size_t page_capacity = 6144;
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
        "<title>Amostragem e Estatísticas</title>"
        "<style>"
        "body{font-family:'Inter',sans-serif;background:#f0f4f1;color:#1f2a24;margin:0;padding:28px}"
        ".card{background:#fff;border-radius:20px;padding:26px;max-width:520px;margin:auto;"
        "box-shadow:0 22px 40px rgba(0,0,0,0.12)}"
        ".tag{display:inline-block;padding:4px 12px;border-radius:999px;background:#c8e6c9;"
        "color:#1b5e20;font-size:12px;font-weight:600}"
        "h1{margin:10px 0 6px;font-size:26px;color:#1b5e20}"
        "h2{margin:20px 0 8px;font-size:20px;color:#2e7d32}"
        ".lead{color:#4f5b62;line-height:1.5;margin-bottom:18px}"
        ".options{display:flex;flex-direction:column;gap:12px;margin:16px 0}"
        ".option{display:flex;align-items:center;gap:10px;font-size:15px;background:#f7faf6;"
        "border-radius:12px;padding:10px 14px;border:1px solid #e0e8e1}"
        "button{padding:12px 18px;border:none;border-radius:12px;background:#388e3c;"
        "color:#fff;font-size:15px;font-weight:600;cursor:pointer;width:100%%;"
        "box-shadow:0 10px 24px rgba(56,142,60,0.25);box-sizing:border-box}"
        "p.hint{font-size:13px;color:#546e7a;margin-top:12px}"
        "a{color:#1976d2;text-decoration:none}"
        ".button-back{display:block;padding:12px 18px;border-radius:12px;"
        "background:#388e3c;color:#fff;text-decoration:none;text-align:center;"
        "border:1px solid #2e7d32;width:100%%;margin-top:16px;font-weight:600;"
        "box-shadow:0 10px 24px rgba(56,142,60,0.25);box-sizing:border-box}"
        ".section{margin-bottom:28px;padding-bottom:24px;border-bottom:1px solid #e0e8e1}"
        ".section:last-child{border-bottom:none;margin-bottom:0;padding-bottom:0}"
        "</style>"
        "</head><body>"
        "<div class='card'>"
        "<span class='tag'>greenSe Campo</span>"
        "<h1>Amostragem e Estatísticas</h1>"
        "<p class='lead'>Configure a frequência de coleta de dados e o número de amostras usadas para cálculos estatísticos e visualização nos gráficos.</p>"
        "<form action='/set_sampling' method='get'>"
        "<div class='section'>"
        "<h2>Frequência de Amostragem</h2>"
        "<p class='lead'>Defina o intervalo entre cada registro de dados. Intervalos menores fornecem dados mais detalhados, enquanto intervalos maiores economizam energia e memória.</p>"
        "<div class='options'>%s</div>"
        "<p class='hint'>Intervalo atual: <strong>%s</strong></p>"
        "</div>"
        "<div class='section'>"
        "<h2>Janela de Análise Estatística</h2>"
        "<p class='lead'>Configure quantas amostras serão consideradas para calcular médias, mínimos e máximos, além de definir quantos pontos aparecem nos gráficos. Mais amostras revelam tendências de longo prazo; menos amostras destacam variações recentes.</p>"
        "<div class='options'>%s</div>"
        "<p class='hint'>Janela atual: <strong>%d amostras</strong></p>"
        "</div>"
        "<button type='submit'>Salvar configurações</button>"
        "</form>"
        "<a class='button-back' href='/'>Voltar ao painel principal</a>"
        "</div>"
        "</body></html>",
        radios,
        current_label,
        stats_radios,
        current_stats_window
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

    if (has_period) {
        uint32_t new_period = (uint32_t)strtoul(buf_period, NULL, 10);
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
        "<p style='margin-top:20px'><a href='/sampling'>Voltar às configurações</a> | <a href='/'>Painel principal</a></p>"
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
 
    const size_t page_capacity = 8192;
    char *page = malloc(page_capacity);
    if (!page) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro de memória");
        return ESP_FAIL;
    }

    int len = snprintf(page, page_capacity,
             "<!DOCTYPE html><html><head>"
             "<meta charset='utf-8'/>"
             "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
             "<title>Configurações de Cultivo</title>"
             "<style>"
             "body{font-family:'Inter',sans-serif;background:#f0f4f1;color:#1f2a24;margin:0;padding:24px;}"
             ".card{background:#fff;border-radius:20px;padding:26px;max-width:600px;margin:auto;"
             "box-shadow:0 24px 46px rgba(0,0,0,0.12);margin-bottom:20px}"
             ".tag{display:inline-block;padding:4px 12px;border-radius:999px;background:#c8e6c9;"
             "color:#1b5e20;font-size:12px;font-weight:600}"
             "h2{margin:12px 0 6px;font-size:24px;color:#1b5e20}"
             "h3{margin:20px 0 8px;font-size:18px;color:#2e7d32;border-top:2px solid #e0e8e1;padding-top:16px}"
             "h3:first-of-type{border-top:none;padding-top:0;margin-top:0}"
             ".lead{color:#4f5b62;line-height:1.5;margin-bottom:18px}"
             ".info{display:flex;gap:12px;margin:18px 0}"
             ".info-box{flex:1;background:#f7faf6;border:1px solid #e0e8e1;border-radius:14px;"
             "padding:12px 14px;text-align:center;font-size:14px}"
             "form label{display:block;margin:12px 0 4px;font-weight:600;font-size:14px;color:#37474f}"
             ".tolerance-row{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:12px}"
             ".tolerance-row label{grid-column:1/-1;margin-bottom:4px}"
             ".tolerance-row input{width:100%%}"
             "input{width:100%%;padding:10px;border:1px solid #cddbd2;border-radius:10px;"
             "font-size:15px;box-sizing:border-box}"
             "button{width:100%%;padding:12px 18px;border:none;border-radius:12px;background:#388e3c;"
             "color:#fff;font-size:15px;font-weight:600;margin-top:18px;box-shadow:0 10px 24px rgba(56,142,60,0.25)}"
             "a.button{display:inline-block;width:100%%;text-align:center;padding:12px 18px;"
             "border-radius:12px;background:#388e3c;color:#fff;text-decoration:none;font-weight:600;"
             "margin-top:12px;border:1px solid #2e7d32;box-shadow:0 8px 20px rgba(56,142,60,0.25);"
             "box-sizing:border-box}"
             ".tip{font-size:13px;color:#546e7a;margin-top:14px}"
             "</style></head><body>"
             "<div class='card'>"
             "<span class='tag'>greenSe Campo</span>"
             "<h2>Configurações de Cultivo</h2>"
             "<p class='lead'>Personalize as faixas ideais de cultivo para cada vari&aacute;vel ambiental medida. "
             "Esses valores definem os limites m&iacute;nimo e m&aacute;ximo que aparecem como linhas de refer&ecirc;ncia nos gr&aacute;ficos, "
             "facilitando a identifica&ccedil;&atilde;o visual de quando as condi&ccedil;&otilde;es est&atilde;o dentro ou fora da faixa ideal para seu cultivo.</p>"
             
             "<form action='/set_tolerance' method='get'>"
             "<h3>Faixas Ideais de Cultivo</h3>"
             
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
             
             "<button type='submit'>Salvar Faixas Ideais</button>"
             "</form>"
             "<form action='/reset_tolerance' method='get' style='margin-top:12px'>"
             "<button type='submit' style='background:#ff9800;box-shadow:0 10px 24px rgba(255,152,0,0.25)'>Restaurar Valores Padrão</button>"
             "</form>"
             "<p class='tip' style='margin-top:8px'><strong>Restaurar padrões:</strong> O bot&atilde;o acima restaura os valores padr&atilde;o recomendados para cultivos gerais "
             "(20-30°C ar, 50-80%% umidade ar, 18-25°C solo, 40-80%% umidade solo, 500-2000 lux, 0.5-2.0 kPa DPV). "
             "Use quando quiser voltar aos valores iniciais do sistema.</p>"
             
             "<h3>Calibração do Sensor de Umidade do Solo</h3>"
             "<p class='lead'>Ajuste os valores de refer&ecirc;ncia para solo seco e totalmente molhado conforme as condi&ccedil;&otilde;es reais do seu canteiro. "
             "Esta calibra&ccedil;&atilde;o garante que as leituras de umidade do solo reflitam com precis&atilde;o a condi&ccedil;&atilde;o atual do solo.</p>"
             "<div class='info'>"
             "<div class='info-box'><strong>Leitura bruta</strong><br>%d</div>"
             "<div class='info-box'><strong>Faixa atual</strong><br>Seco %.0f | Molhado %.0f</div>"
             "</div>"
             "<form action='/set_calibra' method='get'>"
             "<label>Novo valor para solo seco</label>"
             "<input type='number' name='seco' value='%.0f'>"
             "<label>Novo valor para solo molhado</label>"
             "<input type='number' name='molhado' value='%.0f'>"
             "<button type='submit'>Salvar calibração</button>"
             "</form>"
             "<p class='tip'><strong>Dica:</strong> Para uma calibra&ccedil;&atilde;o precisa, registre o valor do sensor logo ap&oacute;s uma irriga&ccedil;&atilde;o completa (solo molhado) "
             "e ap&oacute;s um per&iacute;odo prolongado sem irriga&ccedil;&atilde;o (solo seco). Isso garante que o sistema capture toda a faixa de varia&ccedil;&atilde;o real do seu solo.</p>"
             "<a class='button' href='/'>Voltar ao painel principal</a>"
             "</div>"
             "</body></html>",
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
        "<p><a href='/calibra'>Voltar</a> | <a href='/'>Painel principal</a></p>"
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
        "<p>As faixas ideais foram restauradas para os valores padrão:</p>"
        "<ul>"
        "<li>Temp. do ar: 20-30&deg;C</li>"
        "<li>Umidade do ar: 50-80%%</li>"
        "<li>Temp. do solo: 18-25&deg;C</li>"
        "<li>Umidade do solo: 40-80%%</li>"
        "<li>Luminosidade: 500-2000 lux</li>"
        "<li>DPV: 0.5-2.0 kPa</li>"
        "</ul>"
        "<p><a href='/calibra'>Voltar às configurações</a> | <a href='/'>Painel principal</a></p>"
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
        "<p><a href='/calibra'>Voltar às configurações</a> | <a href='/'>Painel principal</a></p>"
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
 
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

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
        .handler  = handle_config,
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

    httpd_uri_t uri_dashboard = {
        .uri      = "/dashboard",
        .method   = HTTP_GET,
        .handler  = handle_dashboard,
        .user_ctx = NULL,
    };
    if (httpd_register_uri_handler(server_handle, &uri_dashboard) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao registrar handler /dashboard");
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
 
    ESP_LOGI(TAG,
             "Servidor HTTP iniciado e rotas registradas. Acesse http://192.168.4.1/");
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

