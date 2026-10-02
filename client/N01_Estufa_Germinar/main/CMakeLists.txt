idf_component_register(
    SRCS 
    "main.c" 
    "conexoes/conexoes.c" 
    "sensores/sensores.c"
    "sensores/dht11.c"
    "sensores/ds18b20.c"
    "atuadores/atuadores.c"

    INCLUDE_DIRS "." "conexoes" "sensores" "atuadores"
    REQUIRES nvs_flash esp_wifi esp_event mqtt driver led_strip esp_adc esp_timer
    EMBED_TXTFILES "certs/greense_cert.pem"
)
