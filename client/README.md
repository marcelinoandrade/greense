# 🌿 GreenSe · Catálogo rápido de nós

Cada pasta `Nxx_*` representa uma entrega do Projeto GreenSe, com firmware pronto para campo, sensores definidos e assets de hardware (quando disponíveis). Use esta página como índice para localizar o nó certo antes de abrir o README específico de cada diretório.

---

## 📋 Tabela-resumo

| Nó | Diretório | Stack / Linguagem | Hardware principal | Comunicação | Imagem |
|----|-----------|-------------------|--------------------|-------------|--------|
| N01 · Estufa Germinar | `N01_Estufa_Germinar_C` | ESP-IDF 5.x (C) | ESP32 DevKit + sensores AHT20/ENS160/DS18B20 | Wi-Fi AP/STA, MQTT/TLS, HTTP local | ![ESP32](./N01_Estufa_Germinar_C/esp32_Freenove.png) |
| N02 · Estufa Maturar | `N02_Estufa_Maturar_C` | ESP-IDF 5.x (C) | ESP32 DevKit + boias de nível + LED RGB | Wi-Fi STA, MQTT/TLS-WSS | ![ESP32](./N02_Estufa_Maturar_C/esp32_Freenove.png) |
| N03 · Estufa (µPy) | `N03_Estufa_P` | MicroPython | ESP32 com sensores AHT20/ENS160/pH/EC | Wi-Fi STA, MQTT | — |
| N04 · Estufa Câmera | `N04_Estufa_Camera_C` | ESP-IDF (C) + `esp32-camera` | ESP32-CAM AI Thinker | Wi-Fi STA, HTTPS POST, SD card | ![ESP32-CAM](./N04_Estufa_Camera_C/esp32_cam.png) |
| N05 · Estufa Térmica (C) | `N05_Estufa_Termica_C` | ESP-IDF (C) BSP/APP/GUI | ESP32-C3 + MLX90640 | Wi-Fi STA, HTTP JSON, NTP | ![Thermal](./N05_Estufa_Termica_C/imagens/camera_termica.png) |
| N05 · Estufa Térmica (µPy) | `N05_Estufa_Termica_P` | MicroPython | ESP32-C3 + MLX90640 | Wi-Fi STA, HTTP JSON | ![Thermal](./N05_Estufa_Termica_P/camera_termica.png) |
| N06 · Sensor de Campo (C) | `N06_Sensor_Campo_C` | ESP-IDF (C) + servidor HTTP | ESP32 Battery Kit + sensores de solo | Wi-Fi AP `ESP32_TEMP`, HTTP | ![Kit](./N06_Sensor_Campo_C/imagens/esp32_battery.png) |
| N06 · Sensor de Campo 2 (C) | `N06_Sensor_Campo_2_C` | ESP-IDF (C) com camadas app/bsp/gui | ESP32 módulo único + solo capacitivo + DHT11 + DS18B20 + BH1750 | Wi-Fi AP `greenSe_Campo`, HTTP, mDNS | ![Módulo](./N06_Sensor_Campo_2_C/imagens/moduloEsp32Solo.png) |
| N06 · Sensor de Campo (µPy) | `N06_Sensor_Campo_P` | MicroPython + webserver próprio | ESP32 Battery Kit | Wi-Fi AP `ESP32_TEMP`, HTTP | ![Kit](./N06_Sensor_Campo_P/esp32_battery.png) |
| N07 · Estufa Artigo | `N07_Estufa_Artigo_C` | ESP-IDF (C) p/ ESP32-S3 com PSRAM | ESP32-S3 + câmera visual + MLX90640 + SD | Wi-Fi STA, HTTPS, NTP, SD | ![ESP32-S3](./N07_Estufa_Artigo_C/imagens/esp32s3.jpg) |

> Dica: alguns projetos guardam múltiplas fotos em `imagens/`. A tabela mostra apenas a principal; veja a pasta para mais ângulos.

---

## 🔹 Notas rápidas por nó

### N01 · Estufa Germinar (`N01_Estufa_Germinar_C`)
- **Objetivo:** monitorar clima/solo na fase de germinação e acionar relés.
- **Arquitetura:** módulos `conexoes`, `sensores`, `atuadores` + certificados TLS.
- **Protocolos:** Wi-Fi AP/STA, MQTT (`mqtt.greense.com.br:8883`), HTTP embarcado.
- **Doc:** `N01_Estufa_Germinar_C/README.md`.

### N02 · Estufa Maturar (`N02_Estufa_Maturar_C`)
- **Objetivo:** acompanhar reservatórios, claridade e status visual (LED RGB) na fase de maturação.
- **Destaques:** boias, DHT22 externo, DS18B20 interno/externo, JSON periódico via MQTT seguro.
- **Doc:** `N02_Estufa_Maturar_C/README.md`.

### N03 · Estufa MicroPython (`N03_Estufa_P`)
- **Objetivo:** nó híbrido com sensores AHT20/ENS160, pH, EC e controle de bomba.
- **Componentes:** classes `Config`, `Conexao`, `SensorManager`, `ActuatorManager`.
- **Imagem:** não fornecida (adicionar aqui quando disponível).
- **Doc:** código comentado em `main.py` e arquivos auxiliares.

### N04 · Estufa Câmera (`N04_Estufa_Camera_C`)
- **Objetivo:** capturar JPEGs (OV2640), salvar em SD e enviar via HTTPS.
- **Extras:** flash GPIO4, LED status GPIO33, certificado TLS embutido.
- **Doc:** `N04_Estufa_Camera_C/README.md`.

### N05 · Estufa Térmica (ESP-IDF) (`N05_Estufa_Termica_C`)
- **Objetivo:** adquirir frames do MLX90640, converter para °C e enviar JSON agendado.
- **Arquitetura:** BSP/APP/GUI, sincronização NTP, LED GPIO8, pipeline robusto de retries.
- **Imagens:** ver `N05_Estufa_Termica_C/imagens/` (sensor, placa, mapas térmicos).
- **Doc:** `N05_Estufa_Termica_C/README.md`.

### N05 · Estufa Térmica (MicroPython) (`N05_Estufa_Termica_P`)
- **Objetivo:** versão leve em MicroPython do mesmo termovisor.
- **Configuração:** ajustes diretamente no topo de `main.py` (SSID, URL, intervalo).
- **Doc:** `N05_Estufa_Termica_P/README.md`.

### N06 · Sensor de Campo ESP-IDF (`N06_Sensor_Campo_C`)
- **Objetivo:** nó autônomo com AP próprio, dashboard em HTTP e logger SPIFFS.
- **Sensores:** temperatura/umidade ar, DS18B20 (solo), sensor resistivo/capacitivo de umidade.
- **Imagens:** kits e dashboards em `N06_Sensor_Campo_C/imagens/`.
- **Doc:** `N06_Sensor_Campo_C/README.md`.

### N06 · Sensor de Campo 2 (ESP-IDF) (`N06_Sensor_Campo_2_C`)
- **Objetivo:** módulo compacto de solo totalmente offline, com AP `greenSe_Campo`, dashboard interativo e log em SPIFFS.
- **Sensores:** solo capacitivo (ADC32), DS18B20 (solo), DHT11 (ar), BH1750 (luminosidade) e LED de status (GPIO2).
- **Recursos:** mDNS `greense.local`, calibração de solo, tolerâncias configuráveis, gráficos em tempo real e download de CSV.
- **Imagens:** ver `N06_Sensor_Campo_2_C/imagens/` (módulo, sensores e telas).
- **Doc:** `N06_Sensor_Campo_2_C/README.md`.

### N06 · Sensor de Campo MicroPython (`N06_Sensor_Campo_P`)
- **Objetivo:** logger rápido com AP `ESP32_TEMP`, interface HTML e calibração via browser.
- **Arquivos-chave:** `main.py`, `sensores/`, `webserver/http_server.py`.
- **Doc:** comentários no código + arquivo `log_temp.csv` de exemplo.

### N07 · Estufa Artigo (`N07_Estufa_Artigo_C`)
- **Objetivo:** plataforma premium (ESP32-S3) com câmera visual + MLX90640, SD card e LED WS2812.
- **Recursos:** agendamentos independentes, SPIFFS→SD, scripts Python (`visualize_thermal.py`).
- **Imagens:** conferir `N07_Estufa_Artigo_C/imagens/` (visual, térmico, estufas reais).
- **Doc:** `N07_Estufa_Artigo_C/README.md`, `README_THERMAL.md`, `ANALISE_CONFIABILIDADE.md`.

---

## ✅ Como usar
1. Use a tabela para identificar o nó e a imagem correta.
2. Leia a nota rápida para entender sensores, protocolos e particularidades.
3. Entre no diretório correspondente e siga o README local para build/flash.
4. Atualize este catálogo sempre que novas fotos ou nós (N08, N09, …) forem criados.
