idf_component_register(
    SRCS 
    "main.c" 
    "sensores/sensores.c" 
    "sensores/ds18b20.c"
    "sensores/soil_moisture.c"
    "libs/data_logger.c"

    INCLUDE_DIRS 
    "." 
    "sensores" 
    "libs"

    # Correção: Componentes corrigidos
    PRIV_REQUIRES
    "esp_adc"           # Para o sensor de umidade (soil_moisture)
    "nvs_flash"         # Para o nvs_flash_init() em main.c
    "spiffs"            # Para o data_logger (esp_vfs_spiffs_register)
    "driver"            # Para GPIO (ao invés de esp_driver_gpio)
    "esp_rom"           # Para o ds18b20 (esp_rom_sys.h)
    "freertos"          # Para o ds18b20 (freertos/task.h)
    "vfs"               # CORREÇÃO: Para esp_vfs.h no soil_moisture.c
)

