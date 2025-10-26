import json
from flask import Flask, request, jsonify
from flask_cors import CORS
from datetime import datetime
import os
import csv
import logging

# ========== CONFIGURAÇÕES ==========
HOST = "0.0.0.0"
PORT = 5000

# ========== CONFIGURAÇÃO DE LOG ==========
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler('servidor_linha0.log')
    ]
)
logger = logging.getLogger(__name__)

# ========== INICIALIZAÇÃO ==========
app = Flask(__name__)
CORS(app)

# ========== FUNÇÕES AUXILIARES ==========
def salvar_linha_csv(linha_idx, pixels, timestamp):
    """Salva linha 0 em CSV"""
    try:
        dt = datetime.fromtimestamp(timestamp)
        diretorio = "historico_linha0"
        os.makedirs(diretorio, exist_ok=True)

        filename = f"linha0_{dt.strftime('%Y%m%d')}.csv"
        caminho = os.path.join(diretorio, filename)

        file_exists = os.path.exists(caminho)
        
        with open(caminho, "a", newline="") as f:
            writer = csv.writer(f)
            if not file_exists:
                writer.writerow(["Timestamp", "Data_Hora", "Linha", "Coluna", "Temperatura_C"])
            
            for coluna, temp in enumerate(pixels):
                writer.writerow([timestamp, dt.strftime('%Y-%m-%d %H:%M:%S'), linha_idx, coluna, f"{temp:.2f}"])

        logger.info(f"💾 CSV salvo: {caminho}")
        return caminho
    except Exception as e:
        logger.error(f"❌ Erro ao salvar CSV: {e}")
        return None

# ========== ENDPOINT PRINCIPAL ==========
@app.route("/dados-termicos-linha", methods=["POST", "GET"])
def receber_dados_termicos_linha():
    """Endpoint para receber linhas térmicas individuais"""
    try:
        # Log da requisição
        client_ip = request.remote_addr
        logger.info(f"📥 Requisição de {client_ip} - Método: {request.method}")
        
        if request.method == 'GET':
            return jsonify({
                "status": "online",
                "servico": "Receptor Linha 0",
                "mensagem": "Use POST para enviar dados",
                "timestamp": datetime.now().isoformat()
            })
        
        # Processa POST
        if not request.is_json:
            logger.warning("❌ Content-Type não é JSON")
            return jsonify({"status": "erro", "mensagem": "Content-Type deve ser application/json"}), 400
        
        data = request.get_json(force=True)
        
        # Validação dos dados
        if 'linha' not in data or 'pixels' not in data:
            logger.warning("❌ Campos obrigatórios ausentes")
            return jsonify({"status": "erro", "mensagem": "Campos 'linha' e 'pixels' são obrigatórios"}), 400
        
        linha_idx = data['linha']
        pixels = data['pixels']
        timestamp = data.get('timestamp', int(datetime.now().timestamp()))
        
        if not isinstance(pixels, list) or len(pixels) != 32:
            logger.warning(f"❌ Linha deve ter 32 pixels, recebido {len(pixels)}")
            return jsonify({"status": "erro", "mensagem": "Linha deve ter 32 pixels"}), 400
        
        # Valida valores numéricos
        try:
            pixels_float = [float(p) for p in pixels]
        except (ValueError, TypeError) as e:
            logger.warning(f"❌ Valores de pixel inválidos: {e}")
            return jsonify({"status": "erro", "mensagem": "Valores de pixel devem ser números"}), 400
        
        # Processamento
        temp_min = min(pixels_float)
        temp_max = max(pixels_float)
        temp_avg = sum(pixels_float) / len(pixels_float)
        
        logger.info(f"🔥 LINHA {linha_idx} RECEBIDA:")
        logger.info(f"   📅 {datetime.fromtimestamp(timestamp).strftime('%Y-%m-%d %H:%M:%S')}")
        logger.info(f"   📊 Min:{temp_min:.1f}°C Max:{temp_max:.1f}°C Avg:{temp_avg:.1f}°C")
        logger.info(f"   🔢 Amostra: [{pixels_float[0]:.1f}, {pixels_float[16]:.1f}, {pixels_float[31]:.1f}]°C")
        
        # Salva dados
        arquivo_csv = salvar_linha_csv(linha_idx, pixels_float, timestamp)
        
        # Resposta de sucesso
        response_data = {
            "status": "sucesso", 
            "linha": linha_idx,
            "timestamp": timestamp,
            "arquivo": arquivo_csv,
            "estatisticas": {
                "temp_min": float(temp_min),
                "temp_max": float(temp_max),
                "temp_avg": float(temp_avg)
            }
        }
        
        logger.info(f"✅ Resposta enviada: {response_data}")
        return jsonify(response_data)

    except Exception as e:
        logger.error(f"❌ Erro no endpoint: {e}", exc_info=True)
        return jsonify({"status": "erro", "mensagem": "Erro interno do servidor"}), 500

@app.route("/status", methods=["GET"])
def status():
    """Endpoint de status do servidor"""
    return jsonify({
        "status": "online",
        "servico": "Receptor Linha 0",
        "versao": "1.0",
        "timestamp": datetime.now().isoformat(),
        "endpoints": {
            "POST /dados-termicos-linha": "Recebe dados da linha 0",
            "GET /status": "Status do servidor"
        }
    })

@app.route("/", methods=["GET"])
def index():
    """Página inicial"""
    return jsonify({
        "mensagem": "Servidor para recebimento de dados térmicos - Linha 0",
        "uso": "Envie dados POST para /dados-termicos-linha",
        "status": "online"
    })

# ========== HANDLER DE ERROS ==========
@app.errorhandler(404)
def not_found(error):
    return jsonify({"status": "erro", "mensagem": "Endpoint não encontrado"}), 404

@app.errorhandler(405)
def method_not_allowed(error):
    return jsonify({"status": "erro", "mensagem": "Método não permitido"}), 405

# ========== INICIALIZAÇÃO ==========
if __name__ == "__main__":
    print("🚀 SERVIDOR LINHA 0 - INICIANDO...")
    print("=" * 50)
    print(f"🌐 Host: {HOST}")
    print(f"🔌 Porta: {PORT}")
    print("=" * 50)
    print("📊 ENDPOINTS:")
    print("   POST /dados-termicos-linha - Recebe dados da linha 0")
    print("   GET  /status               - Status do servidor")
    print("   GET  /                     - Página inicial")
    print("=" * 50)
    print("💾 ARMAZENAMENTO:")
    print("   📁 CSV: historico_linha0/")
    print("   📄 Log: servidor_linha0.log")
    print("=" * 50)
    
    # Cria diretório se não existir
    os.makedirs("historico_linha0", exist_ok=True)
    
    # Inicia servidor
    app.run(host=HOST, port=PORT, debug=False, threaded=True)