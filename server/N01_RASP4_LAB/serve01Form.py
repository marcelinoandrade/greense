from flask import Flask, request, jsonify
from influxdb import InfluxDBClient

app = Flask(__name__)

# mesma config do seu script atual
INFLUXDB_HOST = "localhost"
INFLUXDB_PORT = 8086
INFLUXDB_DB = "dados_estufa"

client_influx = InfluxDBClient(host=INFLUXDB_HOST, port=INFLUXDB_PORT)
client_influx.switch_database(INFLUXDB_DB)

SENHA_CORRETA = "greense2025"

@app.route("/insere", methods=["POST"])
def insere_manual():
    dados = request.json
    if dados.get("senha") != SENHA_CORRETA:
        return jsonify({"erro": "não autorizado"}), 403

    ph = float(dados.get("ph", 0))
    ec = float(dados.get("ec", 0))

    json_body = [{
        "measurement": "sensores",
        "tags": {"dispositivo": "ESP32_E3"},
        "fields": {
            "ph": ph,
            "ec": ec
        }
    }]

    client_influx.write_points(json_body)
    return jsonify({"status": "ok", "ph": ph, "ec": ec})

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
