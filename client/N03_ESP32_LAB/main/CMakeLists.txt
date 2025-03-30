idf_component_register(
    SRCS "main.c" "conexao.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_event mqtt
)
