from umqtt.simple import MQTTClient
import network
import time
import json
import random
import machine  # Para reiniciar o ESP32 se necessário

# Configuração Wi-Fi do AP do Raspberry Pi
SSID = "greense"
PASSWORD = "greense@3141"

# Configuração do MQTT
MQTT_BROKER = "10.42.0.1"  # IP do Raspberry Pi no modo AP
MQTT_TOPIC = "estufa1/dados"
CLIENT_ID = "ESP32_Estufa"

# Inicializa Wi-Fi
def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    
    if not wlan.isconnected():
        print("Conectando ao AP do Raspberry Pi...")
        wlan.connect(SSID, PASSWORD)

        timeout = 20  # Tempo máximo para conectar (segundos)
        while not wlan.isconnected() and timeout > 0:
            time.sleep(1)
            timeout -= 1
            print(f"Tentando conectar ao AP... {timeout}s restantes")

    if wlan.isconnected():
        print("Wi-Fi conectado ao AP!", wlan.ifconfig())
        return True
    else:
        print("Falha ao conectar ao AP. Reiniciando Wi-Fi...")
        wlan.active(False)  # Desativa Wi-Fi
        time.sleep(2)
        wlan.active(True)   # Reativa Wi-Fi
        return False  # Indica falha

# Inicializa MQTT
def connect_mqtt():
    try:
        client = MQTTClient(CLIENT_ID, MQTT_BROKER, keepalive=60)  # Mantém a conexão ativa
        client.connect()
        print("Conectado ao MQTT Broker!")
        return client
    except Exception as e:
        print("Erro ao conectar ao MQTT:", e)
        time.sleep(5)
        return None  # Retorna None se falhar

# Verifica conexão MQTT
def check_mqtt_connection(client):
    try:
        if client:
            client.ping()  # Verifica se ainda está conectado
            return True
    except:
        print("Conexão MQTT perdida. Tentando reconectar...")
    return False

# Loop principal
def main():
    while not connect_wifi():  # Garante que está conectado antes de continuar
        time.sleep(10)  # Aguarda antes de tentar novamente

    client = connect_mqtt()
    while client is None:  # Se falhar, tenta novamente
        time.sleep(5)
        client = connect_mqtt()

    while True:
        try:
            wlan = network.WLAN(network.STA_IF)

            # Se perder conexão Wi-Fi, tenta reconectar
            if not wlan.isconnected():
                print("Wi-Fi perdido. Tentando reconectar...")
                if not connect_wifi():
                    print("Falha na reconexão. Reiniciando ESP32...")
                    machine.reset()

            # Se perder conexão MQTT, tenta reconectar
            if not check_mqtt_connection(client):
                client = connect_mqtt()
                if client is None:
                    print("Falha ao reconectar MQTT. Reiniciando ESP32...")
                    machine.reset()

            # Simulando valores de sensores
            temperatura = round(random.uniform(20, 35), 2)
            umidade = round(random.uniform(40, 80), 2)
            umidade_solo = round(random.uniform(10, 90), 2)
            luz = round(random.uniform(100, 1000), 2)

            dados = {
                "temperatura": temperatura,
                "umidade": umidade,
                "umidade_solo": umidade_solo,
                "luz": luz
            }

            json_dados = json.dumps(dados)
            client.publish(MQTT_TOPIC, json_dados)
            print(f"Publicado: {json_dados}")

        except Exception as e:
            print("Erro ao enviar dados:", e)

        time.sleep(5)  # Enviar dados a cada 5 segundos

# Executa o programa automaticamente
if __name__ == "__main__":
    main()
