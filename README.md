# GreenSe - Solução para Cultivo Protegido

<div align="center">
  <img src="https://github.com/marcelinoandrade/greense/blob/main/dashboardGreense.jpg" alt="GreenSe Logo" width="800">
</div>

O **GreenSe** é uma solução inovadora para **monitoramento e automação de cultivos protegidos**, com foco em **hidroponia** e outros sistemas em ambiente controlado. Integrando sensores, atuadores e inteligência artificial, o GreenSe otimiza as condições de cultivo, promovendo maior eficiência no uso de água, nutrientes e energia. Saiba mais em [www.greense.com.br](https://www.greense.com.br).

---

## Funcionalidades Principais

- **Monitoramento em tempo real** de imagens visuais e térmicas, temperatura, umidade, pH, condutividade elétrica, nível de CO₂ e luminosidade.
- **Controle de atuadores** para bomba de água (irrigação) e iluminação (LED) baseado em sensores de nível e condições ambientais.
- **Registro e análise contínua de dados** para otimização progressiva das condições de cultivo.
- **Interface web intuitiva** para visualização e controle das estufas.
- **Comunicação segura** via **MQTT** sobre **WebSocket seguro (WSS)** com **TLS**. Suporte a **Cloudflared** (opcional) para acesso remoto seguro.
- **API REST** para inserção de dados complementares manualmente.
- **Geração automática de relatórios** com suporte da IA **Eng. GePeTo** baseada em GPT.

---

## Integração com GPT – IA Eng. GePeTo

<div align="center">
  <img src="https://github.com/marcelinoandrade/greense/blob/main/gepeto.png" alt="Eng. Gepeto" width="100">
</div>

O **GreenSe** integra inteligência artificial por meio da **IA Eng. GePeTo**, um agente construído sobre o modelo GPT, responsável por análises ambientais e suporte ao cultivo. As principais funções dessa integração incluem:

- **Geração automática de relatórios de status**, com interpretações objetivas dos dados de cultivo.
- **Análises técnicas resumidas**, entregues em linguagem prática e direta, simulando a atuação de um engenheiro agrícola.
- **Apoio na tomada de decisão**, oferecendo recomendações rápidas com base nos parâmetros ambientais monitorados.
- **Modelos preditivos** para irrigação, controle climático e uso de nutrientes (em desenvolvimento).
- **Análise automática de imagens** para detecção da evolução da área foliar e de pragas e doenças (em desenvolvimento).

Essa camada de inteligência torna o GreenSe mais autônomo, eficiente e capaz de reduzir a necessidade de intervenção manual no dia a dia do cultivo.


---

## Tecnologias Utilizadas

### Hardware
- **Sensores ambientais**: AHT20 (temperatura/umidade), ENS160 (CO₂/qualidade do ar), DS18B20 (temperatura reservatório), DHT22 (condições externas)
- **Sensores hidropônicos**: pH e condutividade elétrica (EC)
- **Sensores de solo**: temperatura e umidade do solo
- **Sensores de imagem**: ESP32-CAM (visual) e MLX90640 (térmica 24×32 pixels)
- **Atuadores**: relés, LEDs RGB, bombas, sistemas de irrigação
- **Microcontroladores**: ESP32, ESP32-S3, ESP32-C3 (vários modelos conforme aplicação)
- **Servidor**: Raspberry Pi 4 para processamento e armazenamento local
- **Ambiente**: Estufas para cultivo protegido

### Software e Servidores
- **C/C++** para desenvolvimento de firmware de produção.
- **Python (MicroPython)** para prototipagem de sistemas.
- **Flask** para disponibilização de API REST leve e segura.
- **MQTT** com **WebSocket seguro (WSS)** para comunicação de dados.
- **InfluxDB** para armazenamento de séries temporais.
- **Grafana** para dashboards interativos e visualização de dados históricos.
- **Cloudflared** (opcional) para acesso remoto seguro via túnel.
- **NGINX** para hospedagem da página oficial.
- **OpenAI GPT-4o** para suporte de IA no Eng. GePeTo.

---

## Arquitetura do Sistema

### Cliente (Client)

O GreenSe implementa múltiplas soluções de hardware para diferentes necessidades de monitoramento:

#### Estufas de Produção (C/ESP-IDF)
- **N01_Estufa_Germinar_C**: ESP32 com sensores básicos (AHT20, ENS160, DS18B20). Monitora temperatura, umidade e qualidade do ar na fase de germinação. Comunicação MQTT segura (TLS/WSS).
- **N02_Estufa_Maturar_C**: ESP32 com sensores completos (AHT20, ENS160, DS18B20, DHT22, boias de nível, sensor de luz). Monitoramento avançado para estufa de maturação com controle de reservatórios e condições externas.

#### Estufa com Sensores Hidropônicos (MicroPython)
- **N03_Estufa_P**: ESP32 em MicroPython com sensores de pH e condutividade elétrica (EC). Controle de atuadores para ajuste automático de nutrientes e pH na solução hidropônica.

#### Sistemas de Imagem
- **N04_Estufa_Camera_C**: ESP32-CAM para captura de imagens visuais. Monitoramento visual do crescimento e desenvolvimento das plantas.
- **N05_Estufa_Termica_C/P**: ESP32-C3 com câmera térmica MLX90640 (24×32 pixels). Análise térmica para detecção de estresse hídrico, doenças e distribuição de temperatura.
- **N07_Estufa_Artigo_C**: ESP32-S3 com câmera térmica integrada. Solução completa para pesquisa e produção com análise térmica avançada.

#### Sensores de Campo
- **N06_Sensor_Campo_C/P**: ESP32 com bateria para monitoramento de campo. Sensores de temperatura e umidade do solo. Interface web embarcada (AP) para visualização local e download de dados.

**Comunicação**: Todas as soluções cliente utilizam **MQTT sobre TLS/WSS** ou **HTTP POST** para envio seguro de dados ao servidor central.

### Servidor (Server)

#### N01_RASP4_LAB (Raspberry Pi 4)

Sistema central de processamento e armazenamento implementado em Python:

- **server01Full.py**: Servidor principal integrado
  - **MQTT Broker Client**: Recebe dados de todos os nós IoT via MQTT
  - **Flask REST API**: Endpoints para inserção manual de dados, consultas e upload de imagens
  - **InfluxDB Integration**: Armazenamento de séries temporais de todos os sensores
  - **Processamento Térmico**: Recebe e processa dados da câmera térmica MLX90640 (matriz 24×32)
  - **Dashboard Web**: Templates HTML para visualização em tempo real
  - **CSV Export**: Exportação de dados térmicos históricos em CSV incremental

- **server01IA.py**: Serviço de Inteligência Artificial (Eng. GePeTo)
  - Consulta dados do InfluxDB das estufas
  - Gera análises e recomendações usando OpenAI GPT-4o
  - Armazena relatórios no InfluxDB para consulta via dashboard

- **serverTermica.py**: Servidor dedicado para processamento de dados térmicos
  - Recebe dados térmicos via HTTP POST
  - Processa e armazena estatísticas (min, max, média, desvio padrão)
  - Gera visualizações e histórico temporal

**Stack Tecnológico**:
- Python 3.x com Flask
- InfluxDB para armazenamento de séries temporais
- MQTT (paho-mqtt) para comunicação IoT
- OpenAI API para análise inteligente
- NumPy para processamento de dados térmicos

---

## Expansão e Melhorias Futuras

O GreenSe foi concebido para ser **escalável** e atender a diferentes demandas de crescimento:

| Capacidade | Solução |
|:-----------|:--------|
| Até 20 estufas | Raspberry Pi 4 |
| Até 50 estufas | Raspberry Pi 5 |
| Até 1.000 estufas | Servidor central dedicado |
| Acima de 1.000 estufas | Arquitetura distribuída (Edge Computing e Nuvem) |

---


## Como Contribuir

Quer contribuir com o GreenSe? Siga os passos:

1. **Clone o repositório:**
   ```bash
   git clone https://github.com/marcelinoandrade/greense.git
   ```

2. **Explore as soluções:**
   - Revise os projetos em `client/` para entender as diferentes implementações de hardware
   - Analise o servidor em `server/N01_RASP4_LAB/` para compreender a arquitetura central

3. **Desenvolvimento:**
   - Cada projeto cliente possui seu próprio README com instruções específicas
   - O servidor utiliza Python 3.x com dependências listadas nos arquivos de configuração
   - Consulte os READMEs individuais de cada solução para requisitos específicos

4. **Contato:**
   - Para dúvidas ou sugestões, entre em contato: [contato@greense.com.br](mailto:contato@greense.com.br)

---

## Estrutura do Repositório

```
greense/
├── client/              # Soluções de hardware (ESP32)
│   ├── N01_Estufa_Germinar_C/    # Estufa germinação (C/ESP-IDF)
│   ├── N02_Estufa_Maturar_C/     # Estufa maturação (C/ESP-IDF)
│   ├── N03_Estufa_P/             # Estufa hidropônica (MicroPython)
│   ├── N04_Estufa_Camera_C/      # Câmera visual (ESP32-CAM)
│   ├── N05_Estufa_Termica_C/P/   # Câmera térmica (MLX90640)
│   ├── N06_Sensor_Campo_C/P/     # Sensor de campo
│   └── N07_Estufa_Artigo_C/      # Solução completa térmica
├── server/              # Sistema servidor
│   └── N01_RASP4_LAB/            # Raspberry Pi 4 - servidor central
└── README.md           # Este arquivo
