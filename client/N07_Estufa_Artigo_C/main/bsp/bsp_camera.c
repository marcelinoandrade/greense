#include "bsp_camera.h"
#include "bsp_pins.h"
#include "esp_camera.h"
#include "esp_log.h"

#define TAG "BSP_CAMERA"

esp_err_t bsp_camera_init(void)
{
    ESP_LOGI(TAG, "Inicializando câmera ESP32-S3...");
    
    camera_config_t config = {
        .pin_pwdn       = CAM_PWDN_GPIO,
        .pin_reset      = CAM_RESET_GPIO,
        .pin_xclk       = CAM_XCLK_GPIO,
        .pin_sscb_sda   = CAM_SIOD_GPIO,
        .pin_sscb_scl   = CAM_SIOC_GPIO,
        .pin_d7         = CAM_Y9_GPIO,
        .pin_d6         = CAM_Y8_GPIO,
        .pin_d5         = CAM_Y7_GPIO,
        .pin_d4         = CAM_Y6_GPIO,
        .pin_d3         = CAM_Y5_GPIO,
        .pin_d2         = CAM_Y4_GPIO,
        .pin_d1         = CAM_Y3_GPIO,
        .pin_d0         = CAM_Y2_GPIO,
        .pin_vsync      = CAM_VSYNC_GPIO,
        .pin_href       = CAM_HREF_GPIO,
        .pin_pclk       = CAM_PCLK_GPIO,
        .xclk_freq_hz   = 20000000,  // 20MHz
        .ledc_timer     = LEDC_TIMER_0,
        .ledc_channel   = LEDC_CHANNEL_0,
        .pixel_format   = PIXFORMAT_JPEG,
        .frame_size     = FRAMESIZE_XGA,  // 1024x768
        .jpeg_quality   = 12,  // 0-63, menor = melhor qualidade
        .fb_count       = 1,
        .grab_mode      = CAMERA_GRAB_WHEN_EMPTY,
        .fb_location     = CAMERA_FB_IN_PSRAM,
        .sccb_i2c_port  = 0
    };

    // Inicializa a câmera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao inicializar câmera: 0x%x (%s)", err, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Câmera inicializada com sucesso");
    return ESP_OK;
}

camera_fb_t* bsp_camera_capture(void)
{
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Falha ao capturar imagem");
    } else {
        ESP_LOGI(TAG, "Imagem capturada: %dx%d, %d bytes", 
                 fb->width, fb->height, fb->len);
    }
    return fb;
}

void bsp_camera_release(camera_fb_t* fb)
{
    if (fb) {
        esp_camera_fb_return(fb);
    }
}

