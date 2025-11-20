#include "bsp_sdcard.h"
#include "bsp_pins.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define TAG "BSP_SDCARD"

static bool s_sdcard_mounted = false;

esp_err_t bsp_sdcard_init(void)
{
    ESP_LOGI(TAG, "Inicializando cartão SD...");
    
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    
    // Configuração para ESP32-S3
    slot_config.width = 1;  // 1-bit mode (pode tentar 4-bit se suportado)
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    
    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, 
                                            &mount_config, &card);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Falha ao montar sistema de arquivos. Verifique se o cartão está formatado.");
        } else {
            ESP_LOGE(TAG, "Falha ao inicializar cartão SD: %s", esp_err_to_name(ret));
        }
        s_sdcard_mounted = false;
        return ret;
    }
    
    s_sdcard_mounted = true;
    ESP_LOGI(TAG, "Cartão SD montado com sucesso em %s", SD_MOUNT_POINT);
    
    // Lista arquivos no diretório raiz
    ESP_LOGI(TAG, "Listando arquivos em %s:", SD_MOUNT_POINT);
    DIR* dir = opendir(SD_MOUNT_POINT);
    if (dir != NULL) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            ESP_LOGI(TAG, "  %s", entry->d_name);
        }
        closedir(dir);
    }
    
    // Teste de escrita
    FILE* f = fopen(SD_MOUNT_POINT "/teste.txt", "w");
    if (f) {
        fprintf(f, "Teste de escrita no cartão SD.\n");
        fclose(f);
        ESP_LOGI(TAG, "Teste de escrita: OK");
    } else {
        ESP_LOGE(TAG, "Teste de escrita: FALHOU");
    }
    
    return ESP_OK;
}

esp_err_t bsp_sdcard_save_file(const char* filename, const uint8_t* data, size_t data_len)
{
    if (!s_sdcard_mounted) {
        ESP_LOGE(TAG, "SD Card não está montado");
        return ESP_ERR_INVALID_STATE;
    }
    
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, filename);
    
    ESP_LOGI(TAG, "Salvando arquivo: %s (%d bytes)", full_path, data_len);
    
    FILE* f = fopen(full_path, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir arquivo para escrita");
        return ESP_FAIL;
    }
    
    size_t bytes_written = fwrite(data, 1, data_len, f);
    fclose(f);
    
    if (bytes_written == data_len) {
        ESP_LOGI(TAG, "Arquivo salvo com sucesso: %d bytes", bytes_written);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Erro ao escrever arquivo. Escritos: %d, Esperados: %d", 
                 bytes_written, data_len);
        return ESP_FAIL;
    }
}

bool bsp_sdcard_is_mounted(void)
{
    return s_sdcard_mounted;
}

