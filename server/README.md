# Greense Server · Núcleo de Telemetria e IA

Backend executado em Raspberry Pi 4 responsável por centralizar sensores das estufas Greense, processar imagens térmicas e alimentar rotinas de análise via InfluxDB e OpenAI.

---

## Descrição Geral

Sistema central em Python que atua como hub de telemetria e processamento para o ecossistema Greense, integrando múltiplos dispositivos IoT, armazenamento temporal e inteligência artificial para análise agronômica.

### Componentes Principais

- **Coleta térmica**: `server01Full.py` recebe matrizes 24×32 da câmera FLIR, persiste estatísticas no InfluxDB e mantém CSV histórico incremental
- **Ingestão de sensores**: dados chegam via MQTT (`estufa/germinar`, `estufa/maturar`, `estufa3/esp32`) ou inserções manuais autenticadas (`POST /insere`), consolidados na measurement `sensores`
- **Imagens RGB**: endpoints de upload e distribuição guardam última foto de cada câmera em `fotos_recebidas/cam_*`
- **Análises com IA**: `server01IA.py` consulta últimas medições, gera resumo agronômico via GPT-4o e grava laudo em `analise_ia_estufa3`
- **Dashboards embutidos**: `templates/*.html` oferecem páginas estáticas servidas pelo Nginx local com gráficos e estado das estufas

---

## Requisitos

### Hardware
- Raspberry Pi 4/5 com acesso à internet

### Software
- Python 3.10+
- InfluxDB ≥ 1.8 (porta `localhost:8086`)
- Mosquitto ou outro broker MQTT (porta `localhost:1883`)

### Dependências Python

```bash
flask
flask-cors
paho-mqtt
influxdb
numpy
openai
```

---

## Instalação

```bash
# Instalar serviços base
sudo apt install influxdb mosquitto

# Criar ambiente virtual
python -m venv .venv
source .venv/bin/activate

# Instalar dependências
pip install flask flask-cors paho-mqtt influxdb numpy openai
```

### Configuração

Crie `config.py` na raiz do projeto:

```python
PASS_MANUAL = "sua-senha-para-POST-/insere"
OPENAI_API_KEY = "sk-..."
```

---

## Execução

### Servidor Principal

```bash
python N01_RASP4_LAB/server01Full.py
```

Inicia Flask (`0.0.0.0:5000`), fila assíncrona para escrita do CSV térmico e cliente MQTT legado (thread separada).

### Serviço de IA (Opcional)

```bash
python N01_RASP4_LAB/server01IA.py
```

Consulta InfluxDB periodicamente e gera análises agronômicas via GPT-4o.

---

## Endpoints HTTP

| Método | Rota | Descrição |
|--------|------|-----------|
| POST | `/termica` | Recebe JSON com `temperaturas` (768 pontos), enfileira para CSV e estatísticas |
| POST | `/insere` | Inserção manual autenticada (`senha`), grava medições em `sensores` |
| GET | `/imagem` | Retorna `ultima.jpg` da câmera associada ao hostname do request |
| POST | `/upload` | Upload binário de JPGs das câmeras (`camera`, `camera02`, `camera03`) |

---

## Fluxos MQTT

| Tópico | Descrição | Measurement |
|--------|-----------|-------------|
| `estufa/germinar` | Métricas de clima, reservatório e solo | `sensores` |
| `estufa/maturar` | Variáveis ambientais e externas | `sensores` |
| `estufa3/esp32` | Leituras complementares de pH e EC | `sensores` |

Todas as mensagens são normalizadas e gravadas na measurement `sensores`, mantendo tags por dispositivo.

---

## Estrutura do Projeto

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

---

## Observabilidade

### Logs
- `log_cam03.log`, `log_server01IA.log`, `servidor_linha0.log` etc. ajudam a auditar ingestões e alarmes

### CSV Térmico
- `historico_tempo/termica_global.csv` mantém cada novo frame como coluna datada (formato `dd-mm-aa HH:MM`)

### Dashboards
- Publique o conteúdo de `templates/` no Nginx ou sirva diretamente para inspeções rápidas

---

## Boas Práticas Operacionais

- Proteja o Raspberry com `tmux`/`systemd` para manter `server01Full.py` ativo após reboot
- Agende `server01IA.py` (cron ou systemd timer) para obter relatórios recorrentes
- Faça backup periódico de `historico_tempo/` e do banco InfluxDB (série histórica completa)
- Revise permissões dos diretórios `fotos_recebidas/` para evitar falhas no symlink de `ultima.jpg`

---

## Licença

Este projeto faz parte do Projeto GreenSe da Universidade de Brasília.

**Autoria**: Prof. Marcelino Monteiro de Andrade  
**Instituição**: Faculdade de Ciências e Tecnologias em Engenharia (FCTE) – Universidade de Brasília  
**Website**: [https://greense.com.br](https://greense.com.br)