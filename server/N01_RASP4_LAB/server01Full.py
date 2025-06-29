import paho.mqtt.client as mqtt
import json
from influxdb import InfluxDBClient
from flask import Flask, request, jsonify
from flask_cors import CORS
from datetime import datetime
import threading
import config
import os
from flask import send_from_directory


# Configuração do InfluxDB
INFLUXDB_HOST = "localhost"
INFLUXDB_PORT = 8086
INFLUXDB_DB = "dados_estufa"

# Senha para inserção manual
SENHA_CORRETA = config.PASS_MANUAL

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
    
@app.route("/imagem")
def serve_ultima_imagem():
    host = request.headers.get("Host", "").split(":")[0]

    if "camera02" in host:
        diretorio_fotos = "fotos_recebidas/cam_02"
    elif "camera" in host:
        diretorio_fotos = "fotos_recebidas/cam_01"
    else:
        return jsonify({"erro": f"Domínio inválido ou não reconhecido: {host}"}), 400

    caminho = os.path.join(diretorio_fotos, "ultima.jpg")

    if not os.path.exists(caminho):
        return jsonify({"erro": f"Imagem não disponível para {host}"}), 404

    return send_from_directory(diretorio_fotos, "ultima.jpg")



# === Endpoint para recebimento de imagem via POST ===
@app.route("/upload", methods=["POST"])
def upload_foto():
    try:
        host = request.headers.get("Host", "").split(":")[0]
        if "camera02" in host:
            camera_id = "cam_02"
        elif "camera" in host:
            camera_id = "cam_01"
        else:
            return jsonify({"erro": f"❌ camera_id ausente ou host inválido: {host}"}), 400

        diretorio_fotos = os.path.join("fotos_recebidas", camera_id)
        os.makedirs(diretorio_fotos, exist_ok=True)

        conteudo = request.data
        print(f"📩 Recebido {len(conteudo)} bytes de imagem para {camera_id} via host {host}")

        if not conteudo:
            return jsonify({"erro": "Imagem vazia"}), 400

        nome_arquivo = datetime.now().strftime(f"{camera_id}_%Y%m%d_%H%M%S.jpg")
        caminho = os.path.join(diretorio_fotos, nome_arquivo)

        with open(caminho, "wb") as f:
            f.write(conteudo)

        link_fixo = os.path.join(diretorio_fotos, "ultima.jpg")
        try:
            if os.path.exists(link_fixo) or os.path.islink(link_fixo):
                os.remove(link_fixo)
            os.symlink(os.path.abspath(caminho), link_fixo)
        except Exception as e:
            print(f"⚠️ Erro ao criar symlink: {e}")

        # Limpa fotos antigas
        for filename in os.listdir(diretorio_fotos):
            if filename in [os.path.basename(caminho), "ultima.jpg"]:
                continue
            file_path = os.path.join(diretorio_fotos, filename)
            try:
                if os.path.isfile(file_path):
                    os.unlink(file_path)
            except Exception as e:
                print(f"⚠️ Erro ao apagar {filename}: {e}")

        print(f"📷 Imagem salva em: {caminho}")
        return jsonify({"status": "ok", "arquivo": nome_arquivo, "camera": camera_id})

    except Exception as e:
        print(f"❌ Erro no upload: {e}")
        return jsonify({"erro": str(e)}), 500


# === MQTT CALLBACK ===
def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        json_body = []
        dispositivo = "desconhecido"

        if msg.topic == "estufa/germinar":
            dispositivo = "Estufa_Germinar"
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
                        "umid_solo_pct": data.get("umid_solo_pct", 0),  # <-- novo campo
                        "umid_solo_raw": data.get("umid_solo_raw", 0)    # <-- opcional
                    }
                }
            ]

        elif msg.topic == "estufa/maturar":
            dispositivo = "Estufa_Maturar"
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
                        "temp_ext": data.get("temp_externa", 0),  # <-- novo campo
                        "umid_ext": data.get("umid_externa", 0)    # <-- opcional
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
    client_mqtt.subscribe("estufa/germinar")
    client_mqtt.subscribe("estufa/maturar")
    client_mqtt.subscribe("estufa3/esp32")

    print("Servidor MQTT rodando... Aguardando mensagens.")
    client_mqtt.loop_forever()

if __name__ == "__main__":
    threading.Thread(target=start_mqtt).start()
    app.run(host="0.0.0.0", port=5000)