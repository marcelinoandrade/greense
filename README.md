# GreenSe - Solução para Cultivo Protegido

<div align="center">
  <img src="https://github.com/marcelinoandrade/greense/blob/main/dashboardGreense.jpg" alt="GreenSe Logo" width="800">
  
  **Sistema IoT para monitoramento e automação de estufas e cultivos hidropônicos**

  [![Website](https://img.shields.io/badge/website-greense.com.br-green)](https://www.greense.com.br)
  [![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
</div>

---

O **GreenSe** é uma solução completa de **agricultura de precisão** para cultivos protegidos, integrando sensores IoT, automação e inteligência artificial. Desenvolvido para estufas e sistemas hidropônicos, otimiza o uso de água, nutrientes e energia através de monitoramento em tempo real e análises preditivas.

## 🌟 Funcionalidades Principais

- **Monitoramento em tempo real** de parâmetros ambientais (temperatura, umidade, CO₂, luminosidade) e hidropônicos (pH, condutividade elétrica)
- **Análise térmica avançada** via câmera MLX90640 para detecção precoce de estresse hídrico e doenças
- **Controle automatizado** de irrigação, iluminação e sistemas de ventilação
- **IA Eng. GePeTo** com GPT-4o para análises técnicas e recomendações em linguagem acessível
- **Interface web** responsiva com dashboards interativos (Grafana) e relatórios automáticos
- **Comunicação segura** via MQTT/WSS com TLS para transmissão de dados
- **Arquitetura modular** e escalável de 1 a 1.000+ estufas

---

## 🤖 IA Eng. GePeTo

<div align="center">
  <img src="https://github.com/marcelinoandrade/greense/blob/main/gepeto.png" alt="Eng. Gepeto" width="100">
</div>

Agente de IA baseado em GPT-4o que atua como consultor agrícola virtual, fornecendo:

- **Relatórios automáticos** de status com interpretação dos dados coletados
- **Análises técnicas** em linguagem prática, simulando a atuação de um engenheiro agrícola
- **Recomendações** para otimização de irrigação, controle climático e nutrição
- **Modelos preditivos** para previsão de necessidades (em desenvolvimento)

---

## 🏗️ Arquitetura

### Cliente (Hardware IoT)

Soluções modulares baseadas em ESP32 para diferentes necessidades:

| Solução | Descrição | Tecnologia |
|---------|-----------|------------|
| **N01_Estufa_Germinar_C** | Monitoramento básico (germinação) | ESP32 + AHT20, ENS160, DS18B20 |
| **N02_Estufa_Maturar_C** | Monitoramento completo (maturação) | ESP32 + sensores completos + boias |
| **N03_Estufa_P** | Hidroponia com pH/EC | ESP32 + MicroPython |
| **N04_Estufa_Camera_C** | Monitoramento visual | ESP32-CAM |
| **N05_Estufa_Termica_C** | Análise térmica (24×32px) | ESP32-C3 + MLX90640 |
| **N06_Sensor_Campo_C** | Sensores de solo com bateria | ESP32 + interface web embarcada |
| **N07_Estufa_Artigo_C** | Solução completa térmica | ESP32-S3 + MLX90640 |

**Comunicação**: MQTT/TLS ou HTTP POST

### Servidor (Backend)

Sistema central em Python rodando em Raspberry Pi 4:

- **server01Full.py**: Broker MQTT, API REST (Flask), integração InfluxDB, processamento térmico
- **server01IA.py**: Serviço de IA (Eng. GePeTo) com análise de dados e geração de relatórios
- **serverTermica.py**: Processamento dedicado de imagens térmicas

**Stack**: Python 3.x, Flask, InfluxDB, MQTT (paho-mqtt), OpenAI GPT-4o, NumPy

---

## 🛠️ Tecnologias

### Hardware
- **Microcontroladores**: ESP32, ESP32-S3, ESP32-C3
- **Sensores**: AHT20, ENS160, DS18B20, DHT22, pH, EC, MLX90640
- **Servidor**: Raspberry Pi 4/5

### Software
- **Firmware**: C/C++ (ESP-IDF v5.x), MicroPython
- **Backend**: Python 3.x, Flask, InfluxDB, Grafana
- **Comunicação**: MQTT/WSS/TLS
- **IA**: OpenAI GPT-4o

---

## 📊 Escalabilidade

| Capacidade | Solução |
|:-----------|:--------|
| 1-20 estufas | Raspberry Pi 4 |
| 21-50 estufas | Raspberry Pi 5 |
| 51-1.000 estufas | Servidor dedicado |
| 1.000+ estufas | Arquitetura distribuída (Edge + Cloud) |

---

## 🚀 Início Rápido

1. **Clone o repositório:**
   ```bash
   git clone https://github.com/marcelinoandrade/greense.git
   cd greense
   ```

2. **Explore as soluções:**
   - Hardware: `client/` — cada projeto possui README específico
   - Servidor: `server/N01_RASP4_LAB/` — documentação de instalação

3. **Consulte a documentação:**
   - Cada módulo cliente possui README com instruções detalhadas
   - Requisitos e configuração específicos em cada diretório

---

## 📁 Estrutura do Repositório

```
greense/
├── client/                    # Soluções hardware (ESP32)
│   ├── N01_Estufa_Germinar_C/ # Monitoramento básico
│   ├── N02_Estufa_Maturar_C/  # Monitoramento completo
│   ├── N03_Estufa_P/          # Hidroponia (MicroPython)
│   ├── N04_Estufa_Camera_C/   # Câmera visual
│   ├── N05_Estufa_Termica_C/  # Câmera térmica
│   ├── N06_Sensor_Campo_C/    # Sensores de campo
│   └── N07_Estufa_Artigo_C/   # Solução completa térmica
├── server/                    # Sistema servidor
│   └── N01_RASP4_LAB/         # Backend Raspberry Pi 4
└── README.md                  # Este arquivo
```

---

## 📝 Licença

Este projeto está sob a licença MIT. Veja o arquivo [LICENSE](LICENSE) para mais detalhes.

---

## 📧 Contato

- **Website**: [www.greense.com.br](https://www.greense.com.br)
- **Email**: [contato@greense.com.br](mailto:contato@greense.com.br)
- **Coordenação**: Prof. Marcelino Monteiro de Andrade  
  Faculdade de Ciências e Tecnologias em Engenharia (FCTE) – Universidade de Brasília

---

<div align="center">
  <sub>Desenvolvido com ❤️ na Universidade de Brasília</sub>
</div>