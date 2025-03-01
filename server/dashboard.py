from flask import Flask, render_template
from flask_socketio import SocketIO
from influxdb import InfluxDBClient
import time

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*")  # Permite conexões externas

# Conectar ao InfluxDB
INFLUXDB_HOST = "localhost"
INFLUXDB_PORT = 8086
INFLUXDB_DB = "dados_estufa"
client_influx = InfluxDBClient(host=INFLUXDB_HOST, port=INFLUXDB_PORT)
client_influx.switch_database(INFLUXDB_DB)

@app.route('/')
def index():
    return render_template("index_socket.html")

def enviar_dados():
    while True:
        query = "SELECT * FROM sensores ORDER BY time DESC LIMIT 10"
        result = client_influx.query(query)
        pontos = list(result.get_points())

        # Extrair os valores relevantes
        dados = [
            [p["temperatura"], p["umidade"], p["umidade_solo"], p["luz"], p["time"]]
            for p in pontos
        ]

        print("Enviando dados:", dados)  # Debug
        socketio.emit('atualizacao', dados)  # Envia para o front-end
        time.sleep(5)  # Atualiza a cada 5 segundos

@socketio.on('connect')
def handle_connect():
    print("Cliente conectado!")

if __name__ == '__main__':
    socketio.start_background_task(enviar_dados)  # Inicia o envio contínuo de dados
    socketio.run(app, host='0.0.0.0', port=5000, debug=True, allow_unsafe_werkzeug=True)
