#include "bsp_sdcard.h"
#include "bsp_pins.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <errno.h>
#include <stdlib.h>

#define TAG "BSP_SDCARD"

static bool s_sdcard_mounted = false;
static sdmmc_card_t* s_card = NULL;

esp_err_t bsp_sdcard_init(void)
{
    if (s_card != NULL) {
        ESP_LOGI(TAG, "SD Card já inicializado");
        return ESP_OK; // Já inicializado
    }
    
    ESP_LOGI(TAG, "Inicializando cartão SD...");
    ESP_LOGI(TAG, "Pinos: CMD=GPIO%d, CLK=GPIO%d, DATA=GPIO%d", 
             SD_CMD_GPIO, SD_CLK_GPIO, SD_DATA_GPIO);
    
    // Configuração do host SDMMC
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    
    // --- CORREÇÃO DE VELOCIDADE (baseado no template_01) ---
    // Limita a frequência para evitar problemas de estabilidade
    host.max_freq_khz = SDMMC_FREQ_DEFAULT; // 20MHz - rápido e confiável
    // --------------------------------------------------------
    
    // Configuração do slot SDMMC
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    
    // Configuração dos pinos para ESP32-S3
    slot_config.width = 1;  // 1-bit mode inicial (mais compatível)
    slot_config.clk = SD_CLK_GPIO;
    slot_config.cmd = SD_CMD_GPIO;
    slot_config.d0 = SD_DATA_GPIO;
    slot_config.d1 = GPIO_NUM_NC;  // Não usado em 1-bit mode
    slot_config.d2 = GPIO_NUM_NC;
    slot_config.d3 = GPIO_NUM_NC;
    
    // Usa pull-ups internos (importante para estabilidade)
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    
    // Configuração de montagem
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,  // Formata se necessário (como template_01)
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, 
                                            &mount_config, &s_card);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Falha ao montar sistema de arquivos.");
            ESP_LOGE(TAG, "Verifique se o cartão está formatado ou tente formatá-lo.");
        } else {
            ESP_LOGE(TAG, "Falha ao inicializar cartão SD: %s", esp_err_to_name(ret));
            ESP_LOGE(TAG, "Verifique se o cartão está inserido e os pinos estão corretos:");
            ESP_LOGE(TAG, "  CMD: GPIO%d, CLK: GPIO%d, DATA: GPIO%d", 
                     SD_CMD_GPIO, SD_CLK_GPIO, SD_DATA_GPIO);
        }
        s_sdcard_mounted = false;
        s_card = NULL;
        return ret;
    }
    
    s_sdcard_mounted = true;
    ESP_LOGI(TAG, "Cartão SD montado com sucesso em %s", SD_MOUNT_POINT);
    
    // Imprime informações do cartão (como template_01)
    sdmmc_card_print_info(stdout, s_card);
    
    // Lista arquivos no diretório raiz
    ESP_LOGI(TAG, "Listando arquivos em %s:", SD_MOUNT_POINT);
    DIR* dir = opendir(SD_MOUNT_POINT);
    if (dir != NULL) {
        struct dirent* entry;
        int count = 0;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                ESP_LOGI(TAG, "  %s", entry->d_name);
                count++;
            }
        }
        closedir(dir);
        if (count == 0) {
            ESP_LOGI(TAG, "  (diretório vazio)");
        }
    }
    
    // Teste de escrita
    FILE* f = fopen(SD_MOUNT_POINT "/teste.txt", "w");
    if (f) {
        fprintf(f, "Teste de escrita no cartão SD.\n");
        fclose(f);
        ESP_LOGI(TAG, "✅ Teste de escrita: OK");
    } else {
        ESP_LOGW(TAG, "⚠️ Teste de escrita: FALHOU (mas cartão montado)");
    }
    
    return ESP_OK;
}

esp_err_t bsp_sdcard_save_file(const char* filename, const uint8_t* data, size_t data_len)
{
    if (!s_sdcard_mounted || s_card == NULL) {
        ESP_LOGE(TAG, "SD Card não está montado");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!filename || !data || data_len == 0) {
        ESP_LOGE(TAG, "Parâmetros inválidos em save_file (filename=%p, data=%p, len=%d)",
                 (void*)filename, (void*)data, (int)data_len);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Valida nome do arquivo (não pode ser vazio ou muito longo)
    size_t filename_len = strlen(filename);
    if (filename_len == 0 || filename_len > 64) {
        ESP_LOGE(TAG, "Nome de arquivo inválido: comprimento=%d", (int)filename_len);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Constrói caminho completo (garante que não exceda o tamanho)
    char full_path[128];
    int ret = snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, filename);
    if (ret < 0 || ret >= (int)sizeof(full_path)) {
        ESP_LOGE(TAG, "Caminho muito longo: %s/%s", SD_MOUNT_POINT, filename);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Salvando arquivo: %s (%d bytes)", full_path, (int)data_len);
    
    // Verifica se o diretório existe
    struct stat st = {0};
    if (stat(SD_MOUNT_POINT, &st) == -1) {
        ESP_LOGE(TAG, "Diretório %s não existe ou não está acessível (errno=%d)", 
                 SD_MOUNT_POINT, errno);
        return ESP_FAIL;
    }
    
    // Remove arquivo existente se houver (para evitar problemas)
    int unlink_ret = unlink(full_path);
    if (unlink_ret == 0) {
        ESP_LOGD(TAG, "Arquivo existente removido: %s", full_path);
    } else if (errno != ENOENT) {
        ESP_LOGW(TAG, "Aviso ao remover arquivo existente (errno=%d: %s)", errno, strerror(errno));
    }
    
    // Copia dados para RAM interna se estiverem em PSRAM (mais seguro para escrita)
    uint8_t *ram_data = NULL;
    bool needs_free = false;
    
    // Verifica se os dados podem estar em PSRAM (endereço > 0x3F800000 para ESP32-S3)
    if ((uintptr_t)data >= 0x3F800000) {
        ESP_LOGD(TAG, "Dados em PSRAM detectados, copiando para RAM interna...");
        ram_data = (uint8_t *)malloc(data_len);
        if (!ram_data) {
            ESP_LOGE(TAG, "Falha ao alocar buffer RAM para cópia");
            return ESP_ERR_NO_MEM;
        }
        memcpy(ram_data, data, data_len);
        needs_free = true;
        data = ram_data;
    }
    
    // Tenta abrir o arquivo (tenta primeiro com caminho completo, depois apenas o nome)
    FILE* f = fopen(full_path, "wb");
    if (f == NULL) {
        // Tenta abrir apenas com o nome do arquivo (sem o mount point)
        ESP_LOGD(TAG, "Tentando abrir apenas com nome do arquivo: %s", filename);
        f = fopen(filename, "wb");
    }
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir arquivo para escrita (errno=%d: %s)", 
                 errno, strerror(errno));
        ESP_LOGE(TAG, "Caminho tentado: %s", full_path);
        ESP_LOGE(TAG, "Mount point: %s", SD_MOUNT_POINT);
        ESP_LOGE(TAG, "Nome do arquivo: %s (len=%d)", filename, (int)filename_len);
        if (needs_free) {
            free(ram_data);
        }
        return ESP_FAIL;
    }
    
    size_t bytes_written = fwrite(data, 1, data_len, f);
    if (bytes_written != data_len) {
        ESP_LOGE(TAG, "Erro durante escrita: escreveu apenas %d de %d bytes (errno=%d: %s)",
                 (int)bytes_written, (int)data_len, errno, strerror(errno));
        fclose(f);
        if (needs_free) {
            free(ram_data);
        }
        return ESP_FAIL;
    }
    
    fflush(f);  // Garante que os dados são escritos
    fclose(f);
    
    if (needs_free) {
        free(ram_data);
    }
    
    ESP_LOGI(TAG, "✅ Arquivo salvo com sucesso: %d bytes", (int)bytes_written);
    return ESP_OK;
}

bool bsp_sdcard_is_mounted(void)
{
    return s_sdcard_mounted;
}

esp_err_t bsp_sdcard_append_file(const char* filename, const uint8_t* data, size_t data_len)
{
    if (!s_sdcard_mounted || s_card == NULL) {
        ESP_LOGE(TAG, "SD Card não está montado");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!filename || !data || data_len == 0) {
        ESP_LOGE(TAG, "Parâmetros inválidos em append_file");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Constrói caminho completo
    char full_path[128];
    int ret = snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, filename);
    if (ret < 0 || ret >= (int)sizeof(full_path)) {
        ESP_LOGE(TAG, "Caminho muito longo: %s/%s", SD_MOUNT_POINT, filename);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGD(TAG, "Adicionando dados ao arquivo: %s (%d bytes)", full_path, (int)data_len);
    
    // Copia dados para RAM interna se estiverem em PSRAM
    uint8_t *ram_data = NULL;
    bool needs_free = false;
    
    if ((uintptr_t)data >= 0x3F800000) {
        ESP_LOGD(TAG, "Dados em PSRAM detectados, copiando para RAM interna...");
        ram_data = (uint8_t *)malloc(data_len);
        if (!ram_data) {
            ESP_LOGE(TAG, "Falha ao alocar buffer RAM para cópia");
            return ESP_ERR_NO_MEM;
        }
        memcpy(ram_data, data, data_len);
        needs_free = true;
        data = ram_data;
    }
    
    // Abre arquivo em modo append (ab = append binary)
    FILE* f = fopen(full_path, "ab");
    if (f == NULL) {
        // Se arquivo não existe, cria novo
        f = fopen(full_path, "wb");
        if (f == NULL) {
            ESP_LOGE(TAG, "Falha ao abrir/criar arquivo para append (errno=%d: %s)", 
                     errno, strerror(errno));
            if (needs_free) {
                free(ram_data);
            }
            return ESP_FAIL;
        }
    }
    
    size_t bytes_written = fwrite(data, 1, data_len, f);
    if (bytes_written != data_len) {
        ESP_LOGE(TAG, "Erro durante append: escreveu apenas %d de %d bytes (errno=%d: %s)",
                 (int)bytes_written, (int)data_len, errno, strerror(errno));
        fclose(f);
        if (needs_free) {
            free(ram_data);
        }
        return ESP_FAIL;
    }
    
    fflush(f);
    fclose(f);
    
    if (needs_free) {
        free(ram_data);
    }
    
    ESP_LOGD(TAG, "✅ Dados adicionados ao arquivo: %d bytes", (int)bytes_written);
    return ESP_OK;
}

size_t bsp_sdcard_get_file_size(const char* filename)
{
    if (!s_sdcard_mounted || s_card == NULL) {
        return 0;
    }
    
    if (!filename) {
        return 0;
    }
    
    // Constrói caminho completo
    char full_path[128];
    int ret = snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, filename);
    if (ret < 0 || ret >= (int)sizeof(full_path)) {
        return 0;
    }
    
    struct stat st;
    if (stat(full_path, &st) != 0) {
        return 0;  // Arquivo não existe ou erro
    }
    
    return (size_t)st.st_size;
}

esp_err_t bsp_sdcard_verify_write(const char* filename, const uint8_t* expected_data, size_t expected_len, size_t offset)
{
    if (!s_sdcard_mounted || s_card == NULL) {
        ESP_LOGE(TAG, "SD Card não está montado");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!filename || !expected_data || expected_len == 0) {
        ESP_LOGE(TAG, "Parâmetros inválidos em verify_write");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Constrói caminho completo
    char full_path[128];
    int ret = snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, filename);
    if (ret < 0 || ret >= (int)sizeof(full_path)) {
        ESP_LOGE(TAG, "Caminho muito longo");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Abre arquivo para leitura
    FILE* f = fopen(full_path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir arquivo para verificação (errno=%d)", errno);
        return ESP_FAIL;
    }
    
    // Se offset é 0, verifica os últimos bytes escritos
    if (offset == 0) {
        // Posiciona no final do arquivo menos o tamanho esperado
        if (fseek(f, 0, SEEK_END) != 0) {
            ESP_LOGE(TAG, "Falha ao posicionar no final do arquivo");
            fclose(f);
            return ESP_FAIL;
        }
        
        long file_size = ftell(f);
        if (file_size < 0 || (size_t)file_size < expected_len) {
            ESP_LOGE(TAG, "Arquivo muito pequeno para verificação (tamanho=%ld, esperado=%d)", 
                     file_size, (int)expected_len);
            fclose(f);
            return ESP_FAIL;
        }
        
        offset = (size_t)file_size - expected_len;
    }
    
    // Posiciona no offset
    if (fseek(f, (long)offset, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Falha ao posicionar no offset %d", (int)offset);
        fclose(f);
        return ESP_FAIL;
    }
    
    // Lê dados do arquivo
    uint8_t *read_buffer = malloc(expected_len);
    if (!read_buffer) {
        ESP_LOGE(TAG, "Falha ao alocar buffer para verificação");
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    
    size_t bytes_read = fread(read_buffer, 1, expected_len, f);
    fclose(f);
    
    if (bytes_read != expected_len) {
        ESP_LOGE(TAG, "Falha ao ler dados para verificação (leu %d de %d bytes)", 
                 (int)bytes_read, (int)expected_len);
        free(read_buffer);
        return ESP_FAIL;
    }
    
    // Compara dados
    if (memcmp(read_buffer, expected_data, expected_len) != 0) {
        ESP_LOGE(TAG, "❌ Dados verificados não correspondem aos dados escritos!");
        free(read_buffer);
        return ESP_FAIL;
    }
    
    free(read_buffer);
    ESP_LOGD(TAG, "✅ Verificação read-after-write bem-sucedida: %d bytes", (int)expected_len);
    return ESP_OK;
}

esp_err_t bsp_sdcard_read_thermal_frame(const char* filename, uint32_t frame_index, uint8_t* frame_data, size_t frame_size)
{
    if (!s_sdcard_mounted || s_card == NULL) {
        ESP_LOGE(TAG, "SD Card não está montado");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!filename || !frame_data || frame_size == 0) {
        ESP_LOGE(TAG, "Parâmetros inválidos em read_thermal_frame");
        return ESP_ERR_INVALID_ARG;
    }
    
    char full_path[128];
    int ret = snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, filename);
    if (ret < 0 || ret >= (int)sizeof(full_path)) {
        ESP_LOGE(TAG, "Caminho muito longo");
        return ESP_ERR_INVALID_ARG;
    }
    
    FILE* f = fopen(full_path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir arquivo para leitura (errno=%d)", errno);
        return ESP_FAIL;
    }
    
    // Calcula offset do frame
    long offset = (long)frame_index * (long)frame_size;
    
    // Posiciona no offset
    if (fseek(f, offset, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Falha ao posicionar no offset %ld (errno=%d)", offset, errno);
        fclose(f);
        return ESP_FAIL;
    }
    
    // Lê frame
    size_t bytes_read = fread(frame_data, 1, frame_size, f);
    fclose(f);
    
    if (bytes_read != frame_size) {
        ESP_LOGE(TAG, "Falha ao ler frame (leu %d de %d bytes)", (int)bytes_read, (int)frame_size);
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "✅ Frame %lu lido: %d bytes", (unsigned long)frame_index, (int)frame_size);
    return ESP_OK;
}

esp_err_t bsp_sdcard_read_thermal_timestamps(const char* filename, time_t* timestamps, size_t max_frames, size_t* frames_read)
{
    if (!s_sdcard_mounted || s_card == NULL) {
        ESP_LOGE(TAG, "SD Card não está montado");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!filename || !timestamps || max_frames == 0 || !frames_read) {
        ESP_LOGE(TAG, "Parâmetros inválidos em read_thermal_timestamps");
        return ESP_ERR_INVALID_ARG;
    }
    
    *frames_read = 0;
    
    char full_path[128];
    int ret = snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, filename);
    if (ret < 0 || ret >= (int)sizeof(full_path)) {
        ESP_LOGE(TAG, "Caminho muito longo");
        return ESP_ERR_INVALID_ARG;
    }
    
    FILE* f = fopen(full_path, "rb");
    if (f == NULL) {
        ESP_LOGD(TAG, "Arquivo de metadados não existe");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Lê timestamps sequencialmente (cada bloco JSON contém timestamps de uma migração)
    // Por simplicidade, vamos ler apenas do último bloco JSON para obter os timestamps mais recentes
    // O formato do arquivo é: {"frames":[{"timestamp":...},...]}\n (blocos repetidos)
    
    // Estratégia: lê o arquivo inteiro e extrai timestamps do último bloco
    size_t file_size = bsp_sdcard_get_file_size(filename);
    if (file_size == 0 || file_size > 102400) {  // Limita a 100KB
        fclose(f);
        ESP_LOGE(TAG, "Arquivo de metadados muito grande ou vazio");
        return ESP_FAIL;
    }
    
    char* json_buffer = malloc(file_size + 1);
    if (!json_buffer) {
        fclose(f);
        ESP_LOGE(TAG, "Falha ao alocar buffer para metadados");
        return ESP_ERR_NO_MEM;
    }
    
    size_t bytes_read = fread(json_buffer, 1, file_size, f);
    fclose(f);
    
    if (bytes_read == 0) {
        free(json_buffer);
        return ESP_ERR_NOT_FOUND;
    }
    
    json_buffer[bytes_read] = '\0';
    
    // Extrai timestamps do arquivo JSON
    // Formato: múltiplos blocos {"frames":[{"timestamp":...},...]}\n
    // Estratégia: procura por todos os "timestamp":NNNNN e extrai sequencialmente
    
    size_t count = 0;
    char* search_ptr = json_buffer;
    
    while (count < max_frames && search_ptr < json_buffer + bytes_read) {
        char* ts_start = strstr(search_ptr, "\"timestamp\":");
        if (!ts_start) {
            break;
        }
        
        ts_start += strlen("\"timestamp\":");
        // Pula espaços
        while (*ts_start == ' ' || *ts_start == '\t') ts_start++;
        
        // Lê número
        char* end_ptr;
        time_t ts = (time_t)strtoul(ts_start, &end_ptr, 10);
        
        if (ts > 0 && ts < 2147483647) {  // Valida timestamp Unix válido (até 2038)
            timestamps[count++] = ts;
        }
        
        search_ptr = end_ptr + 1;  // Continua busca após o número
    }
    
    *frames_read = count;
    free(json_buffer);
    
    if (count == 0) {
        ESP_LOGW(TAG, "Nenhum timestamp válido encontrado");
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGD(TAG, "✅ Lidos %d timestamps do arquivo de metadados", (int)count);
    return ESP_OK;
}

esp_err_t bsp_sdcard_save_send_index(const char* filename, uint32_t frames_sent)
{
    if (!s_sdcard_mounted || s_card == NULL) {
        ESP_LOGE(TAG, "SD Card não está montado");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!filename) {
        ESP_LOGE(TAG, "Parâmetros inválidos em save_send_index");
        return ESP_ERR_INVALID_ARG;
    }
    
    char full_path[128];
    int ret = snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, filename);
    if (ret < 0 || ret >= (int)sizeof(full_path)) {
        ESP_LOGE(TAG, "Caminho muito longo");
        return ESP_ERR_INVALID_ARG;
    }
    
    FILE* f = fopen(full_path, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao criar arquivo de índice (errno=%d)", errno);
        return ESP_FAIL;
    }
    
    if (fwrite(&frames_sent, sizeof(uint32_t), 1, f) != 1) {
        ESP_LOGE(TAG, "Falha ao escrever índice");
        fclose(f);
        return ESP_FAIL;
    }
    
    fflush(f);
    fclose(f);
    
    ESP_LOGD(TAG, "✅ Índice salvo: %lu frames enviados", (unsigned long)frames_sent);
    return ESP_OK;
}

esp_err_t bsp_sdcard_read_send_index(const char* filename, uint32_t* frames_sent)
{
    if (!s_sdcard_mounted || s_card == NULL) {
        ESP_LOGE(TAG, "SD Card não está montado");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!filename || !frames_sent) {
        ESP_LOGE(TAG, "Parâmetros inválidos em read_send_index");
        return ESP_ERR_INVALID_ARG;
    }
    
    char full_path[128];
    int ret = snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, filename);
    if (ret < 0 || ret >= (int)sizeof(full_path)) {
        ESP_LOGE(TAG, "Caminho muito longo");
        return ESP_ERR_INVALID_ARG;
    }
    
    FILE* f = fopen(full_path, "rb");
    if (f == NULL) {
        // Arquivo não existe = nenhum frame foi enviado ainda
        *frames_sent = 0;
        return ESP_ERR_NOT_FOUND;
    }
    
    if (fread(frames_sent, sizeof(uint32_t), 1, f) != 1) {
        ESP_LOGE(TAG, "Falha ao ler índice");
        fclose(f);
        *frames_sent = 0;
        return ESP_FAIL;
    }
    
    fclose(f);
    ESP_LOGD(TAG, "✅ Índice lido: %lu frames enviados", (unsigned long)*frames_sent);
    return ESP_OK;
}

esp_err_t bsp_sdcard_rename_file(const char* old_filename, const char* new_filename)
{
    if (!s_sdcard_mounted || s_card == NULL) {
        ESP_LOGE(TAG, "SD Card não está montado");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!old_filename || !new_filename) {
        ESP_LOGE(TAG, "Parâmetros inválidos em rename_file");
        return ESP_ERR_INVALID_ARG;
    }
    
    char old_path[128];
    char new_path[128];
    
    int ret1 = snprintf(old_path, sizeof(old_path), "%s/%s", SD_MOUNT_POINT, old_filename);
    int ret2 = snprintf(new_path, sizeof(new_path), "%s/%s", SD_MOUNT_POINT, new_filename);
    
    if (ret1 < 0 || ret1 >= (int)sizeof(old_path) || ret2 < 0 || ret2 >= (int)sizeof(new_path)) {
        ESP_LOGE(TAG, "Caminho muito longo");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Renomeando arquivo: %s -> %s", old_filename, new_filename);
    
    // Remove arquivo de destino se já existir (evita erro EEXIST)
    if (access(new_path, F_OK) == 0) {
        ESP_LOGW(TAG, "Arquivo de destino já existe, removendo: %s", new_filename);
        if (unlink(new_path) != 0) {
            ESP_LOGW(TAG, "Aviso: Falha ao remover arquivo existente (errno=%d), tentando renomear mesmo assim", errno);
        }
    }
    
    int result = rename(old_path, new_path);
    if (result != 0) {
        ESP_LOGE(TAG, "Falha ao renomear arquivo (errno=%d: %s)", errno, strerror(errno));
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ Arquivo renomeado com sucesso");
    return ESP_OK;
}

