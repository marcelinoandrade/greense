idf_component_register(
    SRCS 
    "main.c" 
    "conexoes/conexoes.c" 
    "sensores/sensores.c" 
    "sensores/aht20.c"
    "sensores/ens160.c"
    INCLUDE_DIRS "." "conexoes" "sensores"
    REQUIRES nvs_flash esp_wifi esp_event mqtt driver
)
