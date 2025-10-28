#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>
#include <errno.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_spiffs.h"

#include "data_logger.h"
#include "sensores.h"

#include "cJSON.h"

static const char *TAG = "DATA_LOGGER";

#define SPIFFS_LABEL   "spiffs"
#define SPIFFS_MOUNT   "/spiffs"
#define LOG_FILE_PATH  SPIFFS_MOUNT "/log_temp.csv"
#define CALIB_FILE     SPIFFS_MOUNT "/soil_calib.json"

/* calibração persistida */
static float calib_seco    = 4000.0f;
static float calib_molhado = 400.0f;

/* índice incremental da linha */
static int linha_idx = 1;

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

    esp_vfs_spiffs_conf_t conf = {
        .base_path              = SPIFFS_MOUNT,
        .partition_label        = SPIFFS_LABEL,
        .max_files              = 5,
        .format_if_mount_failed = true
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));

    size_t total = 0, used = 0;
    ESP_ERROR_CHECK(esp_spiffs_info(SPIFFS_LABEL, &total, &used));
    ESP_LOGI(TAG, "SPIFFS montado em %s", SPIFFS_MOUNT);
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
                "N,temp_ar_C,umid_ar_pct,temp_solo_C,umid_solo_pct\n");
        fclose(f);
        linha_idx = 1;
    } else {
        char line[160];
        fgets(line, sizeof(line), f); // header
        int last_idx = 0;
        while (fgets(line, sizeof(line), f)) {
            int n_local;
            float ta, ua, ts, us;
            if (sscanf(line,
                       "%d,%f,%f,%f,%f",
                       &n_local, &ta, &ua, &ts, &us) == 5)
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
    FILE *f = fopen(LOG_FILE_PATH, "a");
    if (!f) {
        ESP_LOGE(TAG, "Falha ao abrir %s para append", LOG_FILE_PATH);
        return false;
    }

    fprintf(f,
            "%d,%.2f,%.2f,%.2f,%.2f\n",
            linha_idx,
            entry->temp_ar,
            entry->umid_ar,
            entry->temp_solo,
            entry->umid_solo);
    fclose(f);

    ESP_LOGI("APP_MAIN",
             "Log salvo! Temp Solo: %.2f C, Umid Solo: %.2f %%",
             entry->temp_solo,
             entry->umid_solo);

    linha_idx++;
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
   Agora retorna 4 séries independentes:

   {
     "temp_ar_points":   [ [idx, temp_ar_C], ... ],
     "umid_ar_points":   [ [idx, umid_ar_pct], ... ],
     "temp_solo_points": [ [idx, temp_solo_C], ... ],
     "umid_solo_points": [ [idx, umid_solo_pct], ... ]
   }

   Pegamos só os últimos HIST_MAX pontos para não encher RAM.
*/
char *data_logger_build_history_json(void)
{
    FILE *f = fopen(LOG_FILE_PATH, "r");
    if (!f)
        return NULL;

    char line[160];
    fgets(line, sizeof(line), f); // descarta header

    #define HIST_MAX 10
    int   idx_arr[HIST_MAX];
    float temp_ar_arr[HIST_MAX];
    float umid_ar_arr[HIST_MAX];
    float temp_solo_arr[HIST_MAX];
    float umid_solo_arr[HIST_MAX];
    int count = 0;

    while (fgets(line, sizeof(line), f)) {
        int   n_local;
        float ta, ua, ts, us;
        if (sscanf(line,
                   "%d,%f,%f,%f,%f",
                   &n_local, &ta, &ua, &ts, &us) == 5)
        {
            int pos = count % HIST_MAX;
            idx_arr[pos]        = n_local;
            temp_ar_arr[pos]    = ta;
            umid_ar_arr[pos]    = ua;
            temp_solo_arr[pos]  = ts;
            umid_solo_arr[pos]  = us;
            count++;
        }
    }
    fclose(f);

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

    for (int k = 0; k < num; k++) {
        int pos = (start + k) % HIST_MAX;

        /* temp_ar_points: [idx, temp_ar] */
        {
            cJSON *p = cJSON_CreateArray();
            cJSON_AddItemToArray(p, cJSON_CreateNumber(idx_arr[pos]));
            cJSON_AddItemToArray(p, cJSON_CreateNumber(temp_ar_arr[pos]));
            cJSON_AddItemToArray(temp_ar_points, p);
        }

        /* umid_ar_points: [idx, umid_ar] */
        {
            cJSON *p = cJSON_CreateArray();
            cJSON_AddItemToArray(p, cJSON_CreateNumber(idx_arr[pos]));
            cJSON_AddItemToArray(p, cJSON_CreateNumber(umid_ar_arr[pos]));
            cJSON_AddItemToArray(umid_ar_points, p);
        }

        /* temp_solo_points: [idx, temp_solo] */
        {
            cJSON *p = cJSON_CreateArray();
            cJSON_AddItemToArray(p, cJSON_CreateNumber(idx_arr[pos]));
            cJSON_AddItemToArray(p, cJSON_CreateNumber(temp_solo_arr[pos]));
            cJSON_AddItemToArray(temp_solo_points, p);
        }

        /* umid_solo_points: [idx, umid_solo] */
        {
            cJSON *p = cJSON_CreateArray();
            cJSON_AddItemToArray(p, cJSON_CreateNumber(idx_arr[pos]));
            cJSON_AddItemToArray(p, cJSON_CreateNumber(umid_solo_arr[pos]));
            cJSON_AddItemToArray(umid_solo_points, p);
        }
    }

    cJSON_AddItemToObject(root, "temp_ar_points",   temp_ar_points);
    cJSON_AddItemToObject(root, "umid_ar_points",   umid_ar_points);
    cJSON_AddItemToObject(root, "temp_solo_points", temp_solo_points);
    cJSON_AddItemToObject(root, "umid_solo_points", umid_solo_points);

    char *json_txt = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_txt; /* caller da free() */
}
