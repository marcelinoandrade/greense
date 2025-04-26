from influxdb import InfluxDBClient
from openai import OpenAI
from datetime import datetime
import config 

# === Configurações ===

# Configurações do InfluxDB
INFLUXDB_HOST = "localhost"
INFLUXDB_PORT = 8086
INFLUXDB_DB = "dados_estufa"

# Configuração da API da OpenAI
openai_client = OpenAI(api_key=config.OPENAI_API_KEY)

# === Funções ===

def registrar_inicio_log():
    agora = datetime.now()
    data_hora_formatada = agora.strftime("%d/%m/%Y %H:%M")
    print(f"\n📜 [{data_hora_formatada}] Início da execução do script de análise da estufa.\n")

def coletar_dados_estufa3():
    client = InfluxDBClient(host=INFLUXDB_HOST, port=INFLUXDB_PORT, database=INFLUXDB_DB)

    # Consulta geral dos sensores (exceto pH e EC)
    query_sensores = "SELECT LAST(temp), LAST(umid), LAST(co2), LAST(luz), LAST(temp_reserv_int), LAST(temp_reserv_ext) FROM sensores WHERE dispositivo='ESP32_E3'"
    result_sensores = client.query(query_sensores)

    dados = {}
    if result_sensores:
        points = list(result_sensores.get_points())
        if points:
            dados.update({
                'temp': points[0].get('last', None),
                'umid': points[0].get('last_1', None),
                'co2': points[0].get('last_2', None),
                'luz': points[0].get('last_3', None),
                'temp_reserv_int': points[0].get('last_4', None),
                'temp_reserv_ext': points[0].get('last_5', None),
            })

    # Buscar último pH diferente de zero
    query_ph = "SELECT ph FROM sensores WHERE dispositivo='ESP32_E3' AND ph != 0 ORDER BY time DESC LIMIT 1"
    result_ph = client.query(query_ph)
    ph_value = None
    for point in result_ph.get_points():
        ph_value = point.get('ph', None)
    
    # Buscar último EC diferente de zero
    query_ec = "SELECT ec FROM sensores WHERE dispositivo='ESP32_E3' AND ec != 0 ORDER BY time DESC LIMIT 1"
    result_ec = client.query(query_ec)
    ec_value = None
    for point in result_ec.get_points():
        ec_value = point.get('ec', None)

    # Atualizar dados finais
    dados['ph'] = ph_value
    dados['ec'] = ec_value

    return dados

def montar_estrutura_openai(dados):
    estrutura = {
        "temperatura_ambiente": dados.get('temp', None),
        "umidade_ambiente": dados.get('umid', None),
        "nivel_co2": dados.get('co2', None),
        "luminosidade": dados.get('luz', None),
        "ph_agua": dados.get('ph', None),
        "condutividade_ec": dados.get('ec', None),
        "temp_reservatorio_interno": dados.get('temp_reserv_int', None),
        "temp_reservatorio_externo": dados.get('temp_reserv_ext', None),
        "timestamp_consulta": datetime.utcnow().isoformat() + "Z"
    }
    return estrutura

from datetime import datetime

def enviar_para_openai(estrutura):
    try:
        agora = datetime.now()
        data_hora_formatada = agora.strftime("%d/%m/%Y %H:%M")

        prompt = f"""
        Você é um engenheiro agrícola. Avalie as condições da estufa de maturação para cultivo de alface hidroponico com base nos dados abaixo.
        Responda em um único parágrafo, de forma o mais curta e direta possível.
        
        Observação: a luminosidade está adequada; o sensor indica apenas se a luz está ligada ou desligada e o ciclo é de 12hs.
        Informe também no início da resposta a hora e o dia da análise ({data_hora_formatada}).

        Dados coletados:
        - Temperatura ambiente: {estrutura['temperatura_ambiente']} °C
        - Umidade ambiente: {estrutura['umidade_ambiente']} %
        - Nível de CO₂: {estrutura['nivel_co2']} ppm
        - Luminosidade (indicador ligado/desligado): {estrutura['luminosidade']}
        - pH da água: {estrutura['ph_agua']}
        - Condutividade elétrica (EC): {estrutura['condutividade_ec']} µS/cm
        - Temperatura do reservatório interno: {estrutura['temp_reservatorio_interno']} °C
        """

        response = openai_client.chat.completions.create(
            model="gpt-4o",
            messages=[
                {"role": "system", "content": "Você é um engenheiro agrícola especialista em estufas de maturação."},
                {"role": "user", "content": prompt}
            ],
            temperature=0.2
        )

        return response.choices[0].message.content

    except Exception as e:
        print("⚠️ Erro ao conectar com a OpenAI:")
        print(str(e))
        return None

    
def gravar_resposta_influx(resposta_ia):
    client = InfluxDBClient(host=INFLUXDB_HOST, port=INFLUXDB_PORT, database=INFLUXDB_DB)

    json_body = [
        {
            "measurement": "analise_ia_estufa3",
            "tags": {
                "dispositivo": "ESP32_E3"
            },
            "fields": {
                "avaliacao": resposta_ia
            },
            "time": datetime.utcnow().isoformat() + "Z"
        }
    ]

    client.write_points(json_body)
    print("✅ Resposta da IA gravada no InfluxDB.")


def main():
    
    registrar_inicio_log()

    dados_estufa3 = coletar_dados_estufa3()
    print("📡 Dados coletados da Estufa3:")
    print(dados_estufa3)

    estrutura = montar_estrutura_openai(dados_estufa3)
    print("\n📦 Estrutura para enviar para OpenAI:")
    print(estrutura)

    resposta_ia = enviar_para_openai(estrutura)
    if resposta_ia:
        print("\n🧠 Resposta da IA sobre a Estufa3:")
        print(resposta_ia)
        gravar_resposta_influx(resposta_ia)


# === Execução ===

if __name__ == "__main__":
    main()
