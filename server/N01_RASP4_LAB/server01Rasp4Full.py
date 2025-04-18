import paho.mqtt.client as mqtt
import json
from influxdb import InfluxDBClient
from flask import Flask, request, jsonify
from flask_cors import CORS
from datetime import datetime
import threading

# Configuração do InfluxDB
INFLUXDB_HOST = "localhost"
INFLUXDB_PORT = 8086
INFLUXDB_DB = "dados_estufa"

# Senha para inserção manual
SENHA_CORRETA = "greense2025"

# Conectar ao InfluxDB
client_influx = InfluxDBClient(host=INFLUXDB_HOST, port=INFLUXDB_PORT)
client_influx.create_database(INFLUXDB_DB)
client_influx.switch_database(INFLUXDB_DB)

# === FLASK PARA INSERÇÃO MANUAL ===
app = Flask(__name__)
CORS(app)

from datetime import datetime

@app.route("/insere", methods=["POST"])
def insere_manual():
    dados = request.json
    if dados.get("senha") != SENHA_CORRETA:
        return jsonify({"erro": "não autorizado"}), 403

    try:
        ph = float(dados.get("ph", 0))
        ec = float(dados.get("ec", 0))
        timestamp = int(datetime.utcnow().timestamp() * 1e9)  # nanossegundos

        json_body = [{
            "measurement": "sensores",
            "tags": {"dispositivo": "ESP32_E3"},
            "time": timestamp,
            "fields": {"ph": ph, "ec": ec}
        }]
        client_influx.write_points(json_body, time_precision='n')
        return jsonify({"status": "ok", "ph": ph, "ec": ec})
    except Exception as e:
        return jsonify({"erro": str(e)}), 500


# === MQTT CALLBACK ===
def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        json_body = []
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
                    "fields": {"bomba_agua": data.get("bomba_agua", 0)}
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
                        "temp": data.get("temp", 0),
                        "umid": data.get("umid", 0),
                        "co2": data.get("co2", 0),
                        "luz": data.get("luz", 0),
                        "agua_min": float(data.get("agua_min", 0)),
                        "agua_max": float(data.get("agua_max", 0)),
                        "temp_reserv_int": data.get("temp_reserv_int", 0),
                        "ph": data.get("ph", 0),
                        "ec": data.get("ec", 0),
                        "temp_reserv_ext": data.get("temp_reserv_ext", 0),
                    }
                }
            ]

        elif msg.topic == "estufa4/uno":
            dispositivo = "UNO_E4"
            json_body = [
                {
                    "measurement": "sensores",
                    "tags": {"dispositivo": dispositivo},
                    "fields": {
                        "painel": float(data.get("painel", 0)),
                        "exaustor": float(data.get("exaustor", 0)),
                        "temp": float(data.get("temp", 0)),
                        "umid": float(data.get("umid", 0)),
                        "temp_solo": float(data.get("temp_solo", 0)),
                        "umid_solo": float(data.get("umid_solo", 0)),
                        "boia_baixa": float(data.get("boia_baixa", 0)),
                        "boia_alta": float(data.get("boia_alta", 0)),
                        "bomba_baixa": float(data.get("bomba_baixa", 0)),
                        "bomba_alta": float(data.get("bomba_alta", 0))
                    }
                }
            ]

        else:
            print(f"⚠️ Tópico desconhecido: {msg.topic}")
            return

        client_influx.write_points(json_body)
        print(f"Dado salvo ({dispositivo}) no InfluxDB: {data}")

    except Exception as e:
        print(f"Erro ao processar mensagem MQTT: {e}")

# === INICIALIZAÇÃO DO MQTT E FLASK ===
def start_mqtt():
    client_mqtt = mqtt.Client()
    client_mqtt.on_message = on_message
    client_mqtt.connect("localhost", 1883, 60)

    client_mqtt.subscribe("estufa1/esp32")
    client_mqtt.subscribe("estufa1/esp8266")
    client_mqtt.subscribe("estufa3/esp32")
    client_mqtt.subscribe("estufa4/uno")

    print("Servidor MQTT rodando... Aguardando mensagens.")
    client_mqtt.loop_forever()

if __name__ == "__main__":
    threading.Thread(target=start_mqtt).start()
    app.run(host="0.0.0.0", port=5000)
