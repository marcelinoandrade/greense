# 🌱 Projeto GreenSe – Sensor de Campo IoT (ESP32)

Sistema embarcado desenvolvido com **ESP-IDF (v5.x)** para monitoramento ambiental e de solo, integrando sensores de temperatura, umidade e armazenamento local, com interface web embarcada em servidor HTTP.  

## ⚙️ Visão Geral

O projeto implementa um nó de coleta de dados ambientais e de solo para aplicações de **agricultura inteligente**.  

O firmware cria uma rede **Wi-Fi Access Point (AP)** local e hospeda uma página interativa acessível via navegador (`http://192.168.4.1/`), permitindo visualizar gráficos, calibrar sensores e baixar o histórico de medições em CSV.

### Funcionalidades principais

- 📡 Cria rede Wi-Fi local “ESP32_TEMP” com IP fixo `192.168.4.1`.
- 🌤️ Lê sensores de:
  - Temperatura e umidade do ar (AHT/DHT ou similar)
  - Temperatura do solo (DS18B20)
  - Umidade do solo (sensor resistivo ou capacitivo via ADC)
- 💾 Armazena leituras em `log_temp.csv` no **SPIFFS**.
- 📈 Exibe **dashboard com 4 gráficos**:
  - Temperatura do ar (°C)
  - Umidade do ar (%)
  - Temperatura do solo (°C)
  - Umidade do solo (%)
- ⚙️ Permite **calibração da umidade do solo** (parâmetros “seco” e “molhado”).
- ⬇️ Oferece **download direto** do log em CSV.
- 🔧 Possui servidor HTTP leve com rotas dedicadas.

---

## 🧩 Estrutura de Diretórios

```
main/
├── main.c                     # Inicialização, tarefas e loop principal
├── libs/
│   ├── data_logger.c/.h       # Registro de dados no SPIFFS e histórico JSON
│   ├── http_server.c/.h       # Servidor HTTP e páginas web
│
├── sensores/
│   ├── sensores.c/.h          # Integração dos sensores
│   ├── ds18b20.c/.h           # Leitura do sensor de temperatura do solo
│   ├── soil_moisture.c/.h     # Leitura e calibração do sensor de umidade do solo
│
├── CMakeLists.txt             # Configuração de build e dependências
└── README.md                  # Este arquivo
```

---

## 🖼️ Hardware de Referência

| ESP32-Battery|
|-----------------|
| ![ESP32](esp32_cam.png) |


## 🌐 Servidor Web Integrado

### Rotas HTTP

| Rota             | Método | Descrição |
|------------------|---------|-----------|
| `/`              | GET     | Página principal com 4 gráficos e botões de ação |
| `/history`       | GET     | Retorna JSON com últimas leituras |
| `/calibra`       | GET     | Página para calibração manual |
| `/set_calibra`   | GET     | Aplica calibração (via query string) |
| `/download`      | GET     | Baixa `log_temp.csv` completo |
| `/favicon.ico`   | GET     | Ícone da página (1×1 PNG) |

---

## 📊 Estrutura do Arquivo CSV

Local: `/spiffs/log_temp.csv`

| Campo | Descrição | Unidade |
|--------|------------|---------|
| N | Índice sequencial | — |
| temp_ar_C | Temperatura do ar | °C |
| umid_ar_pct | Umidade relativa do ar | % |
| temp_solo_C | Temperatura do solo | °C |
| umid_solo_pct | Umidade do solo calibrada | % |

---

## 💾 Requisitos de Build

### Ferramentas

- ESP-IDF ≥ **v5.0**
- Python 3.x
- Ferramentas padrão (`idf.py`, `esptool.py`)

### Componentes ESP-IDF utilizados

- `esp_wifi`, `esp_netif`, `esp_http_server`
- `esp_event`, `lwip`
- `esp_adc`, `nvs_flash`, `spiffs`, `driver`
- `freertos`, `esp_rom`, `vfs`

---

## 🚀 Como Executar

1. Clone este repositório e configure o ambiente ESP-IDF:
   ```bash
   idf.py set-target esp32
   idf.py menuconfig
   ```
2. Compile e grave na placa:
   ```bash
   idf.py build flash monitor
   ```
3. Conecte-se ao Wi-Fi **ESP32_TEMP** (senha: `12345678`).
4. Acesse **http://192.168.4.1/** no navegador.

---

## 🧪 Testes de Campo

- Testado em ESP32-WROOM-32 e ESP32-S3.
- Funcionamento validado em:
  - **Chrome** (Android e Desktop)
  - **Edge** (Desktop)
  - **Samsung Browser** — com restrições de cabeçalhos HTTP (erro 431 sem impacto funcional).

---

## 🧰 Extensões futuras

- Envio MQTT para servidor remoto.
- Dashboard remoto via Flask/InfluxDB.
- Integração com AI (modelo embarcado de previsão de irrigação).
- Modo STA (conexão em rede existente).
- Suporte a OTA update.

---

## 🧑‍🔬 Autoria e Créditos

**Projeto GreenSe | Agricultura Inteligente**  
Coordenação: *Prof. Marcelino Monteiro de Andrade* e *Prof. Ronne Toledo*  
Faculdade de Ciências e Tecnologias em Engenharia (FCTE) – Universidade de Brasília  
📧 [andrade@unb.br](mailto:andrade@unb.br)  
🌐 [https://greense.com.br](https://greense.com.br)
