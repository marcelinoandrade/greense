idf_component_register(
  SRCS "main.c"
  INCLUDE_DIRS "."
  REQUIRES esp32-camera esp_http_server esp_wifi nvs_flash esp_http_client
  EMBED_TXTFILES "certs/greense_cert.pem" 
)
