idf_component_register(
  SRCS "main.c"
  REQUIRES esp_wifi esp_http_client nvs_flash driver json esp_timer
)
