#include "bsp_spiffs.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define TAG "BSP_SPIFFS"
#define SPIFFS_MOUNT_POINT "/spiffs"
#define SPIFFS_PARTITION_LABEL "spiffs"
#define PARTIAL_DATA_FILE "/spiffs/thermal_partial.bin"
#define PARTIAL_DATA_FILE_TMP "/spiffs/thermal_partial.tmp"
#define PARTIAL_INDEX_FILE "/spiffs/thermal_index.bin"
#define PARTIAL_INDEX_FILE_TMP "/spiffs/thermal_index.tmp"
#define ACCUM_DATA_FILE "/spiffs/thermal_accum.bin"
#define ACCUM_META_FILE "/spiffs/thermal_accum_meta.bin"
#define MAX_RETRY_ATTEMPTS 3
#define RETRY_DELAY_MS 100

static bool s_spiffs_mounted = false;

esp_err_t bsp_spiffs_init(void)
{
    if (s_spiffs_mounted) {
        ESP_LOGI(TAG, "SPIFFS já está montado");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Inicializando SPIFFS...");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_MOUNT_POINT,
        .partition_label = SPIFFS_PARTITION_LABEL,
        .max_files = 5,
        .format_if_mount_failed = true  // Formata se necessário
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Falha ao montar ou formatar sistema de arquivos SPIFFS");
            ESP_LOGE(TAG, "A partição pode estar corrompida. Verifique partitions.csv");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Partição SPIFFS não encontrada. Verifique partitions.csv");
            ESP_LOGE(TAG, "Certifique-se de que a partição 'spiffs' está definida na tabela de partições");
        } else {
            ESP_LOGE(TAG, "Falha ao inicializar SPIFFS: %s (0x%x)", 
                     esp_err_to_name(ret), (unsigned int)ret);
        }
        s_spiffs_mounted = false;
        return ret;
    }

    // Aguarda um pouco para garantir que a formatação (se ocorreu) foi finalizada
    vTaskDelay(pdMS_TO_TICKS(100));

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(SPIFFS_PARTITION_LABEL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao obter informações do SPIFFS (%s)", esp_err_to_name(ret));
        // Continua mesmo assim, pois a montagem foi bem-sucedida
    } else {
        ESP_LOGI(TAG, "✅ SPIFFS montado com sucesso em %s", SPIFFS_MOUNT_POINT);
        ESP_LOGI(TAG, "📊 Partição SPIFFS: total=%d KB, usado=%d KB, livre=%d KB", 
                 (int)(total / 1024), (int)(used / 1024), (int)((total - used) / 1024));
        
        if (used == 0) {
            ESP_LOGI(TAG, "ℹ️ SPIFFS formatado (primeira inicialização) - pronto para uso");
        }
    }

    s_spiffs_mounted = true;
    return ESP_OK;
}

bool bsp_spiffs_is_mounted(void)
{
    return s_spiffs_mounted;
}

// Calcula checksum CRC32 simples para validação de integridade
uint32_t bsp_spiffs_calculate_checksum(const void *data, size_t len)
{
    if (!data || len == 0) {
        return 0;
    }
    
    // CRC32 polinomial: 0x04C11DB7 (padrão IEEE 802.3)
    const uint32_t polynomial = 0x04C11DB7;
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *bytes = (const uint8_t *)data;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint32_t)bytes[i] << 24;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc ^ 0xFFFFFFFF;
}

esp_err_t bsp_spiffs_save_partial_frame(int frame_index, const void *frame, size_t frame_size)
{
    if (!s_spiffs_mounted) {
        ESP_LOGE(TAG, "SPIFFS não está montado");
        return ESP_ERR_INVALID_STATE;
    }

    if (!frame || frame_size == 0 || frame_index < 0) {
        ESP_LOGE(TAG, "Parâmetros inválidos em save_partial_frame");
        return ESP_ERR_INVALID_ARG;
    }

    // Calcula checksum do frame para validação
    uint32_t frame_checksum = bsp_spiffs_calculate_checksum(frame, frame_size);
    
    // Retry com backoff para operações críticas
    for (int attempt = 0; attempt < MAX_RETRY_ATTEMPTS; attempt++) {
        if (attempt > 0) {
            ESP_LOGW(TAG, "Tentativa %d/%d de salvar frame parcial", attempt + 1, MAX_RETRY_ATTEMPTS);
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS * attempt));
        }

        // Estratégia simplificada: escreve diretamente no arquivo final
        // Não usa arquivo temporário devido a problemas com rename() no SPIFFS
        // A validação read-after-write + checksum garante integridade
        
        // Abre arquivo para escrita (modo r+b para permitir leitura/escrita)
        FILE *f = fopen(PARTIAL_DATA_FILE, "rb+");
        
        if (f == NULL) {
            // Arquivo não existe, cria novo
            f = fopen(PARTIAL_DATA_FILE, "wb");
            if (f == NULL) {
                ESP_LOGE(TAG, "Falha ao criar arquivo parcial: %s (errno=%d)", PARTIAL_DATA_FILE, errno);
                continue;
            }
        }

        // Posiciona no offset do frame
        long offset = (long)frame_index * (long)frame_size;
        long current_size = 0;
        
        // Verifica tamanho atual do arquivo
        if (fseek(f, 0, SEEK_END) == 0) {
            current_size = ftell(f);
        } else {
            ESP_LOGE(TAG, "Falha ao verificar tamanho do arquivo");
            fclose(f);
            continue;
        }
        
        // Se o arquivo precisa ser expandido para acomodar este frame
        if (offset + (long)frame_size > current_size) {
            // Estratégia melhorada: escreve zeros do final atual até a posição necessária
            long bytes_to_expand = (offset + (long)frame_size) - current_size;
            
            // Posiciona no final do arquivo
            if (fseek(f, current_size, SEEK_SET) != 0) {
                ESP_LOGE(TAG, "Falha ao posicionar no final do arquivo para expansão");
                fclose(f);
                continue;
            }
            
            // Escreve zeros para expandir o arquivo
            uint8_t *zero_buf = calloc(1, 4096);  // Buffer de zeros
            if (!zero_buf) {
                ESP_LOGE(TAG, "Falha ao alocar buffer para expansão");
                fclose(f);
                continue;
            }
            
            size_t written_expand = 0;
            while (written_expand < (size_t)bytes_to_expand) {
                size_t chunk = (bytes_to_expand - written_expand > 4096) ? 4096 : (bytes_to_expand - written_expand);
                size_t written = fwrite(zero_buf, 1, chunk, f);
                if (written != chunk) {
                    ESP_LOGE(TAG, "Falha ao expandir arquivo (escreveu %d de %d bytes)", 
                             (int)written, (int)chunk);
                    free(zero_buf);
                    fclose(f);
                    continue;
                }
                written_expand += written;
            }
            
            free(zero_buf);
            fflush(f);  // Garante que a expansão foi escrita
        }
        
        // Posiciona no offset correto do frame
        if (fseek(f, offset, SEEK_SET) != 0) {
            ESP_LOGE(TAG, "Falha ao posicionar no arquivo (offset=%ld, tamanho=%ld)", offset, current_size);
            fclose(f);
            continue;
        }

        // Escreve o frame
        size_t written = fwrite(frame, 1, frame_size, f);
        if (written != frame_size) {
            ESP_LOGE(TAG, "Falha ao escrever frame (escreveu %d de %d bytes)", 
                     (int)written, (int)frame_size);
            fclose(f);
            continue;
        }

        fflush(f);  // Garante escrita imediata
        fsync(fileno(f));  // Força sincronização com storage (SPIFFS)
        fclose(f);  // Fecha arquivo após escrita
        
        // Pequeno delay para garantir que SPIFFS finalizou todas as operações
        vTaskDelay(pdMS_TO_TICKS(50));
        
        // Read-after-write: Reabre arquivo para verificação
        FILE *verify_f = fopen(PARTIAL_DATA_FILE, "rb");
        if (verify_f == NULL) {
            ESP_LOGE(TAG, "Falha ao reabrir arquivo para verificação (errno=%d)", errno);
            continue;
        }

        // Posiciona no offset do frame
        if (fseek(verify_f, offset, SEEK_SET) != 0) {
            ESP_LOGE(TAG, "Falha ao posicionar para verificação (offset=%ld, errno=%d)", offset, errno);
            fclose(verify_f);
            continue;
        }

        uint8_t *verify_buf = malloc(frame_size);
        if (!verify_buf) {
            ESP_LOGE(TAG, "Falha ao alocar buffer para verificação");
            fclose(verify_f);
            continue;
        }

        size_t read = fread(verify_buf, 1, frame_size, verify_f);
        fclose(verify_f);

        if (read != frame_size) {
            ESP_LOGE(TAG, "Falha ao ler dados para verificação (leu %d de %d bytes, offset=%ld)", 
                     (int)read, (int)frame_size, offset);
            free(verify_buf);
            continue;
        }

        // Valida checksum
        uint32_t verify_checksum = bsp_spiffs_calculate_checksum(verify_buf, frame_size);
        free(verify_buf);

        if (verify_checksum != frame_checksum) {
            ESP_LOGE(TAG, "Checksum inválido após escrita (esperado: 0x%08X, obtido: 0x%08X)", 
                     (unsigned int)frame_checksum, (unsigned int)verify_checksum);
            continue;  // Tenta novamente
        }

        // Salva índice (escrita direta, sem rename)
        FILE *idx_f = fopen(PARTIAL_INDEX_FILE, "wb");
        if (idx_f == NULL) {
            ESP_LOGW(TAG, "Falha ao criar arquivo de índice (errno=%d)", errno);
            // Continua mesmo assim, pois dados foram salvos
        } else {
            int last_index = frame_index + 1;
            if (fwrite(&last_index, sizeof(int), 1, idx_f) != 1) {
                ESP_LOGW(TAG, "Falha ao escrever índice");
            }
            fflush(idx_f);
            fsync(fileno(idx_f));  // Força sincronização
            fclose(idx_f);
        }

        ESP_LOGD(TAG, "✅ Frame parcial salvo e validado: índice=%d, tamanho=%d bytes, checksum=0x%08X", 
                 frame_index, (int)frame_size, (unsigned int)frame_checksum);
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Falha ao salvar frame parcial após %d tentativas", MAX_RETRY_ATTEMPTS);
    return ESP_FAIL;
}

int bsp_spiffs_load_partial_frames(void *buffer, size_t buffer_size, size_t frame_size)
{
    if (!s_spiffs_mounted) {
        ESP_LOGE(TAG, "SPIFFS não está montado");
        return -1;
    }

    if (!buffer || buffer_size == 0 || frame_size == 0) {
        ESP_LOGE(TAG, "Parâmetros inválidos em load_partial_frames");
        return -1;
    }

    // Verifica se o arquivo existe
    struct stat st;
    if (stat(PARTIAL_DATA_FILE, &st) != 0) {
        ESP_LOGD(TAG, "Nenhum dado parcial encontrado");
        return 0;  // Não é erro, apenas não há dados
    }

    // Lê índice (quantidade de frames salvos)
    int frame_count = 0;
    FILE *idx_f = fopen(PARTIAL_INDEX_FILE, "rb");
    if (idx_f != NULL) {
        if (fread(&frame_count, sizeof(int), 1, idx_f) != 1) {
            ESP_LOGW(TAG, "Falha ao ler índice");
            frame_count = 0;
        }
        fclose(idx_f);
    } else {
        // Se não há arquivo de índice, tenta determinar pelo tamanho do arquivo
        ESP_LOGW(TAG, "Arquivo de índice não encontrado, estimando quantidade de frames");
        if (st.st_size > 0 && frame_size > 0) {
            frame_count = (int)(st.st_size / frame_size);
            ESP_LOGI(TAG, "Estimativa: %d frames baseado no tamanho do arquivo (%d bytes)", 
                     frame_count, (int)st.st_size);
        } else {
            return 0;
        }
    }

    // Valida frame_count
    if (frame_count <= 0 || frame_count > (int)(buffer_size / frame_size)) {
        ESP_LOGW(TAG, "Frame count inválido ou buffer pequeno (count=%d, buf_frames=%d)", 
                 frame_count, (int)(buffer_size / frame_size));
        if (st.st_size > 0 && frame_size > 0) {
            frame_count = (int)(st.st_size / frame_size);
            if (frame_count > (int)(buffer_size / frame_size)) {
                frame_count = (int)(buffer_size / frame_size);
                ESP_LOGW(TAG, "Ajustado frame_count para %d (limite do buffer)", frame_count);
            }
        } else {
            return 0;
        }
    }

    // Lê dados do arquivo
    FILE *f = fopen(PARTIAL_DATA_FILE, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir arquivo parcial para leitura");
        return -1;
    }

    size_t bytes_to_read = (size_t)frame_count * frame_size;
    if (bytes_to_read > buffer_size) {
        bytes_to_read = buffer_size;
        ESP_LOGW(TAG, "Limitando leitura a %d bytes (tamanho do buffer)", (int)bytes_to_read);
    }

    size_t bytes_read = fread(buffer, 1, bytes_to_read, f);
    fclose(f);

    if (bytes_read == 0) {
        ESP_LOGW(TAG, "Arquivo parcial está vazio");
        return 0;
    }

    // Valida integridade dos frames lidos
    int valid_frames = 0;
    uint8_t *frame_ptr = (uint8_t *)buffer;
    
    for (int i = 0; i < frame_count; i++) {
        if ((size_t)(i + 1) * frame_size > bytes_read) {
            ESP_LOGW(TAG, "Frame %d incompleto (esperado %d bytes, disponível %d)", 
                     i, (int)frame_size, (int)(bytes_read - i * frame_size));
            break;
        }

        // Verifica se o frame tem dados válidos (não está zerado)
        bool is_valid = false;
        for (size_t j = 0; j < frame_size; j++) {
            if (frame_ptr[i * frame_size + j] != 0) {
                is_valid = true;
                break;
            }
        }

        if (is_valid) {
            // Validação adicional: verifica se timestamp é razoável (não zero)
            // (assumindo que timestamp é o primeiro campo de thermal_frame_t)
            if (frame_size >= sizeof(time_t)) {
                time_t *ts = (time_t *)&frame_ptr[i * frame_size];
                if (*ts > 0 && *ts < 2147483647) {  // Timestamp Unix válido até 2038
                    valid_frames++;
                } else {
                    ESP_LOGW(TAG, "Frame %d tem timestamp inválido: %ld", i, (long)*ts);
                }
            } else {
                valid_frames++;
            }
        } else {
            ESP_LOGW(TAG, "Frame %d está zerado (provavelmente não foi escrito)", i);
        }
    }

    if (valid_frames == 0) {
        ESP_LOGW(TAG, "Nenhum frame válido encontrado no arquivo parcial");
        return 0;
    }

    if (valid_frames < frame_count) {
        ESP_LOGW(TAG, "Apenas %d de %d frames são válidos", valid_frames, frame_count);
    }

    ESP_LOGI(TAG, "✅ Carregados e validados %d frames parciais do SPIFFS (%d bytes)", 
             valid_frames, (int)bytes_read);
    return valid_frames;
}

bool bsp_spiffs_has_partial_data(void)
{
    if (!s_spiffs_mounted) {
        return false;
    }

    struct stat st;
    if (stat(PARTIAL_DATA_FILE, &st) != 0) {
        return false;  // Arquivo não existe
    }

    // Verifica se o arquivo tem tamanho válido (> 0)
    return (st.st_size > 0);
}

esp_err_t bsp_spiffs_clear_partial_data(void)
{
    if (!s_spiffs_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;

    // Remove arquivo de dados
    if (unlink(PARTIAL_DATA_FILE) != 0) {
        if (errno != ENOENT) {  // ENOENT = arquivo não existe (não é erro)
            ESP_LOGW(TAG, "Falha ao remover arquivo parcial (errno=%d)", errno);
            ret = ESP_FAIL;
        }
    }

    // Remove arquivo de índice
    if (unlink(PARTIAL_INDEX_FILE) != 0) {
        if (errno != ENOENT) {
            ESP_LOGW(TAG, "Falha ao remover arquivo de índice (errno=%d)", errno);
            ret = ESP_FAIL;
        }
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Dados parciais removidos do SPIFFS");
    }

    return ret;
}

// ========== Funções para arquivo acumulativo ==========

esp_err_t bsp_spiffs_append_thermal_frame(const void *frame_data, size_t frame_size, time_t timestamp)
{
    if (!s_spiffs_mounted) {
        ESP_LOGE(TAG, "SPIFFS não está montado");
        return ESP_ERR_INVALID_STATE;
    }

    if (!frame_data || frame_size == 0) {
        ESP_LOGE(TAG, "Parâmetros inválidos em append_thermal_frame");
        return ESP_ERR_INVALID_ARG;
    }

    // Retry com backoff
    for (int attempt = 0; attempt < MAX_RETRY_ATTEMPTS; attempt++) {
        if (attempt > 0) {
            ESP_LOGW(TAG, "Tentativa %d/%d de append frame acumulativo", attempt + 1, MAX_RETRY_ATTEMPTS);
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS * attempt));
        }

        // Abre arquivo acumulativo em modo append (a+b = append binary)
        FILE *f = fopen(ACCUM_DATA_FILE, "ab");
        if (f == NULL) {
            ESP_LOGE(TAG, "Falha ao abrir arquivo acumulativo para append (errno=%d)", errno);
            continue;
        }

        // Escreve dados do frame
        size_t written = fwrite(frame_data, 1, frame_size, f);
        if (written != frame_size) {
            ESP_LOGE(TAG, "Falha ao escrever frame acumulativo (escreveu %d de %d bytes)", 
                     (int)written, (int)frame_size);
            fclose(f);
            continue;
        }

        fflush(f);
        fsync(fileno(f));
        fclose(f);

        // Salva timestamp no arquivo de metadados
        FILE *meta_f = fopen(ACCUM_META_FILE, "ab");
        if (meta_f != NULL) {
            if (fwrite(&timestamp, sizeof(time_t), 1, meta_f) != 1) {
                ESP_LOGW(TAG, "Falha ao escrever timestamp no arquivo de metadados");
            } else {
                fflush(meta_f);
                fsync(fileno(meta_f));
            }
            fclose(meta_f);
        } else {
            ESP_LOGW(TAG, "Falha ao abrir arquivo de metadados (errno=%d)", errno);
            // Continua mesmo assim, pois o frame foi salvo
        }

        ESP_LOGD(TAG, "✅ Frame adicionado ao arquivo acumulativo: %d bytes", (int)frame_size);
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Falha ao adicionar frame acumulativo após %d tentativas", MAX_RETRY_ATTEMPTS);
    return ESP_FAIL;
}

size_t bsp_spiffs_get_thermal_file_size(void)
{
    if (!s_spiffs_mounted) {
        return 0;
    }

    struct stat st;
    if (stat(ACCUM_DATA_FILE, &st) != 0) {
        return 0;  // Arquivo não existe ou erro
    }

    return (size_t)st.st_size;
}

esp_err_t bsp_spiffs_read_thermal_file(void *buffer, size_t buffer_size, size_t *bytes_read)
{
    if (!s_spiffs_mounted) {
        ESP_LOGE(TAG, "SPIFFS não está montado");
        return ESP_ERR_INVALID_STATE;
    }

    if (!buffer || buffer_size == 0 || !bytes_read) {
        ESP_LOGE(TAG, "Parâmetros inválidos em read_thermal_file");
        return ESP_ERR_INVALID_ARG;
    }

    *bytes_read = 0;

    FILE *f = fopen(ACCUM_DATA_FILE, "rb");
    if (f == NULL) {
        ESP_LOGD(TAG, "Arquivo acumulativo não existe");
        return ESP_OK;  // Não é erro, apenas não há dados
    }

    size_t read = fread(buffer, 1, buffer_size, f);
    fclose(f);

    *bytes_read = read;
    ESP_LOGI(TAG, "✅ Lidos %d bytes do arquivo acumulativo", (int)read);
    return ESP_OK;
}

esp_err_t bsp_spiffs_read_thermal_metadata(time_t *timestamps, size_t max_count, size_t *count_read)
{
    if (!s_spiffs_mounted) {
        ESP_LOGE(TAG, "SPIFFS não está montado");
        return ESP_ERR_INVALID_STATE;
    }

    if (!timestamps || max_count == 0 || !count_read) {
        ESP_LOGE(TAG, "Parâmetros inválidos em read_thermal_metadata");
        return ESP_ERR_INVALID_ARG;
    }

    *count_read = 0;

    FILE *f = fopen(ACCUM_META_FILE, "rb");
    if (f == NULL) {
        ESP_LOGD(TAG, "Arquivo de metadados não existe");
        return ESP_OK;  // Não é erro, apenas não há metadados
    }

    // Calcula quantos timestamps cabem no buffer
    size_t bytes_to_read = max_count * sizeof(time_t);
    size_t bytes_read = fread(timestamps, 1, bytes_to_read, f);
    fclose(f);

    *count_read = bytes_read / sizeof(time_t);
    ESP_LOGD(TAG, "✅ Lidos %d timestamps do arquivo de metadados", (int)(*count_read));
    return ESP_OK;
}

esp_err_t bsp_spiffs_clear_thermal_file(void)
{
    if (!s_spiffs_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;

    // Remove arquivo de dados acumulativos
    if (unlink(ACCUM_DATA_FILE) != 0) {
        if (errno != ENOENT) {
            ESP_LOGW(TAG, "Falha ao remover arquivo acumulativo (errno=%d)", errno);
            ret = ESP_FAIL;
        }
    }

    // Remove arquivo de metadados
    if (unlink(ACCUM_META_FILE) != 0) {
        if (errno != ENOENT) {
            ESP_LOGW(TAG, "Falha ao remover arquivo de metadados (errno=%d)", errno);
            ret = ESP_FAIL;
        }
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Arquivo acumulativo limpo do SPIFFS");
    }

    return ret;
}

