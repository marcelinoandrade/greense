import network
import time
from umqtt.simple import MQTTClient

class Conexao:
    def __init__(self, config):
        self.config = config  # Recebe a instância de Config
        self.wlan = network.WLAN(network.STA_IF)
        self.mqtt_client = None

    # Métodos para Wi-Fi
    def conectar_wifi(self):
        """
        Tenta conectar ao Wi-Fi. Retorna True se conseguir, False caso contrário.
        """
        self.wlan.active(True)  # Ativa a interface Wi-Fi
        
        if not self.wlan.isconnected():
            print("Conectando ao Wi-Fi...")
            self.wlan.connect(self.config.ssid, self.config.password)  # Usa ssid e password de Config

            timeout = 20  # Tempo máximo para conectar (segundos)
            while not self.wlan.isconnected() and timeout > 0:
                time.sleep(1)
                timeout -= 1
                print(f"Tentando conectar ao Wi-Fi... {timeout}s restantes")

        if self.wlan.isconnected():
            print("Wi-Fi conectado!", self.wlan.ifconfig())
            return True
        else:
            print("Falha ao conectar ao Wi-Fi. Reiniciando Wi-Fi...")
            self.wlan.active(False)  # Desativa a interface Wi-Fi
            time.sleep(2)
            self.wlan.active(True)   # Reativa a interface Wi-Fi
            return False  # Indica falha

    def wifi_conectado(self):
        """
        Verifica se o Wi-Fi está conectado.
        Retorna True se estiver conectado, False caso contrário.
        """
        return self.wlan.isconnected()

    # Métodos para MQTT
    def conectar_mqtt(self):
        """
        Tenta conectar ao broker MQTT. Retorna True se conseguir, False caso contrário.
        """
        try:
            self.mqtt_client = MQTTClient(self.config.client_id, self.config.mqtt_broker, keepalive=self.config.mqtt_keepalive)
            self.mqtt_client.connect()
            print("Conectado ao MQTT Broker!")
            return True
        except Exception as e:
            print("Erro ao conectar ao MQTT:", e)
            return False

    def publicar_mqtt(self, data):
        """
        Publica dados no tópico MQTT.
        Retorna True se conseguir, False caso contrário.
        """
        if self.mqtt_client:
            try:
                self.mqtt_client.publish(self.config.mqtt_topic, data)
                print(f"Publicado: {data}")
                return True
            except Exception as e:
                print("Erro ao publicar dados:", e)
                return False
        return False

    def verificar_conexao_mqtt(self):
        """
        Verifica se a conexão MQTT está ativa.
        Retorna True se estiver conectado, False caso contrário.
        """
        try:
            if self.mqtt_client:
                self.mqtt_client.ping()
                return True
        except:
            print("Conexão MQTT perdida.")
            return False