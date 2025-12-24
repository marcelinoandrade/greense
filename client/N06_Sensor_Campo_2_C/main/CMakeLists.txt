# Este é o CMakeLists para o componente "main"
# Arquitetura BSP/APP/GUI

idf_component_register(
    SRCS
    # Main
    "app/app_main.c"
    
    # BSP - Board Support Package
    "bsp/sensors/bsp_ds18b20.c"
    "bsp/sensors/bsp_adc.c"
    "bsp/sensors/bsp_dht11.c"
    "bsp/sensors/bsp_aht10.c"
    "bsp/sensors/bsp_bh1750.c"
    "bsp/sensors/dht.c"
    "bsp/sensors/bsp_sensors.c"
    "bsp/actuators/bsp_led.c"
    "bsp/network/bsp_wifi_ap.c"
    
    # APP - Lógica de Aplicação
    "app/app_sensor_manager.c"
    "app/app_data_logger.c"
    "app/app_sampling_period.c"
    "app/app_stats_window.c"
    "app/app_cultivation_tolerance.c"
    "app/app_atuadores.c"
    "app/gui_services.c"
    
    # GUI - Interface Gráfica
    "gui/web/gui_http_server.c"
    
    INCLUDE_DIRS
    "."
    "bsp"
    "bsp/sensors"
    "bsp/actuators"
    "bsp/network"
    "app"
    "gui"
    "gui/web"
    
    # Lista de dependências de componentes do ESP-IDF
    PRIV_REQUIRES
    
    # Dependências do Servidor Web
    "esp_wifi"
    "esp_netif"
    "esp_http_server"
    "esp_event"
    "lwip" 
    
    # Dependências dos Sensores e Log
    "esp_adc"           # Para o sensor de umidade (adc_bsp)
    "nvs_flash"         # Para o nvs_flash_init() em main.c
    "spiffs"            # Para o data_logger (esp_vfs_spiffs_register)
    "driver"            # Para GPIO e I2C (i2c_master.h está em driver)
    "esp_rom"           # Para o ds18b20 (esp_rom_sys.h)
    "esp_timer"         # Para medição de tempo em app_main.c
    "freertos"          # Para o ds18b20 e tarefas
    "vfs"               # Para esp_vfs.h
)
