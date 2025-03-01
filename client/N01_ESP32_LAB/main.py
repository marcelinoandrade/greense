from config import Config
from conexao import Conexao
from sensores import SensorManager
from atuadores import ActuatorManager
import json
import time
import machine

class MainApp:
    def __init__(self):
        self.config = Config()  # Instancia a classe de configurações
        self.conexao = Conexao(self.config)  # Instancia a classe Conexao
        self.sensor_manager = SensorManager()
        self.actuator_manager = ActuatorManager(self.config)

    def run(self):
        # Conecta ao Wi-Fi
        while not self.conexao.conectar_wifi():
            print("Tentando reconectar ao Wi-Fi...")
            time.sleep(10)

        # Conecta ao MQTT
        while not self.conexao.conectar_mqtt():
            print("Tentando reconectar ao MQTT...")
            time.sleep(5)

        # Loop principal
        while True:
            try:
                # Verifica conexão Wi-Fi
                if not self.conexao.wifi_conectado():
                    print("Wi-Fi perdido. Tentando reconectar...")
                    if not self.conexao.conectar_wifi():
                        print("Falha na reconexão. Reiniciando ESP32...")
                        machine.reset()

                # Verifica conexão MQTT
                if not self.conexao.verificar_conexao_mqtt():
                    if not self.conexao.conectar_mqtt():
                        print("Falha ao reconectar MQTT. Reiniciando ESP32...")
                        machine.reset()

                # Lê sensores
                sensor_data = self.sensor_manager.read_sensors()
                json_data = json.dumps(sensor_data)

                # Publica dados no MQTT
                if not self.conexao.publicar_mqtt(json_data):
                    print("Falha ao publicar dados.")

                # Controla atuadores (se necessário)
                self.actuator_manager.control_actuators(sensor_data)

            except Exception as e:
                print("Erro no loop principal:", e)

            time.sleep(self.config.sensor_read_interval)  # Usa o intervalo da configuração

# Função main
def main():
    app = MainApp()
    app.run()

# Executa o programa
if __name__ == "__main__":
    main()