# Greense Server · Núcleo de Telemetria e IA

Backend executado em um Raspberry Pi 4 responsável por centralizar sensores das estufas Greense, processar imagens térmicas e alimentar rotinas de análise via InfluxDB e OpenAI. Os serviços expostos nesta pasta compõem o “cérebro” que integra câmeras, ESP32, filas MQTT e dashboards HTML leves.

## Visão geral
- **Coleta térmica:** `server01Full.py` recebe matrizes 24×32 da câmera FLIR, persiste estatísticas no InfluxDB e mantém um CSV histórico incremental.
- **Ingestão de sensores:** dados chegam via MQTT (`estufa/germinar`, `estufa/maturar`, `estufa3/esp32`) ou por inserções manuais autenticadas (`POST /insere`), todos consolidados na measurement `sensores`.
- **Imagens RGB:** endpoints de upload e distribuição guardam a última foto de cada câmera em `fotos_recebidas/cam_*`.
- **Análises com IA:** `server01IA.py` consulta as últimas medições, gera um resumo agronômico no GPT-4o e grava o laudo em `analise_ia_estufa3`.
- **Dashboards embutidos:** `templates/*.html` oferecem páginas estáticas servidas pelo Nginx local com gráficos e estado das estufas.

## Requisitos rápidos
- Python 3.10+ e pip
- InfluxDB ≥ 1.8 em `localhost:8086`
- Mosquitto (ou outro broker) em `localhost:1883`
- Dependências Python: `flask`, `flask-cors`, `paho-mqtt`, `influxdb`, `numpy`, `openai`

```bash
sudo apt install influxdb mosquitto
python -m venv .venv && source .venv/bin/activate
pip install flask flask-cors paho-mqtt influxdb numpy openai
```

Crie um `config.py` na raiz com:

```python
PASS_MANUAL = "sua-senha-para-POST-/insere"
OPENAI_API_KEY = "sk-..."
```

## Como executar
1. Garanta que InfluxDB e Mosquitto estejam ativos e com o banco `dados_estufa` criado (o script cria automaticamente se não existir).
2. Ative o ambiente virtual.
3. Rode o servidor principal:
   ```bash
   python N01_RASP4_LAB/server01Full.py
   ```
   Ele inicia Flask (`0.0.0.0:5000`), a fila assíncrona para escrita do CSV térmico e o cliente MQTT legado (thread separada).
4. (Opcional) Agende o analista IA:
   ```bash
   python N01_RASP4_LAB/server01IA.py
   ```

## Endpoints HTTP expostos (`server01Full.py`)
| Método | Rota        | Função                                                                 |
|--------|-------------|------------------------------------------------------------------------|
| POST   | `/termica`  | Recebe JSON com `temperaturas` (768 pontos), enfileira para CSV e estatísticas. |
| POST   | `/insere`   | Inserção manual autenticada (`senha`), grava medições simples em `sensores`.    |
| GET    | `/imagem`   | Retorna `ultima.jpg` da câmera associada ao hostname do request.        |
| POST   | `/upload`   | Upload binário de JPGs das câmeras (`camera`, `camera02`, `camera03`). |

## Fluxos MQTT monitorados
- `estufa/germinar` → métricas de clima, reservatório e solo.
- `estufa/maturar` → variáveis ambientais e externas.
- `estufa3/esp32` → leituras complementares de pH e EC.

Todas as mensagens são normalizadas e gravadas na measurement `sensores`, mantendo tags por dispositivo.

## Estrutura útil
```
N01_RASP4_LAB/
├── server01Full.py      # API Flask + MQTT + fila CSV
├── server01IA.py        # Consulta InfluxDB e gera laudo com OpenAI
├── serverTermica.py     # Variante simplificada focada na câmera térmica
├── copia_foto_cam03.py  # Auxiliar para espelhamento de imagens
├── historico_tempo/     # CSV acumulado de mapas térmicos
├── templates/           # HTML usados pelos dashboards e Nginx
└── *.log                # Logs persistentes por serviço/câmera
```

## Observabilidade e arquivos
- **Logs:** `log_cam03.log`, `log_server01IA.log`, `servidor_linha0.log` etc. ajudam a auditar ingestões e alarmes.
- **CSV térmico:** `historico_tempo/termica_global.csv` mantém cada novo frame como uma coluna datada (formato `dd-mm-aa HH:MM`).
- **Dashboards:** publique o conteúdo de `templates/` no Nginx ou sirva diretamente para inspeções rápidas.

## Boas práticas operacionais
- Proteja o Raspberry com `tmux`/`systemd` para manter `server01Full.py` ativo após reboot.
- Agende `server01IA.py` (cron ou systemd timer) para obter relatórios recorrentes.
- Faça backup periódico de `historico_tempo/` e do banco InfluxDB, pois concentram a série histórica completa.
- Revise permissões dos diretórios `fotos_recebidas/` para evitar falhas no symlink de `ultima.jpg`.

---
Mantenha este README atualizado sempre que novos sensores, endpoints ou dashboards forem adicionados ao hub Greense.

