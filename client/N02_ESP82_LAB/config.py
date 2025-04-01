class Config:
    def __init__(self):
        # Configurações de Wi-Fi
        self.ssid = "XXXXXX"
        self.password = "XXXXXXX"

        # Configurações de MQTT
        self.mqtt_broker = "10.42.0.1"
        self.mqtt_topic = "estufa1/esp8266"
        self.client_id = "ESP32_Estufa"
        self.mqtt_keepalive = 60

        # Configurações de sensores
        self.sensor_read_interval = 5  # Intervalo de leitura dos sensores em segundos

        # Configurações de atuadores (se necessário)
        self.actuator_pin = 25  # Exemplo: Pino do atuador no ESP32