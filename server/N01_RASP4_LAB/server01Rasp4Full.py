import paho.mqtt.client as mqtt
import json
from influxdb import InfluxDBClient

# Configuração do InfluxDB
INFLUXDB_HOST = "localhost"
INFLUXDB_PORT = 8086
INFLUXDB_DB = "dados_estufa"

# Conectar ao InfluxDB
client_influx = InfluxDBClient(host=INFLUXDB_HOST, port=INFLUXDB_PORT)
client_influx.create_database(INFLUXDB_DB)  # Cria o banco se não existir
client_influx.switch_database(INFLUXDB_DB)

# Callback quando recebe mensagem MQTT
def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        dispositivo = "desconhecido"

        if msg.topic == "estufa1/esp32":
            dispositivo = "ESP32_E1"   

            json_body = [
                {
                    "measurement": "sensores",
                    "tags": {"dispositivo": dispositivo},
                    "fields": {
                        "temp": data.get("temp", 0),
                        "umid": data.get("umid", 0),
                        "co2": data.get("co2", 0),
                        "luz": data.get("luz", 0),
                        "agua_min": data.get("agua_min", 0),
                        "agua_max": data.get("agua_max", 0),
                        "temp_reserv_int": data.get("temp_reserv_int", 0),
                        "ph": data.get("ph", 0),
                        "ec": data.get("ec", 0),
                        "temp_reserv_ext": data.get("temp_reserv_ext", 0),
                    }
                },
                {
                    "measurement": "atuadores",
                    "tags": {"dispositivo": dispositivo},
                    "fields": {
                        "bomba_agua": data.get("bomba_agua", 0)
                    }
                }
            ]            

        elif msg.topic == "estufa1/esp8266":
            dispositivo = "ESP8266_E2"

            json_body = [
                {
                    "measurement": "sensores",
                    "tags": {"dispositivo": dispositivo},
                    "fields": {
                        "temperatura": data.get("temperatura", 0),
                        "umidade": data.get("umidade", 0),
                        "umidade_solo": data.get("umidade_solo", 0),
                        "luz": data.get("luz", 0)
                    }
                }
            ]

        elif msg.topic == "estufa3/esp32":
            dispositivo = "ESP32_E3"

            json_body = [
                {
                    "measurement": "sensores",
                    "tags": {"dispositivo": dispositivo},
                    "fields": {
                        "temp": data.get("temp", 0)
                    }
                }
            ]

        else:
            print(f"⚠️ Tópico desconhecido: {msg.topic}")
            return

        # Inserir no InfluxDB
        client_influx.write_points(json_body)
        print(f"Dado salvo ({dispositivo}) no InfluxDB: {data}")

    except Exception as e:
        print(f"Erro ao processar mensagem MQTT: {e}")

# Configuração do cliente MQTT
client_mqtt = mqtt.Client()
client_mqtt.on_message = on_message
client_mqtt.connect("localhost", 1883, 60)

# Inscrevendo-se nos tópicos dos dispositivos
client_mqtt.subscribe("estufa1/esp32")
client_mqtt.subscribe("estufa1/esp8266")
client_mqtt.subscribe("estufa3/esp32")  # <-- novo dispositivo adicionado

print("Servidor MQTT rodando... Aguardando mensagens.")
client_mqtt.loop_forever()
