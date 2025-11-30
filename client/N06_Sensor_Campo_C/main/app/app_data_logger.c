#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>
#include <errno.h>
#include <float.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "app_data_logger.h"
#include "../bsp/board.h"
#include "gui_services.h"
#include "cJSON.h"

static const char *TAG = "APP_DATA_LOGGER";

#define LOG_FILE_PATH  BSP_SPIFFS_MOUNT "/log_temp.csv"
#define CALIB_FILE     BSP_SPIFFS_MOUNT "/soil_calib.json"
#define RECENT_STATS_MAX_WINDOW 64

/* calibração persistida */
static float calib_seco    = 4000.0f;
static float calib_molhado = 400.0f;

/* índice incremental da linha */
static int linha_idx = 1;

/* Mutex para proteger acesso ao arquivo CSV */
static SemaphoreHandle_t file_mutex = NULL;

/* ---------- calibração solo ---------- */

static esp_err_t carregar_calibracao(void)
{
    FILE *f = fopen(CALIB_FILE, "r");
    if (!f) {
        ESP_LOGW(TAG,
                 "Arquivo %s nao encontrado. Usando padrao (Seco: %.0f, Molhado: %.0f)",
                 CALIB_FILE,
                 calib_seco,
                 calib_molhado);
        return ESP_OK;
    }

    char buf[128] = {0};
    size_t rd = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);

    if (rd == 0) {
        ESP_LOGW(TAG, "Calibracao vazia, mantendo padrao.");
        return ESP_OK;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        ESP_LOGW(TAG, "JSON calib invalido");
        return ESP_FAIL;
    }

    cJSON *js_seco    = cJSON_GetObjectItem(root, "seco");
    cJSON *js_molhado = cJSON_GetObjectItem(root, "molhado");

    if (cJSON_IsNumber(js_seco))    calib_seco    = (float)js_seco->valuedouble;
    if (cJSON_IsNumber(js_molhado)) calib_molhado = (float)js_molhado->valuedouble;

    cJSON_Delete(root);

    ESP_LOGI(TAG, "Calibracao carregada. seco=%.0f molhado=%.0f",
             calib_seco, calib_molhado);
    return ESP_OK;
}

static esp_err_t salvar_calibracao(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "seco",    calib_seco);
    cJSON_AddNumberToObject(root, "molhado", calib_molhado);

    char *txt = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!txt) return ESP_FAIL;

    FILE *f = fopen(CALIB_FILE, "w");
    if (!f) {
        ESP_LOGE(TAG, "Erro abrindo %s p/ escrita (%d)", CALIB_FILE, errno);
        free(txt);
        return ESP_FAIL;
    }
    fwrite(txt, 1, strlen(txt), f);
    fclose(f);

    ESP_LOGI(TAG,
             "Calibracao salva. seco=%.0f molhado=%.0f",
             calib_seco, calib_molhado);

    free(txt);
    return ESP_OK;
}

/* ---------- API pública ---------- */

esp_err_t data_logger_init(void)
{
    ESP_LOGI(TAG, "Inicializando SPIFFS...");

    /* Cria mutex para proteger acesso ao arquivo */
    if (file_mutex == NULL) {
        file_mutex = xSemaphoreCreateMutex();
        if (file_mutex == NULL) {
            ESP_LOGE(TAG, "Falha ao criar mutex");
            return ESP_FAIL;
        }
    }

    esp_vfs_spiffs_conf_t conf = {
        .base_path              = BSP_SPIFFS_MOUNT,
        .partition_label        = BSP_SPIFFS_LABEL,
        .max_files              = BSP_SPIFFS_MAX_FILES,
        .format_if_mount_failed = true
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));

    size_t total = 0, used = 0;
    ESP_ERROR_CHECK(esp_spiffs_info(BSP_SPIFFS_LABEL, &total, &used));
    ESP_LOGI(TAG, "SPIFFS montado em %s", BSP_SPIFFS_MOUNT);
    ESP_LOGI(TAG, "Total=%d bytes, Usado=%d bytes", (int)total, (int)used);

    /* criar ou recuperar log_temp.csv */
    FILE *f = fopen(LOG_FILE_PATH, "r");
    if (!f) {
        ESP_LOGI(TAG, "Criando novo %s", LOG_FILE_PATH);
        f = fopen(LOG_FILE_PATH, "w");
        if (!f) {
            ESP_LOGE(TAG, "Nao consegui criar %s", LOG_FILE_PATH);
            return ESP_FAIL;
        }
        fprintf(f,
                "N,temp_ar_C,umid_ar_pct,temp_solo_C,umid_solo_pct,luminosidade_lux,dpv_kPa\n");
        fclose(f);
        linha_idx = 1;
    } else {
        char line[160];
        fgets(line, sizeof(line), f); // header
        int last_idx = 0;
        while (fgets(line, sizeof(line), f)) {
            int n_local;
            float ta, ua, ts, us, lum, dpv;
            // Tenta ler formato antigo (4 variáveis) ou novo (6 variáveis)
            if (sscanf(line,
                       "%d,%f,%f,%f,%f,%f,%f",
                       &n_local, &ta, &ua, &ts, &us, &lum, &dpv) >= 5)
            {
                last_idx = n_local;
            }
        }
        fclose(f);
        linha_idx = last_idx + 1;
        ESP_LOGI(TAG, "Proximo indice de log sera: %d", linha_idx);
    }

    carregar_calibracao();
    data_logger_dump_to_logcat();
    return ESP_OK;
}

float data_logger_raw_to_pct(int leitura_raw)
{
    if (leitura_raw < 0) {
        return NAN;
    }

    float seco    = calib_seco;
    float molhado = calib_molhado;
    if (fabsf(seco - molhado) < 1e-3f) {
        return NAN;
    }

    /* seco -> 0%, molhado -> 100% (sua convenção atual) */
    float pct = 100.0f * ( (seco - (float)leitura_raw) / (seco - molhado) );
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return pct;
}

void data_logger_get_calibracao(float *seco_out, float *molhado_out)
{
    if (seco_out)    *seco_out    = calib_seco;
    if (molhado_out) *molhado_out = calib_molhado;
}

esp_err_t data_logger_set_calibracao(float seco, float molhado)
{
    calib_seco    = seco;
    calib_molhado = molhado;
    return salvar_calibracao();
}

bool data_logger_append(const log_entry_t *entry)
{
    if (file_mutex == NULL) {
        ESP_LOGE(TAG, "Mutex nao inicializado");
        return false;
    }

    /* Protege acesso ao arquivo */
    if (xSemaphoreTake(file_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Timeout ao obter mutex para append");
        return false;
    }

    FILE *f = fopen(LOG_FILE_PATH, "a");
    if (!f) {
        ESP_LOGE(TAG, "Falha ao abrir %s para append", LOG_FILE_PATH);
        xSemaphoreGive(file_mutex);
        return false;
    }

    fprintf(f,
            "%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.3f\n",
            linha_idx,
            entry->temp_ar,
            entry->umid_ar,
            entry->temp_solo,
            entry->umid_solo,
            entry->luminosidade,
            entry->dpv);
    fclose(f);

    linha_idx++;
    xSemaphoreGive(file_mutex);
    return true;
}

void data_logger_dump_to_logcat(void)
{
    ESP_LOGI(TAG, "--- Lendo %s ---", LOG_FILE_PATH);
    FILE *f = fopen(LOG_FILE_PATH, "r");
    if (!f) {
        ESP_LOGW(TAG, "Nao consegui abrir %s", LOG_FILE_PATH);
        return;
    }

    char line[160];
    while (fgets(line, sizeof(line), f)) {
        ESP_LOGI(TAG, "%s", line);
    }

    fclose(f);
    ESP_LOGI(TAG, "--- Fim do arquivo ---");
}

/*
   Retorna 6 séries independentes:

   {
     "temp_ar_points":   [ [idx, temp_ar_C], ... ],
     "umid_ar_points":   [ [idx, umid_ar_pct], ... ],
     "temp_solo_points": [ [idx, temp_solo_C], ... ],
     "umid_solo_points": [ [idx, umid_solo_pct], ... ],
     "luminosidade_points": [ [idx, luminosidade_lux], ... ],
     "dpv_points":       [ [idx, dpv_kPa], ... ]
   }

   Pegamos só os últimos HIST_MAX pontos para não encher RAM.
*/
char *data_logger_build_history_json(void)
{
    if (file_mutex == NULL) {
        ESP_LOGE(TAG, "Mutex nao inicializado");
        return NULL;
    }

    /* Protege acesso ao arquivo */
    if (xSemaphoreTake(file_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        ESP_LOGE(TAG, "Timeout ao obter mutex para leitura");
        return NULL;
    }

    FILE *f = fopen(LOG_FILE_PATH, "r");
    if (!f) {
        xSemaphoreGive(file_mutex);
        return NULL;
    }

    char line[160];
    fgets(line, sizeof(line), f); // descarta header

    #define HIST_MAX 10
    int   idx_arr[HIST_MAX];
    float temp_ar_arr[HIST_MAX];
    float umid_ar_arr[HIST_MAX];
    float temp_solo_arr[HIST_MAX];
    float umid_solo_arr[HIST_MAX];
    float luminosidade_arr[HIST_MAX];
    float dpv_arr[HIST_MAX];
    int count = 0;

    while (fgets(line, sizeof(line), f)) {
        int   n_local;
        float ta, ua, ts, us, lum = NAN, dpv = NAN;
        // Tenta ler formato novo (6 variáveis) ou antigo (4 variáveis)
        int fields = sscanf(line,
                           "%d,%f,%f,%f,%f,%f,%f",
                           &n_local, &ta, &ua, &ts, &us, &lum, &dpv);
        if (fields >= 5)  // Aceita formato antigo (4 variáveis) ou novo (6 variáveis)
        {
            int pos = count % HIST_MAX;
            idx_arr[pos]        = n_local;
            temp_ar_arr[pos]    = ta;
            umid_ar_arr[pos]    = ua;
            temp_solo_arr[pos]  = ts;
            umid_solo_arr[pos]  = us;
            luminosidade_arr[pos] = (fields >= 6) ? lum : NAN;
            dpv_arr[pos]        = (fields >= 7) ? dpv : NAN;
            count++;
        }
    }
    fclose(f);
    xSemaphoreGive(file_mutex); /* Libera mutex após ler arquivo */

    int start = 0;
    int num   = count;
    if (num > HIST_MAX) {
        start = count - HIST_MAX;
        num   = HIST_MAX;
    }

    cJSON *root              = cJSON_CreateObject();
    cJSON *temp_ar_points    = cJSON_CreateArray();
    cJSON *umid_ar_points    = cJSON_CreateArray();
    cJSON *temp_solo_points  = cJSON_CreateArray();
    cJSON *umid_solo_points  = cJSON_CreateArray();
    cJSON *luminosidade_points = cJSON_CreateArray();
    cJSON *dpv_points       = cJSON_CreateArray();

    for (int k = 0; k < num; k++) {
        int pos = (start + k) % HIST_MAX;

        /* temp_ar_points: [idx, temp_ar] */
        if (isfinite(temp_ar_arr[pos])) {
            cJSON *p = cJSON_CreateArray();
            cJSON_AddItemToArray(p, cJSON_CreateNumber(idx_arr[pos]));
            cJSON_AddItemToArray(p, cJSON_CreateNumber(temp_ar_arr[pos]));
            cJSON_AddItemToArray(temp_ar_points, p);
        }

        /* umid_ar_points: [idx, umid_ar] */
        if (isfinite(umid_ar_arr[pos])) {
            cJSON *p = cJSON_CreateArray();
            cJSON_AddItemToArray(p, cJSON_CreateNumber(idx_arr[pos]));
            cJSON_AddItemToArray(p, cJSON_CreateNumber(umid_ar_arr[pos]));
            cJSON_AddItemToArray(umid_ar_points, p);
        }

        /* temp_solo_points: [idx, temp_solo] */
        if (isfinite(temp_solo_arr[pos])) {
            cJSON *p = cJSON_CreateArray();
            cJSON_AddItemToArray(p, cJSON_CreateNumber(idx_arr[pos]));
            cJSON_AddItemToArray(p, cJSON_CreateNumber(temp_solo_arr[pos]));
            cJSON_AddItemToArray(temp_solo_points, p);
        }

        /* umid_solo_points: [idx, umid_solo] */
        if (isfinite(umid_solo_arr[pos])) {
            cJSON *p = cJSON_CreateArray();
            cJSON_AddItemToArray(p, cJSON_CreateNumber(idx_arr[pos]));
            cJSON_AddItemToArray(p, cJSON_CreateNumber(umid_solo_arr[pos]));
            cJSON_AddItemToArray(umid_solo_points, p);
        }

        /* luminosidade_points: [idx, luminosidade] */
        if (isfinite(luminosidade_arr[pos])) {
            cJSON *p = cJSON_CreateArray();
            cJSON_AddItemToArray(p, cJSON_CreateNumber(idx_arr[pos]));
            cJSON_AddItemToArray(p, cJSON_CreateNumber(luminosidade_arr[pos]));
            cJSON_AddItemToArray(luminosidade_points, p);
        }

        /* dpv_points: [idx, dpv] */
        if (isfinite(dpv_arr[pos])) {
            cJSON *p = cJSON_CreateArray();
            cJSON_AddItemToArray(p, cJSON_CreateNumber(idx_arr[pos]));
            cJSON_AddItemToArray(p, cJSON_CreateNumber(dpv_arr[pos]));
            cJSON_AddItemToArray(dpv_points, p);
        }
    }

    cJSON_AddItemToObject(root, "temp_ar_points",   temp_ar_points);
    cJSON_AddItemToObject(root, "umid_ar_points",   umid_ar_points);
    cJSON_AddItemToObject(root, "temp_solo_points", temp_solo_points);
    cJSON_AddItemToObject(root, "umid_solo_points", umid_solo_points);
    cJSON_AddItemToObject(root, "luminosidade_points", luminosidade_points);
    cJSON_AddItemToObject(root, "dpv_points",       dpv_points);

    char *json_txt = cJSON_PrintUnformatted(root);
    if (json_txt == NULL) {
        ESP_LOGE(TAG, "Falha ao gerar JSON");
        cJSON_Delete(root);
        return NULL;
    }
    
    cJSON_Delete(root);
    return json_txt; /* caller da free() */
}

static void gui_stats_reset(gui_sensor_stats_t *stat)
{
    if (!stat) {
        return;
    }
    stat->min      = 0.0f;
    stat->max      = 0.0f;
    stat->avg      = 0.0f;
    stat->latest   = 0.0f;
    stat->has_data = false;
}

static void gui_stats_compute(gui_sensor_stats_t *dest,
                              const float *buffer,
                              int total_samples,
                              int max_window,
                              int window_samples)
{
    gui_stats_reset(dest);
    if (!dest || !buffer || window_samples <= 0 || total_samples <= 0) {
        return;
    }

    float  min_v        = FLT_MAX;
    float  max_v        = -FLT_MAX;
    double sum          = 0.0;
    int    valid_samples = 0;
    float  latest_value  = 0.0f;
    bool   latest_valid  = false;

    int start_idx = (total_samples <= max_window) ? 0 : (total_samples % max_window);

    for (int i = 0; i < window_samples; ++i) {
        int idx = (total_samples <= max_window) ? i : ((start_idx + i) % max_window);
        float value = buffer[idx];
        if (!isfinite(value)) {
            continue;
        }
        if (value < min_v) min_v = value;
        if (value > max_v) max_v = value;
        sum += value;
        valid_samples++;
        latest_value = value;
        latest_valid = true;
    }

    if (valid_samples == 0 || !latest_valid) {
        return;
    }

    dest->has_data = true;
    dest->min      = min_v;
    dest->max      = max_v;
    dest->avg      = (float)(sum / valid_samples);
    dest->latest   = latest_value;
}

bool data_logger_get_recent_stats(int max_samples, gui_recent_stats_t *out)
{
    if (!out) {
        return false;
    }

    if (max_samples <= 0) {
        max_samples = RECENT_STATS_MAX_WINDOW;
    } else if (max_samples > RECENT_STATS_MAX_WINDOW) {
        max_samples = RECENT_STATS_MAX_WINDOW;
    }

    memset(out, 0, sizeof(*out));
    gui_stats_reset(&out->temp_ar);
    gui_stats_reset(&out->umid_ar);
    gui_stats_reset(&out->temp_solo);
    gui_stats_reset(&out->umid_solo);
    gui_stats_reset(&out->luminosidade);
    gui_stats_reset(&out->dpv);

    if (file_mutex == NULL) {
        ESP_LOGE(TAG, "Mutex nao inicializado para estatisticas");
        return false;
    }

    float temp_ar_buf[RECENT_STATS_MAX_WINDOW]   = {0};
    float umid_ar_buf[RECENT_STATS_MAX_WINDOW]   = {0};
    float temp_solo_buf[RECENT_STATS_MAX_WINDOW] = {0};
    float umid_solo_buf[RECENT_STATS_MAX_WINDOW] = {0};
    float luminosidade_buf[RECENT_STATS_MAX_WINDOW] = {0};
    float dpv_buf[RECENT_STATS_MAX_WINDOW] = {0};
    int   total_samples = 0;

    if (xSemaphoreTake(file_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        ESP_LOGE(TAG, "Timeout ao obter mutex para estatisticas");
        return false;
    }

    FILE *f = fopen(LOG_FILE_PATH, "r");
    if (f) {
        char line[160];
        /* descarta header se existir */
        fgets(line, sizeof(line), f);

        while (fgets(line, sizeof(line), f)) {
            int   idx_local;
            float ta, ua, ts, us, lum = NAN, dpv = NAN;
            // Tenta ler formato novo (6 variáveis) ou antigo (4 variáveis)
            int fields = sscanf(line, "%d,%f,%f,%f,%f,%f,%f",
                       &idx_local, &ta, &ua, &ts, &us, &lum, &dpv);
            if (fields >= 5) {  // Aceita formato antigo ou novo
                int pos             = total_samples % max_samples;
                temp_ar_buf[pos]    = ta;
                umid_ar_buf[pos]    = ua;
                temp_solo_buf[pos]  = ts;
                umid_solo_buf[pos]  = us;
                luminosidade_buf[pos] = (fields >= 6) ? lum : NAN;
                dpv_buf[pos]        = (fields >= 7) ? dpv : NAN;
                total_samples++;
            }
        }
        fclose(f);
    } else {
        ESP_LOGW(TAG, "Nao consegui abrir %s para estatisticas", LOG_FILE_PATH);
    }

    xSemaphoreGive(file_mutex);

    int window_samples = (total_samples < max_samples) ? total_samples : max_samples;
    out->window_samples = window_samples;
    out->total_samples  = total_samples;

    if (window_samples > 0) {
        gui_stats_compute(&out->temp_ar,   temp_ar_buf,   total_samples, max_samples, window_samples);
        gui_stats_compute(&out->umid_ar,   umid_ar_buf,   total_samples, max_samples, window_samples);
        gui_stats_compute(&out->temp_solo, temp_solo_buf, total_samples, max_samples, window_samples);
        gui_stats_compute(&out->umid_solo, umid_solo_buf, total_samples, max_samples, window_samples);
        gui_stats_compute(&out->luminosidade, luminosidade_buf, total_samples, max_samples, window_samples);
        gui_stats_compute(&out->dpv,       dpv_buf,       total_samples, max_samples, window_samples);
    }

    size_t total_bytes = 0;
    size_t used_bytes  = 0;
    if (esp_spiffs_info(BSP_SPIFFS_LABEL, &total_bytes, &used_bytes) == ESP_OK) {
        out->storage_total_bytes = total_bytes;
        out->storage_used_bytes  = used_bytes;
    } else {
        out->storage_total_bytes = 0;
        out->storage_used_bytes  = 0;
        ESP_LOGW(TAG, "Falha ao obter info do SPIFFS");
    }

    return true;
}

esp_err_t data_logger_clear_all(void)
{
    ESP_LOGI(TAG, "Limpando todos os dados armazenados...");
    
    /* Remove arquivo de log */
    if (remove(LOG_FILE_PATH) == 0) {
        ESP_LOGI(TAG, "Arquivo %s removido", LOG_FILE_PATH);
    } else {
        ESP_LOGW(TAG, "Nao foi possivel remover %s (pode nao existir)", LOG_FILE_PATH);
    }
    /* Recria arquivo com header padrão para manter histórico funcionando */
    FILE *f = fopen(LOG_FILE_PATH, "w");
    if (f) {
        fprintf(f, "N,temp_ar_C,umid_ar_pct,temp_solo_C,umid_solo_pct,luminosidade_lux,dpv_kPa\n");
        fclose(f);
        ESP_LOGI(TAG, "Arquivo %s recriado com header", LOG_FILE_PATH);
    } else {
        ESP_LOGE(TAG, "Falha ao recriar %s apos limpeza", LOG_FILE_PATH);
    }
    
    /* Remove arquivo de calibração */
    if (remove(CALIB_FILE) == 0) {
        ESP_LOGI(TAG, "Arquivo %s removido", CALIB_FILE);
    } else {
        ESP_LOGW(TAG, "Nao foi possivel remover %s (pode nao existir)", CALIB_FILE);
    }
    
    /* Reseta índice e calibração para valores padrão */
    linha_idx = 1;
    calib_seco = 4000.0f;
    calib_molhado = 400.0f;
    
    ESP_LOGI(TAG, "Dados limpos. Sistema reiniciado do zero.");
    return ESP_OK;
}

