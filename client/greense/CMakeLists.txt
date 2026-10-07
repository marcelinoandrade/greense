idf_component_register(
    SRCS
        "src/greense_app.c"
        "src/conexoes.c"
        "src/sensores.c"
        "src/dht11.c"
        "src/ds18b20.c"
        "src/atuadores.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES nvs_flash esp_wifi esp_event mqtt driver led_strip esp_adc esp_timer
    EMBED_TXTFILES "certs/greense_cert.pem"
)
