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
import numpy as np
import csv

# ========== CONFIGURAÇÕES ==========
INFLUXDB_HOST = "localhost"
INFLUXDB_PORT = 8086
INFLUXDB_DB = "dados_estufa"
SENHA_CORRETA = config.PASS_MANUAL

# ========== INICIALIZAÇÃO ==========
# Conectar ao InfluxDB
client_influx = InfluxDBClient(host=INFLUXDB_HOST, port=INFLUXDB_PORT)
client_influx.create_database(INFLUXDB_DB)
client_influx.switch_database(INFLUXDB_DB)

# Flask app
app = Flask(__name__)
CORS(app)

# ========== FUNÇÕES AUXILIARES ==========
def salvar_dados_termicos_influxdb(temperaturas, timestamp):
    """Salva estatísticas térmicas no InfluxDB"""
    try:
        temp_min = np.min(temperaturas)
        temp_max = np.max(temperaturas)
        temp_avg = np.mean(temperaturas)
        temp_std = np.std(temperaturas)
        
        json_body = [{
            "measurement": "termica",
            "tags": {"dispositivo": "Camera_Termica"},
            "time": timestamp * 1000000000,  # converte para nanossegundos
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

def salvar_csv_completo(temperaturas, timestamp):
    """Salva matriz completa em CSV"""
    try:
        dt = datetime.fromtimestamp(timestamp)
        diretorio = "historico_tempo"
        os.makedirs(diretorio, exist_ok=True)

        filename = f"termica_{dt.strftime('%Y%m%d_%H%M%S')}.csv"
        caminho = os.path.join(diretorio, filename)

        with open(caminho, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["Linha", "Coluna", "Temperatura_C"])
            for i in range(24):
                for j in range(32):
                    writer.writerow([i, j, f"{temperaturas[i][j]:.2f}"])

        print(f"💾 CSV salvo em: {caminho}")
        return caminho
    except Exception as e:
        print(f"❌ Erro ao salvar CSV: {e}")
        return None

# ===============================================
# <-- INÍCIO DA MUDANÇA
# ===============================================

def processar_em_background(temperaturas_copia, timestamp):
    """
    Função executada em background para salvar o CSV sem travar a 
    requisição principal.
    """
    print("🚀 Iniciando salvamento do CSV em background...")
    try:
        salvar_csv_completo(temperaturas_copia, timestamp)
    except Exception as e:
        print(f"❌ Erro na thread de salvamento do CSV: {e}")

# ========== ENDPOINTS FLASK ==========
@app.route("/termica", methods=["POST"])
def receber_dados_termicos():
    """Endpoint principal para receber dados térmicos via HTTP"""
    try:
        data = request.get_json(force=True)
        
        # Validação dos dados
        if 'temperaturas' not in data:
            return jsonify({"status": "erro", "mensagem": "Campo 'temperaturas' ausente"}), 400
        
        # Converte lista em matriz 24x32
        temperaturas = np.array(data['temperaturas']).reshape(24, 32)
        timestamp = data.get('timestamp', int(datetime.now().timestamp()))
        
        # Print dos dados recebidos (RÁPIDO)
        temp_min = np.min(temperaturas)
        temp_max = np.max(temperaturas)
        temp_avg = np.mean(temperaturas)
        
        print(f"🔥 DADOS TÉRMICOS RECEBIDOS:")
        print(f"   📅 Timestamp: {datetime.fromtimestamp(timestamp).strftime('%Y-%m-%d %H%M%S')}")
        print(f"   📊 Estatísticas: min={temp_min:.1f}°C, max={temp_max:.1f}°C, avg={temp_avg:.1f}°C")
        print(f"   📐 Dimensões: {temperaturas.shape[0]}x{temperaturas.shape[1]} ({temperaturas.size} pontos)")
        print(f"   🔢 Amostra: [{temperaturas[0][0]:.1f}, {temperaturas[12][16]:.1f}, {temperaturas[23][31]:.1f}]°C")
        
        # (Não salva no InfluxDB)
        # salvar_dados_termicos_influxdb(temperaturas, timestamp) 
        
        # INICIA O SALVAMENTO DO CSV EM SEGUNDO PLANO (RÁPIDO)
        # Passamos uma cópia (np.copy) para garantir que a thread
        # tenha seus próprios dados.
        args_para_thread = (np.copy(temperaturas), timestamp)
        threading.Thread(target=processar_em_background, args=args_para_thread, daemon=True).start()
        
        # RESPONDE IMEDIATAMENTE AO ESP32 (RÁPIDO)
        print("⚡ Resposta 200 OK enviada ao ESP32.")
        
        return jsonify({
            "status": "sucesso", 
            "arquivo": "processando_em_background",
            "pontos": temperaturas.size, 
            "timestamp": timestamp,
            "estatisticas": {
                "temp_min": float(temp_min),
                "temp_max": float(temp_max),
                "temp_avg": float(temp_avg)
            }
        })

    except Exception as e:
        print(f"❌ Erro ao processar dados térmicos: {e}")
        return jsonify({"status": "erro", "mensagem": str(e)}), 500

# ===============================================
# <-- FIM DA MUDANÇA
# ===============================================

@app.route("/insere", methods=["POST"])
def insere_manual():
    """Inserção manual de dados de sensores"""
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
        
        print(f"🔧 DADOS MANUAIS INSERIDOS:")
        print(f"   📅 Horário: {datetime.now().strftime('%Y-%m-%d %H:%M%S')}")
        print(f"   🧪 pH: {ph}")
        print(f"   ⚡ EC: {ec}")
        
        return jsonify({"status": "ok", "ph": ph, "ec": ec})
    except Exception as e:
        return jsonify({"erro": str(e)}), 500

@app.route("/imagem")
def serve_ultima_imagem():
    """Serve a última imagem baseada no hostname"""
    host = request.headers.get("Host", "").split(":")[0]
    
    if "camera03" in host:
        diretorio_fotos = "fotos_recebidas/cam_03"
    elif "camera02" in host:
        diretorio_fotos = "fotos_recebidas/cam_02"
    elif "camera" in host:
        diretorio_fotos = "fotos_recebidas/cam_01"
    else:
        return jsonify({"erro": f"Domínio inválido: {host}"}), 400

    caminho = os.path.join(diretorio_fotos, "ultima.jpg")
    if not os.path.exists(caminho):
        return jsonify({"erro": f"Imagem não disponível para {host}"}), 404

    return send_from_directory(diretorio_fotos, "ultima.jpg")

@app.route("/upload", methods=["POST"])
def upload_foto():
    """Upload de imagens das câmeras"""
    try:
        host = request.headers.get("Host", "").split(":")[0]
        
        if "camera03" in host:
            camera_id = "cam_03"
        elif "camera02" in host:
            camera_id = "cam_02"
        elif "camera" in host:
            camera_id = "cam_01"
        else:
            return jsonify({"erro": f"Host inválido: {host}"}), 400

        diretorio_fotos = os.path.join("fotos_recebidas", camera_id)
        os.makedirs(diretorio_fotos, exist_ok=True)

        conteudo = request.data
        if not conteudo:
            return jsonify({"erro": "Imagem vazia"}), 400

        # Salva arquivo com timestamp
        nome_arquivo = datetime.now().strftime(f"{camera_id}_%Y%m%d_%H%M%S.jpg")
        caminho = os.path.join(diretorio_fotos, nome_arquivo)
        
        with open(caminho, "wb") as f:
            f.write(conteudo)

        # Cria symlink para última imagem
        link_fixo = os.path.join(diretorio_fotos, "ultima.jpg")
        try:
            if os.path.exists(link_fixo):
                os.remove(link_fixo)
            os.symlink(os.path.abspath(caminho), link_fixo)
        except Exception as e:
            print(f"⚠️ Erro ao criar symlink: {e}")

        # Limpeza de arquivos antigos
        for filename in os.listdir(diretorio_fotos):
            if filename not in [os.path.basename(caminho), "ultima.jpg"]:
                file_path = os.path.join(diretorio_fotos, filename)
                try:
                    if os.path.isfile(file_path):
                        os.unlink(file_path)
                except Exception as e:
                    print(f"⚠️ Erro ao apagar {filename}: {e}")

        print(f"📷 IMAGEM RECEBIDA:")
        print(f"   📁 Câmera: {camera_id}")
        print(f"   💾 Arquivo: {caminho}")
        print(f"   📏 Tamanho: {len(conteudo)} bytes")
        print(f"   🕒 Horário: {datetime.now().strftime('%Y-%m-%d %H%M%S')}")

        return jsonify({"status": "ok", "arquivo": nome_arquivo, "camera": camera_id})

    except Exception as e:
        print(f"❌ Erro no upload: {e}")
        return jsonify({"erro": str(e)}), 500

# ========== MQTT (Legado) ==========
def processar_dados_mqtt(topic, data):
    """Processa dados MQTT para diferentes estufas"""
    dispositivo = "desconhecido"
    json_body = []

    if topic == "estufa/germinar":
        dispositivo = "Estufa_Germinar"
        json_body = [{
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
                "umid_solo_pct": data.get("umid_solo_pct", 0),
                "umid_solo_raw": data.get("umid_solo_raw", 0)
            }
        }]

    elif topic == "estufa/maturar":
        dispositivo = "Estufa_Maturar"
        json_body = [{
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
                "temp_ext": data.get("temp_externa", 0),
                "umid_ext": data.get("umid_externa", 0)
            }
        }]

    elif topic == "estufa3/esp32":
        dispositivo = "ESP32_E3"
        json_body = [{
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
        }]

    else:
        print(f"⚠️ Tópico desconhecido: {topic}")
        return

    client_influx.write_points(json_body)
    
    # PRINT DETALHADO DOS DADOS MQTT
    print(f"📡 DADO MQTT SALVO ({dispositivo}):")
    print(f"   📍 Tópico: {topic}")
    print(f"   🕒 Horário: {datetime.now().strftime('%Y-%m-%d %H%M%S')}")
    
    # Dados principais
    if 'temp' in data:
        print(f"   🌡️  Temperatura: {data.get('temp', 'N/A')}°C")
    if 'umid' in data:
        print(f"   💧 Umidade: {data.get('umid', 'N/A')}%")
    if 'co2' in data:
        print(f"   🌫️  CO2: {data.get('co2', 'N/A')} ppm")
    if 'luz' in data:
        print(f"   💡 Luz: {data.get('luz', 'N/A')} lux")
    
    # Dados de água/reservatório
    if 'agua_min' in data or 'agua_max' in data:
        print(f"   💦 Água: min={data.get('agua_min', 'N/A')}, max={data.get('agua_max', 'N/A')}")
    if 'temp_reserv_int' in data:
        print(f"   🔥 Temp. Reserv. Interna: {data.get('temp_reserv_int', 'N/A')}°C")
    if 'temp_reserv_ext' in data:
        print(f"   ❄️  Temp. Reserv. Externa: {data.get('temp_reserv_ext', 'N/A')}°C")
    
    # Dados de solo/químicos
    if 'ph' in data:
        print(f"   🧪 pH: {data.get('ph', 'N/A')}")
    if 'ec' in data:
        print(f"   ⚡ EC: {data.get('ec', 'N/A')}")
    if 'umid_solo_pct' in data:
        print(f"   🌱 Umidade Solo: {data.get('umid_solo_pct', 'N/A')}%")
    
    # Dados externos
    if 'temp_externa' in data:
        print(f"   🌍 Temp. Externa: {data.get('temp_externa', 'N/A')}°C")
    if 'umid_externa' in data:
        print(f"   ☁️  Umidade Externa: {data.get('umid_externa', 'N/A')}%")
    
    print(f"   📊 Total de campos: {len(data)}")

def on_message(client, userdata, msg):
    """Callback para mensagens MQTT"""
    try:
        data = json.loads(msg.payload.decode())
        processar_dados_mqtt(msg.topic, data)
    except Exception as e:
        print(f"❌ Erro ao processar MQTT: {e}")

def start_mqtt():
    """Inicia cliente MQTT (modo legado)"""
    client_mqtt = mqtt.Client()
    client_mqtt.on_message = on_message
    client_mqtt.connect("localhost", 1883, 60)
    client_mqtt.subscribe("estufa/germinar")
    client_mqtt.subscribe("estufa/maturar")
    client_mqtt.subscribe("estufa3/esp32")

    print("🔌 MQTT rodando (modo legado)")
    print("   👂 Aguardando mensagens nos tópicos:")
    print("      - estufa/germinar")
    print("      - estufa/maturar") 
    print("      - estufa3/esp32")
    
    client_mqtt.loop_forever()

# ========== INICIALIZAÇÃO ==========
if __name__ == "__main__":
    print("🚀 INICIANDO SERVIDOR TÉRMICO...")
    print("=" * 50)
    print("📊 ENDPOINTS DISPONÍVEIS:")
    print("   POST /termica    - Dados da câmera térmica")
    print("   POST /insere            - Inserção manual de sensores")
    print("   GET  /imagem            - Última imagem da câmera")
    print("   POST /upload            - Upload de imagens")
    print("=" * 50)
    print("🔌 MQTT ATIVO (modo legado)")
    print("=" * 50)
    
    # Inicia MQTT em thread separada (opcional)
    threading.Thread(target=start_mqtt, daemon=True).start()
    
    # Inicia Flask
    app.run(host="0.0.0.0", port=5000, debug=False, threaded=True)