# Este é o CMakeLists para o componente "main"

idf_component_register(
    SRCS
    # Fontes principais e de log
    "main.c"
    "libs/data_logger.c"
    
    # Fontes dos Sensores
    "sensores/sensores.c"
    "sensores/ds18b20.c"
    "sensores/soil_moisture.c"

    # NOVA FONTE: O servidor web
    "libs/http_server.c"

    INCLUDE_DIRS
    "."
    "sensores"
    "libs"

    # Lista de dependências de componentes do ESP-IDF
    PRIV_REQUIRES
    
    # Dependências do Servidor Web
    "esp_wifi"
    "esp_netif"
    "esp_http_server"
    "esp_event"
    "lwip" 

    # Dependências dos Sensores e Log (Corrigidas)
    "esp_adc"           # Para o sensor de umidade (soil_moisture)
    "nvs_flash"         # Para o nvs_flash_init() em main.c
    "spiffs"            # Para o data_logger (esp_vfs_spiffs_register)
    "driver"            # Para GPIO (ao invés de esp_driver_gpio)
    "esp_rom"           # Para o ds18b20 (esp_rom_sys.h)
    "freertos"          # Para o ds18b20 (freertos/task.h)
    "vfs"               # CORREÇÃO: Para esp_vfs.h no soil_moisture.c
)

