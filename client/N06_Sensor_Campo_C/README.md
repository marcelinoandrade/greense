# 🌱 Projeto GreenSe – Sensor de Campo IoT (ESP32)

Sistema embarcado desenvolvido com **ESP-IDF (v5.x)** para monitoramento ambiental e de solo, integrando sensores de temperatura, umidade e armazenamento local, com interface web embarcada em servidor HTTP.  

## ⚙️ Visão Geral

O projeto implementa um nó de coleta de dados ambientais e de solo para aplicações de **agricultura inteligente**.  

O firmware cria uma rede **Wi-Fi Access Point (AP)** local e hospeda uma página interativa acessível via navegador (`http://192.168.4.1/`), permitindo visualizar gráficos, calibrar sensores e baixar o histórico de medições em CSV.

### Funcionalidades principais

- 📡 Cria uma rede Wi-Fi local “ESP32_TEMP” com IP fixo `192.168.4.1`.
- 🌤️ Lê sensores de:
  - Temperatura e umidade do ar (AHT/DHT ou similar)
  - Temperatura do solo (DS18B20)
  - Umidade do solo (sensor resistivo ou capacitivo via ADC)
- 💾 Armazena leituras em `log_temp.csv` no **SPIFFS** e expõe JSON com histórico.
- 📈 Exibe **dashboard responsivo** com 4 gráficos e cards de status em tempo real.
- 🔁 Permite ajustar o **período de amostragem** (1 s, 1 min, 10 min, 1 h, 6 h, 12 h) diretamente na interface web.
- ⚙️ Possui **calibração guiada** da umidade do solo (parâmetros “seco” e “molhado”).
- ⬇️ Oferece **download direto** do log em CSV e limpeza total dos dados.
- 🧠 Quando algum sensor está ausente, gera dados simulados para manter o dashboard ativo.
- 🔧 Possui servidor HTTP leve com rotas dedicadas.

---

## 🧩 Estrutura de Diretórios

```
main/
├── app/
│   ├── app_main.c             # Inicialização, tarefas FreeRTOS e laço principal
│   ├── app_data_logger.c/.h   # Registro em SPIFFS e geração de JSON/CSV
│   ├── app_sensor_manager.c/.h# Integração com BSP dos sensores
│   ├── app_sampling_period.c/.h # Configuração dinâmica do período de amostragem (NVS)
│   └── gui_services.c/.h      # Ponte entre camada APP e GUI
├── bsp/
│   ├── board.h                # Definições da placa (GPIOs, SPIFFS, intervalos)
│   ├── sensors/               # Drivers DS18B20, ADC e camada `bsp_sensors.c`
│   └── network/               # SoftAP (`bsp_wifi_ap`)
├── gui/
│   └── web/
│       ├── gui_http_server.c  # Servidor HTTP e páginas HTML inline
│       └── gui_http_server.h
├── CMakeLists.txt             # Registro de fontes no componente `main`
└── README.md                  # Este arquivo
```

---

## 🖼️ Hardware de Referência

| ESP32-Battery|
|-----------------|
| ![ESP32](esp32_battery.png) |


## 🌐 Servidor Web Integrado

### Rotas HTTP

| Rota           | Método | Descrição |
|----------------|--------|-----------|
| `/`            | GET    | Painel principal (ação rápidas, branding greenSe Campo) |
| `/dashboard`   | GET    | Dashboard com cards, gráficos e leituras instantâneas |
| `/history`     | GET    | JSON com as últimas amostras para alimentar o dashboard |
| `/sampling`    | GET    | Página para escolher o período de amostragem (1 s até 12 h) |
| `/set_sampling`| GET    | Aplica o período selecionado (persistido em NVS) |
| `/calibra`     | GET    | Calibração guiada da umidade do solo |
| `/set_calibra` | GET    | Salva novos valores “seco/molhado” |
| `/download`    | GET    | Baixa `log_temp.csv` completo |
| `/clear_data`  | POST   | Limpa o log + calibração diretamente no dispositivo |
| `/favicon.ico` | GET    | Ícone da página (1×1 PNG) |

### Experiência da Interface Web

- **Painel principal**: cartão único com tag “greenSe Campo”, textos explicativos e botões para dashboard, amostragem, calibração, download e limpeza.
- **Dashboard**: hero com resumo das leituras, tabela textual e quatro gráficos personalizados desenhados via canvas.
- **Período de amostragem**: formulário com múltipla escolha (1 s → 12 h), descrições de impacto e botões responsivos.
- **Calibração**: cards destacando leitura bruta e faixa atual, inputs com labels claros, dica prática e botão verde padrão para retorno ao painel.

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
