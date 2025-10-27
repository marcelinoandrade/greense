#include "data_logger.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h> // Para 'stat' (verificar se arquivo existe)
#include "esp_spiffs.h" // Para o sistema de arquivos SPIFFS
#include "esp_log.h"

static const char *TAG = "DATA_LOGGER";

// Cabeçalho do CSV, como no seu código Python
static const char* CSV_HEADER = "N,temp_ar_C,umid_ar_pct,temp_solo_C,umid_solo_pct\n";

// Variáveis globais estáticas para manter o estado (como 'self' em Python)
static uint32_t g_log_idx = 0;           // O 'self.idx'
static char g_full_log_path[128];        // O 'self.path' completo (ex: /spiffs/log_temp.csv)
static char g_mount_path[64];            // O 'self.mount_path' (ex: /spiffs)

/**
 * @brief (Interno) Verifica se um arquivo existe.
 * Equivalente ao seu '_arquivo_existe'
 */
static bool _arquivo_existe(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        // Arquivo encontrado
        return true;
    }
    // Arquivo não encontrado
    return false;
}

/**
 * @brief (Interno) Lê o arquivo para encontrar o último índice.
 * Equivalente ao seu '_ler_ultimo_indice'
 */
static uint32_t _ler_ultimo_indice(void) {
    FILE* f = fopen(g_full_log_path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir %s para ler o índice.", g_full_log_path);
        return 0;
    }

    char line[128];
    char last_line[128] = {0}; // Guarda a última linha válida

    // Lê o arquivo linha por linha até o fim
    while (fgets(line, sizeof(line), f)) {
        // Remove a quebra de linha, se houver
        line[strcspn(line, "\r\n")] = 0;
        
        // Ignora linhas vazias
        if (strlen(line) > 0) {
            strcpy(last_line, line);
        }
    }
    fclose(f);

    if (strlen(last_line) == 0) {
        ESP_LOGW(TAG, "Arquivo de log vazio ou inválido.");
        return 0; // Nenhuma linha encontrada
    }

    // Pega a primeira parte da linha (o índice)
    char* token = strtok(last_line, ",");
    if (token != NULL) {
        // Verifica se é a linha do cabeçalho
        if (strcmp(token, "N") == 0) {
            return 0; // A única linha é o cabeçalho
        }
        
        // Converte o token (string) para um inteiro
        int last_idx = atoi(token);
        return (uint32_t)last_idx;
    }

    return 0; // Linha mal formatada
}

/**
 * @brief (Interno) Garante que o cabeçalho exista.
 * Equivalente ao seu '_garantir_cabecalho'
 */
static bool _garantir_cabecalho(void) {
    if (!_arquivo_existe(g_full_log_path)) {
        ESP_LOGI(TAG, "Arquivo de log não encontrado. Criando novo em %s", g_full_log_path);
        FILE* f = fopen(g_full_log_path, "w"); // "w" = Write (cria/trunca)
        if (f == NULL) {
            ESP_LOGE(TAG, "Falha ao criar arquivo de log!");
            return false;
        }
        fprintf(f, "%s", CSV_HEADER);
        fclose(f);
        g_log_idx = 0; // Se o arquivo é novo, índice começa em 0
        ESP_LOGI(TAG, "Cabeçalho do CSV escrito.");
    } else {
        // Se o arquivo existe, lê o último índice
        g_log_idx = _ler_ultimo_indice();
    }
    return true;
}

// --- Funções Públicas ---

bool data_logger_init(const char* mount_path, const char* log_file_name) {
    ESP_LOGI(TAG, "Inicializando SPIFFS...");

    // Salva os caminhos
    snprintf(g_full_log_path, sizeof(g_full_log_path), "%s%s", mount_path, log_file_name);
    strncpy(g_mount_path, mount_path, sizeof(g_mount_path) - 1);
    g_mount_path[sizeof(g_mount_path) - 1] = '\0'; // Garante terminação nula

    esp_vfs_spiffs_conf_t conf = {
      .base_path = g_mount_path, // Onde o VFS será montado
      .partition_label = NULL, // Rótulo da partição (NULL = usa a partição 'spiffs' padrão)
      .max_files = 5,          // Número máximo de arquivos abertos
      .format_if_mount_failed = true // Formata se a montagem falhar
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Falha ao montar ou formatar o sistema de arquivos");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Partição SPIFFS não encontrada. Verifique seu partitions.csv");
        } else {
            ESP_LOGE(TAG, "Falha ao inicializar SPIFFS (%s)", esp_err_to_name(ret));
        }
        return false;
    }

    ESP_LOGI(TAG, "SPIFFS montado em %s", g_mount_path);

    // Garante que o cabeçalho do CSV exista
    if (!_garantir_cabecalho()) {
        return false;
    }
    
    ESP_LOGI(TAG, "Logger inicializado. Próximo índice será: %d", (int)g_log_idx + 1);
    return true;
}

bool data_logger_append(log_data_t data) {
    // Abre o arquivo em modo "append" (adicionar ao final)
    FILE* f = fopen(g_full_log_path, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir %s para append!", g_full_log_path);
        return false;
    }

    // Incrementa o índice global
    g_log_idx++;

    // Escreve a linha formatada no arquivo
    int written = fprintf(f, "%d,%.1f,%.1f,%.1f,%.1f\n",
                          (int)g_log_idx,
                          data.temp_ar,
                          data.umid_ar,
                          data.temp_solo,
                          data.umid_solo);
    
    fclose(f);

    if (written < 0) {
        ESP_LOGE(TAG, "Falha ao escrever no arquivo de log!");
        g_log_idx--; // Desfaz o incremento se a escrita falhou
        return false;
    }

    //ESP_LOGI(TAG, "Log salvo, índice: %d", (int)g_log_idx);
    return true;
}

void data_logger_read_all(void) {
    FILE* f = fopen(g_full_log_path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir %s para leitura.", g_full_log_path);
        return;
    }

    ESP_LOGI(TAG, "--- Lendo %s ---", g_full_log_path);
    char line[256]; // Buffer maior para linhas de log
    while (fgets(line, sizeof(line), f)) {
        // Remove a quebra de linha para uma impressão limpa
        line[strcspn(line, "\r\n")] = 0;
        printf("%s\n", line); // Usa printf para imprimir o dado puro
    }
    fclose(f);
    ESP_LOGI(TAG, "--- Fim do arquivo ---");
}

void data_logger_deinit(void) {
    esp_vfs_spiffs_unregister(NULL); // NULL = partição padrão
    ESP_LOGI(TAG, "SPIFFS desmontado.");
}
