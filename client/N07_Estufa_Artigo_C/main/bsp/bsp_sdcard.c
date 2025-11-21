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

