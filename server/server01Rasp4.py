import paho.mqtt.client as mqtt
import json
from influxdb import InfluxDBClient

# Configuração do InfluxDB
INFLUXDB_HOST = "localhost"
INFLUXDB_PORT = 8086
INFLUXDB_DB = "dados_estufa"

# Conectar ao InfluxDB
client_influx = InfluxDBClient(host=INFLUXDB_HOST, port=INFLUXDB_PORT)
client_influx.create_database(INFLUXDB_DB)  # Cria o banco caso não exista
client_influx.switch_database(INFLUXDB_DB)

# Callback quando recebe mensagem MQTT
def on_message(client, userdata, msg):
    data = json.loads(msg.payload.decode())

    # Estrutura dos dados para InfluxDB
    json_body = [
        {
            "measurement": "sensores",
            "tags": {"estufa": "estufa1"},
            "fields": {
                "temperatura": data["temperatura"],
                "umidade": data["umidade"],
                "umidade_solo": data["umidade_solo"],
                "luz": data["luz"]
            }
        }
    ]

    # Inserir no InfluxDB
    client_influx.write_points(json_body)
    print(f"Dado salvo no InfluxDB: {data}")

# Configuração do cliente MQTT
client_mqtt = mqtt.Client()
client_mqtt.on_message = on_message
client_mqtt.connect("localhost", 1883, 60)

# Inscrever-se no tópico MQTT
client_mqtt.subscribe("estufa1/dados")
client_mqtt.loop_forever()
