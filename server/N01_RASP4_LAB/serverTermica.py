import os
import csv
import json
import threading
from datetime import datetime

import numpy as np
from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
import paho.mqtt.client as mqtt
from influxdb import InfluxDBClient

import config  # precisa ter PASS_MANUAL


# =========================
# CONFIGURAÇÃO GERAL
# =========================

INFLUXDB_HOST = "localhost"
INFLUXDB_PORT = 8086
INFLUXDB_DB = "dados_estufa"
SENHA_CORRETA = config.PASS_MANUAL

ARQUIVO_CSV_GLOBAL = "historico_tempo/termica_global.csv"

# conecta InfluxDB
client_influx = InfluxDBClient(host=INFLUXDB_HOST, port=INFLUXDB_PORT)
client_influx.create_database(INFLUXDB_DB)
client_influx.switch_database(INFLUXDB_DB)

# Flask
app = Flask(__name__)
CORS(app)


# =========================
# FUNÇÕES AUXILIARES
# =========================

def salvar_dados_termicos_influxdb(temperaturas, timestamp):
    """
    Salva estatísticas térmicas no InfluxDB.
    temperaturas: np.array 24x32
    timestamp: int (epoch seconds)
    """
    try:
        temp_min = np.min(temperaturas)
        temp_max = np.max(temperaturas)
        temp_avg = np.mean(temperaturas)
        temp_std = np.std(temperaturas)

        json_body = [{
            "measurement": "termica",
            "tags": {"dispositivo": "Camera_Termica"},
            "time": timestamp * 1000000000,  # nanossegundos
            "fields": {
                "temp_min": float(temp_min),
                "temp_max": float(temp_max),
                "temp_avg": float(temp_avg),
                "temp_std": float(temp_std),
                "temp_00_00": float(temperaturas[0][0]),
                "temp_12_16": float(temperaturas[12][16]),
                "temp_23_31": float(temperaturas[23][31])
            }
        }]

        client_influx.write_points(json_body)
        print(f"📈 Estatísticas térmicas salvas no InfluxDB")
        print(f"   📊 Temperaturas: min={temp_min:.1f}°C, max={temp_max:.1f}°C, avg={temp_avg:.1f}°C")
        return True

    except Exception as e:
        print(f"❌ Erro ao salvar no InfluxDB: {e}")
        return False


def atualizar_csv_termico_incremental(temperaturas, timestamp):
    """
    Mantém um CSV acumulado.
    Cada linha é um pixel fixo (Linha, Coluna).
    Cada nova captura vira uma nova coluna com nome timestamp.

    Formato final:
    Linha,Coluna,20251027_110501,20251027_110732,...
    0,0,23.8,24.1,...
    ...

    temperaturas: np.array shape (24,32)
    timestamp: int epoch seconds
    """
    diretorio = os.path.dirname(ARQUIVO_CSV_GLOBAL)
    os.makedirs(diretorio, exist_ok=True)

    dt = datetime.fromtimestamp(timestamp)
    nome_coluna = dt.strftime("%Y%m%d_%H%M%S")

    flat_temp = temperaturas.reshape(24 * 32)

    # caso CSV ainda não exista
    if not os.path.exists(ARQUIVO_CSV_GLOBAL):
        with open(ARQUIVO_CSV_GLOBAL, "w", newline="") as f:
            writer = csv.writer(f)
            # cabeçalho inicial
            writer.writerow(["Linha", "Coluna", nome_coluna])

            idx = 0
            for i in range(24):
                for j in range(32):
                    writer.writerow([i, j, f"{flat_temp[idx]:.2f}"])
                    idx += 1

        print(f"💾 Criado CSV novo {ARQUIVO_CSV_GLOBAL} com coluna {nome_coluna}")
        return True

    # CSV já existe. Ler tudo e anexar coluna
    with open(ARQUIVO_CSV_GLOBAL, "r", newline="") as f:
        reader = list(csv.reader(f))

    header = reader[0]      # ex: ['Linha','Coluna','20251027_110501',...]
    corpo = reader[1:]      # linhas de pixels

    num_pixels_esperado = 24 * 32
    if len(corpo) != num_pixels_esperado:
        print(f"⚠️ Inconsistência: CSV tem {len(corpo)} linhas. Esperado {num_pixels_esperado}.")

    # Decide se cria nova coluna ou sobrescreve timestamp repetido
    if nome_coluna in header:
        print(f"⚠️ Timestamp repetido {nome_coluna}. Coluna existente será sobrescrita.")
        col_exists = True
        idx_col = header.index(nome_coluna)
    else:
        header.append(nome_coluna)
        col_exists = False
        idx_col = len(header) - 1

    # Preenche valores da nova captura em cada linha
    idx = 0
    for k in range(len(corpo)):
        valor_pixel = f"{flat_temp[idx]:.2f}"
        idx += 1

        if col_exists:
            while len(corpo[k]) <= idx_col:
                corpo[k].append("")
            corpo[k][idx_col] = valor_pixel
        else:
            corpo[k].append(valor_pixel)

    # Regrava CSV inteiro
    with open(ARQUIVO_CSV_GLOBAL, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(header)
        for linha in corpo:
            w.writerow(linha)

    print(f"📄 Atualizado {ARQUIVO_CSV_GLOBAL} com nova coluna {nome_coluna}")
    return True


# =========================
# ENDPOINTS HTTP
# =========================

@app.route("/termica", methods=["POST"])
def receber_dados_termicos():
    """
    Recebe JSON:
    {
        "temperaturas": [768 valores float],
        "timestamp": 1690000000 (opcional)
    }
    """
    try:
        data = request.get_json(force=True)

        if 'temperaturas' not in data:
            return jsonify({"status": "erro", "mensagem": "Campo 'temperaturas' ausente"}), 400

        temperaturas = np.array(data['temperaturas']).reshape(24, 32)
        timestamp = data.get('timestamp', int(datetime.now().timestamp()))

        temp_min = float(np.min(temperaturas))
        temp_max = float(np.max(temperaturas))
        temp_avg = float(np.mean(temperaturas))

        print("🔥 DADOS TÉRMICOS RECEBIDOS:")
        print(f"   📅 Timestamp: {datetime.fromtimestamp(timestamp).strftime('%Y-%m-%d %H%M%S')}")
        print(f"   📊 Estatísticas: min={temp_min:.1f}°C, max={temp_max:.1f}°C, avg={temp_avg:.1f}°C")
        print(f"   📐 Dimensões: {temperaturas.shape[0]}x{temperaturas.shape[1]} ({temperaturas.size} pontos)")
        print(f"   🔢 Amostra: [{temperaturas[0][0]:.1f}, {temperaturas[12][16]:.1f}, {temperaturas[23][31]:.1f}]°C")

        # opcional: manter histórico agregado no InfluxDB
        # salvar_dados_termicos_influxdb(temperaturas, timestamp)

        # atualiza CSV cumulativo
        ok = atualizar_csv_termico_incremental(temperaturas, timestamp)

        return jsonify({
            "status": "sucesso",
            "csv_atualizado": ok,
            "pontos": int(temperaturas.size),
            "timestamp": timestamp,
            "estatisticas": {
                "temp_min": temp_min,
                "temp_max": temp_max,
                "temp_avg": temp_avg
            }
        })

    except Exception as e:
        print(f"❌ Erro ao processar dados térmicos: {e}")
        return jsonify({"status": "erro", "mensagem": str(e)}), 500


@app
