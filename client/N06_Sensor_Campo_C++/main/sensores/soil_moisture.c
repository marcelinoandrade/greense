#include "soil_moisture.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "cJSON.h" // Biblioteca JSON que já vem no ESP-IDF
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_adc/adc_oneshot.h" // Driver ADC moderno
#include "soc/soc_caps.h"

static const char *TAG = "SOIL_MOISTURE";

// --- Configurações do Sensor ---
// Use `adc_oneshot_config_channel` para mudar o pino
// GPIO 34 é ADC1_CHANNEL_6
#define SOIL_ADC_CHANNEL    ADC_CHANNEL_6
#define SOIL_ADC_ATTEN      ADC_ATTEN_DB_11 // Atenuação de 11dB (0-3.3V)

// --- Variáveis Estáticas (equivalente ao 'self' do Python) ---
static float g_seco_raw = 4000.0;     // Padrão se calibração não existir
static float g_molhado_raw = 400.0;  // Padrão se calibração não existir
static char g_calib_path[128];        // Caminho completo: /spiffs/soil_calib.json
static adc_oneshot_unit_handle_t g_adc_handle;

// Nome do arquivo de calibração (como no Python)
static const char* CALIB_FILE_NAME = "/soil_calib.json";

/**
 * @brief (Interno) Tenta carregar a calibração do arquivo JSON.
 * Equivalente ao '_carregar_calibracao' do Python.
 */
static bool _carregar_calibracao(void) {
    struct stat st;
    if (stat(g_calib_path, &st) != 0) {
        ESP_LOGW(TAG, "Arquivo %s nao encontrado. Usando valores padrao (Seco: %.0f, Molhado: %.0f)", 
                 g_calib_path, g_seco_raw, g_molhado_raw);
        return false; // Arquivo não existe
    }

    FILE* f = fopen(g_calib_path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir %s para leitura.", g_calib_path);
        return false;
    }

    // Aloca buffer para ler o arquivo
    char* buffer = malloc(st.st_size + 1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Falha ao alocar memoria para ler calibracao");
        fclose(f);
        return false;
    }
    fread(buffer, 1, st.st_size, f);
    buffer[st.st_size] = '\0';
    fclose(f);

    // Parse do JSON
    cJSON *json = cJSON_Parse(buffer);
    if (json == NULL) {
        ESP_LOGE(TAG, "Falha ao parsear %s. Arquivo corrompido?", g_calib_path);
        free(buffer);
        return false;
    }

    cJSON *seco_json = cJSON_GetObjectItemCaseSensitive(json, "seco_raw");
    cJSON *molhado_json = cJSON_GetObjectItemCaseSensitive(json, "molhado_raw");

    if (cJSON_IsNumber(seco_json)) {
        g_seco_raw = seco_json->valuedouble;
    }
    if (cJSON_IsNumber(molhado_json)) {
        g_molhado_raw = molhado_json->valuedouble;
    }

    ESP_LOGI(TAG, "Calibracao carregada: Seco=%.0f, Molhado=%.0f", g_seco_raw, g_molhado_raw);

    cJSON_Delete(json);
    free(buffer);
    return true;
}

// --- Funções Públicas ---

bool soil_moisture_init(const char* mount_path) {
    // 1. Constrói o caminho completo para o arquivo de calibração
    snprintf(g_calib_path, sizeof(g_calib_path), "%s%s", mount_path, CALIB_FILE_NAME);

    // 2. Configura o driver ADC
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1 // GPIO 34 está no ADC1
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &g_adc_handle));

    // 3. Configura o canal ADC
    adc_oneshot_chan_cfg_t chan_config = {
        .atten = SOIL_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT // Padrão (12 bits)
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(g_adc_handle, SOIL_ADC_CHANNEL, &chan_config));

    ESP_LOGI(TAG, "ADC inicializado no Canal %d", SOIL_ADC_CHANNEL);

    // 4. Tenta carregar a calibração
    _carregar_calibracao(); // O resultado (true/false) não impede a inicialização
    
    return true;
}

bool soil_moisture_salvar_calibracao(float seco, float molhado) {
    g_seco_raw = seco;
    g_molhado_raw = molhado;

    ESP_LOGI(TAG, "Salvando calibracao: Seco=%.0f, Molhado=%.0f", g_seco_raw, g_molhado_raw);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Falha ao criar objeto cJSON");
        return false;
    }

    cJSON_AddNumberToObject(root, "seco_raw", g_seco_raw);
    cJSON_AddNumberToObject(root, "molhado_raw", g_molhado_raw);

    char* json_str = cJSON_Print(root);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "Falha ao converter cJSON para string");
        cJSON_Delete(root);
        return false;
    }
    
    cJSON_Delete(root); // Libera o objeto cJSON

    FILE* f = fopen(g_calib_path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir %s para escrita!", g_calib_path);
        free(json_str);
        return false;
    }

    fprintf(f, "%s", json_str);
    fclose(f);
    free(json_str); // Libera a string

    ESP_LOGI(TAG, "Calibracao salva com sucesso em %s", g_calib_path);
    return true;
}

int soil_moisture_leitura_bruta(void) {
    int raw_value = 0;
    esp_err_t ret = adc_oneshot_read(g_adc_handle, SOIL_ADC_CHANNEL, &raw_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha na leitura ADC: %s", esp_err_to_name(ret));
        return 0; // Retorna 0 em caso de erro
    }
    return raw_value;
}

float soil_moisture_umidade_percentual(void) {
    float val = (float)soil_moisture_leitura_bruta();
    float seco = g_seco_raw;
    float molh = g_molhado_raw;

    // Prevenção de divisão por zero
    if (seco == molh) {
        return 0.0;
    }

    // Mapeamento linear (como no seu Python)
    // Leitura alta = seco
    // Leitura baixa = molhado
    float pct = (seco - val) * 100.0 / (seco - molh);

    // Limitar 0..100
    if (pct < 0.0) {
        pct = 0.0;
    }
    if (pct > 100.0) {
        pct = 100.0;
    }

    return pct;
}
