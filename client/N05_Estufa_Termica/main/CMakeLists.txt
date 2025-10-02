idf_component_register(
    SRCS 
    "main.c" 
    "conexoes/conexoes.c" 
    INCLUDE_DIRS "." "conexoes"
    REQUIRES 
        nvs_flash 
        esp_wifi 
        esp_event 
        mqtt 
        driver
        esp_netif
    EMBED_TXTFILES "certs/greense_cert.pem"
)