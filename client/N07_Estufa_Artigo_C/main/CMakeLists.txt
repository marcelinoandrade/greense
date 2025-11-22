idf_component_register(
    SRCS 
    "main.c"
    "bsp/bsp_gpio.c"
    "bsp/bsp_wifi.c"
    "bsp/bsp_camera.c"
    "bsp/bsp_sdcard.c"
    "bsp/bsp_spiffs.c"
    "bsp/bsp_uart.c"
    "app/app_main.c"
    "app/app_http.c"
    "app/app_thermal.c"
    "app/app_time.c"
    "gui/gui_led.c"
    
    INCLUDE_DIRS "." "bsp" "app" "gui"
    REQUIRES nvs_flash esp_wifi esp_event esp_netif esp_http_client driver 
             esp32-camera fatfs sdmmc led_strip lwip spiffs
    EMBED_TXTFILES "certs/greense_cert.pem"
)

