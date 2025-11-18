idf_component_register(
  SRCS 
    "main.c"
    "bsp/bsp_gpio.c"
    "bsp/bsp_uart.c"
    "bsp/bsp_wifi.c"
    "app/app_main.c"
    "app/app_thermal.c"
    "app/app_http.c"
    "app/app_time.c"
    "gui/gui_led.c"
  INCLUDE_DIRS "."
  REQUIRES esp_wifi esp_http_client nvs_flash driver json esp_timer lwip
  EMBED_TXTFILES "certs/greense_cert.pem"
)
