from influxdb import InfluxDBClient
from openai import OpenAI
from datetime import datetime
import config

# === Configurações ===

INFLUXDB_HOST = "localhost"
INFLUXDB_PORT = 8086
INFLUXDB_DB = "dados_estufa"
openai_client = OpenAI(api_key=config.OPENAI_API_KEY)

# === Funções ===

def registrar_inicio_log():
    agora = datetime.now()
    data_hora_formatada = agora.strftime("%d/%m/%Y %H:%M")
    print(f"\n📜 [{data_hora_formatada}] Início da execução do script de análise da estufa.\n")

def coletar_dados_estufa3():
    client = InfluxDBClient(host=INFLUXDB_HOST, port=INFLUXDB_PORT, database=INFLUXDB_DB)

    # Consulta unificada dos sensores
    query_sensores = """
    SELECT 
        LAST(temp), 
        LAST(umid), 
        LAST(co2), 
        LAST(luz), 
        LAST(temp_reserv_int), 
        LAST(agua_min), 
        LAST(agua_max) 
    FROM sensores 
    WHERE dispositivo='ESP32_E3'
    """
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
                'agua_min': points[0].get('last_5', 0),
                'agua_max': points[0].get('last_6', 0),
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
        "agua_min": int(dados.get("agua_min", 0)),
        "agua_max": int(dados.get("agua_max", 0)),
        "timestamp_consulta": datetime.utcnow().isoformat() + "Z"
    }
    return estrutura

def enviar_para_openai(estrutura):
    try:
        agora = datetime.now()
        data_hora_formatada = agora.strftime("%d/%m/%Y %H:%M")

        prompt = f"""
        Você é um engenheiro agrícola. Avalie as condições ambientais de uma estufa de maturação para cultivo de alface hidropônico com base nos seguintes dados (coletados em {data_hora_formatada}).

        Faixas ideais:
        - Temperatura ambiente: 19.5 °C (tolerável: 15–24 °C)
        - Umidade relativa: 60% (tolerável: 50–70%)
        - CO₂: 400–800 ppm (tolerável até 1000 ppm)
        - pH: 6.0 (tolerável: 5.5–6.5)
        - EC: 1.6 mS/cm (tolerável: 1.2–2.0 mS/cm)
        - Temp. reservatório: 21 °C (tolerável: 18–24 °C)
        - Luminosidade: sempre adequada (12h de luz garantida)
        - água_max e água_min: sensores binários onde 1 indica que o nível do reservatório está acima da boia (nível adequado), e 0 indica que está abaixo da boia.  
          → água_max em 0 indica que o nível está abaixo do ideal e recomenda-se reposição preventiva, embora não haja qualquer risco envolvido;  
          → água_min em 0 indica nível criticamente baixo, com risco de falha no fornecimento de nutrientes à planta.

        Dados atuais:
        - Temp. ambiente: {estrutura['temperatura_ambiente']} °C
        - Umidade: {estrutura['umidade_ambiente']} %
        - CO₂: {estrutura['nivel_co2']} ppm
        - pH: {estrutura['ph_agua']}
        - EC: {estrutura['condutividade_ec']} mS/cm
        - Temp. reservatório: {estrutura['temp_reservatorio_interno']} °C
        - água_max: {estrutura['agua_max']} (1 = nível adequado, 0 = abaixo da boia, reposição recomendada)
        - água_min: {estrutura['agua_min']} (1 = nível adequado, 0 = crítico, abaixo da boia)

        Avalie cada parâmetro separadamente utilizando frases curtas, claras e objetivas.
        Para cada item, indique se o valor está dentro, próximo dos limites ou fora da faixa tolerada, sempre apresentando
        o valor numérico correspondente. Inicie a resposta com a data e hora da análise, no formato:
        "Análise realizada em {data_hora_formatada}." Não utilize marcações como negrito, sublinhado ou quebras de linha explícitas (\n).
        Separe cada avaliação individual com ponto e vírgula (;). Ao final da análise, inclua uma conclusão geral iniciada por
        "Conclusão:", separada do restante por ' || '. Em seguida, adicione uma previsão de impacto iniciada por "Impacto previsto:",
        resumindo em uma única frase o que pode ocorrer caso os parâmetros fora da faixa não sejam corrigidos;
        mencione até duas possíveis fitopatologia mais prováveis, justifique as causas de forma curta e vinculado ao paramentro medido;
        classifique o risco atual em uma escala de 0/5 a 5/5, sendo 5/5 crítico;
        Caso todos os parâmetros estejam dentro das faixas toleradas, informe que não há risco identificado.
        Caso algum ou mais sensor esteja com valor zero realize a analise sem considerar esse sensor. Poŕem, indique que o sensor esta inoperante.
        Toda a resposta deve ser redigida como um único parágrafo corrido.

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
    print("✅ Resposta da IA Eng. GePeTo gravada no InfluxDB.")

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
        print("\n🧠 Resposta da IA Eng. GePeTo sobre a Estufa de Maturação:")
        print(resposta_ia)
        gravar_resposta_influx(resposta_ia)

# === Execução ===

if __name__ == "__main__":
    main()
