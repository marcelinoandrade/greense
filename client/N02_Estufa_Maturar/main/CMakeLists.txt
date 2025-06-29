idf_component_register(
    SRCS 
    "main.c" 
    "conexoes/conexoes.c" 
    "sensores/sensores.c" 
    "sensores/aht20.c"
    "sensores/ens160.c"
    "sensores/ds18b20.c"
    "sensores/dht.c" 
    "atuadores/atuadores.c"

    INCLUDE_DIRS "." "conexoes" "sensores" "atuadores"
    REQUIRES nvs_flash esp_wifi esp_event mqtt driver led_strip
    EMBED_TXTFILES "certs/greense_cert.pem"
)
