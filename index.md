---
layout: default
title: greenSe
---

<div align="center">

<a id="topo"></a>
<p align="center">
  <img src="greense.svg" alt="greenSe logo" width="520">
</p>

### Sistema IoT para Monitoramento e Automação de Estufas e Cultivos Hidropônicos

[![Website](https://img.shields.io/badge/🌐-Website-green?style=for-the-badge)](https://www.greense.com.br)
[![License](https://img.shields.io/badge/License-MIT-blue?style=for-the-badge)](LICENSE)
[![ESP32](https://img.shields.io/badge/ESP32-Ready-orange?style=for-the-badge&logo=espressif)](https://www.espressif.com/)
[![Python](https://img.shields.io/badge/Python-3.x-blue?style=for-the-badge&logo=python)](https://www.python.org/)
[![Raspberry Pi](https://img.shields.io/badge/Raspberry%20Pi-4-red?style=for-the-badge&logo=raspberry-pi)](https://www.raspberrypi.org/)

</div>

---

## 📖 Sobre o Projeto


O **greenSe** é uma solução completa de **agricultura de precisão** para cultivos protegidos, integrando sensores IoT, automação e inteligência artificial. Desenvolvido para estufas e sistemas hidropônicos, otimiza o uso de água, nutrientes e energia através de monitoramento em tempo real e análises preditivas.

### ✨ Destaques

<div align="center">

| 🌡️ Monitoramento | 🤖 Inteligência Artificial | 📊 Análise Térmica | 🔒 Segurança |
|:---:|:---:|:---:|:---:|
| Tempo real 24/7 | IA Eng. GePeTo (GPT-4o) | Câmera MLX90640 | MQTT/TLS/WSS |
| Múltiplos sensores | Relatórios automáticos | Detecção precoce | Comunicação criptografada |

</div>

---

## 🌟 Funcionalidades Principais

### 📡 Monitoramento Inteligente
- ⚡ **Tempo real** de parâmetros ambientais (temperatura, umidade, CO₂, luminosidade)
- 💧 **Hidroponia avançada** com pH e condutividade elétrica (EC)
- 🌡️ **Análise térmica** via câmera MLX90640 (24×32px) para detecção precoce de estresse hídrico e doenças
- 📷 **Monitoramento visual** com ESP32-CAM para acompanhamento do crescimento

### ⚙️ Automação e Controle
- 🔄 **Controle automatizado** de irrigação, iluminação e sistemas de ventilação
- 💾 **Armazenamento histórico** com InfluxDB para análise temporal
- 📈 **Dashboards interativos** com Grafana para visualização de dados

### 🤖 Inteligência Artificial

<div align="center">
  <img src="gepeto.png" alt="Eng. Gepeto" width="120">
</div>

A **IA Eng. GePeTo** (baseada em GPT-4o) atua como consultor agrícola virtual:

- 📝 **Relatórios automáticos** com interpretação técnica dos dados
- 🧠 **Análises inteligentes** em linguagem prática e acessível
- 💡 **Recomendações personalizadas** para otimização de irrigação, clima e nutrição
- 🔮 **Modelos preditivos** para previsão de necessidades (em desenvolvimento)

---

## 🏗️ Arquitetura do Sistema

### 🔌 Cliente (Hardware IoT)

Soluções modulares baseadas em **ESP32** para diferentes necessidades:

<div align="center">

| Solução | 📋 Descrição | 🔧 Tecnologia |
|:--------|:-------------|:--------------|
| **N01** | Germinação - Monitoramento básico | ESP32 + AHT20, ENS160, DS18B20 |
| **N02** | Maturação - Monitoramento completo | ESP32 + sensores completos + boias |
| **N03** | Hidroponia - pH e EC | ESP32 + MicroPython |
| **N04** | Câmera Visual - Monitoramento óptico | ESP32-CAM |
| **N05** | Câmera Térmica - Análise térmica | ESP32-C3 + MLX90640 (24×32px) |
| **N06** | Sensor de Campo - Solo com bateria | ESP32 + interface web embarcada |
| **N07** | Solução Completa - Térmica avançada | ESP32-S3 + MLX90640 |

</div>

**🌐 Comunicação**: MQTT/TLS ou HTTP POST

### 🖥️ Servidor (Backend)

Sistema central em **Python** rodando em **Raspberry Pi 4**:

| Módulo | Funcionalidade |
|:-------|:---------------|
| **server01Full.py** | 🔌 Broker MQTT • 🚀 API REST (Flask) • 💾 InfluxDB • 🌡️ Processamento térmico |
| **server01IA.py** | 🤖 Serviço IA (Eng. GePeTo) • 📊 Análise de dados • 📝 Geração de relatórios |
| **serverTermica.py** | 🌡️ Processamento dedicado de imagens térmicas • 📈 Estatísticas |

**🛠️ Stack Tecnológico**: Python 3.x • Flask • InfluxDB • MQTT (paho-mqtt) • OpenAI GPT-4o • NumPy

---

## 🛠️ Tecnologias Utilizadas

<div align="center">

### 🔩 Hardware

![ESP32](https://img.shields.io/badge/ESP32--WROOM--32-✓-orange?style=flat-square)
![ESP32-S3](https://img.shields.io/badge/ESP32--S3-✓-orange?style=flat-square)
![ESP32-C3](https://img.shields.io/badge/ESP32--C3-✓-orange?style=flat-square)
![Raspberry Pi](https://img.shields.io/badge/Raspberry%20Pi%204/5-✓-red?style=flat-square)

**Sensores**: AHT20 • ENS160 • DS18B20 • DHT22 • pH/EC • MLX90640

### 💻 Software

![C++](https://img.shields.io/badge/C/C++-ESP--IDF%20v5.x-blue?style=flat-square&logo=c%2B%2B)
![Python](https://img.shields.io/badge/Python-3.x-blue?style=flat-square&logo=python)
![MicroPython](https://img.shields.io/badge/MicroPython-ESP32-yellow?style=flat-square)
![Flask](https://img.shields.io/badge/Flask-API%20REST-black?style=flat-square&logo=flask)
![MQTT](https://img.shields.io/badge/MQTT/WSS-TLS-green?style=flat-square)
![InfluxDB](https://img.shields.io/badge/InfluxDB-Time%20Series-22ADF6?style=flat-square&logo=influxdb)
![Grafana](https://img.shields.io/badge/Grafana-Dashboards-F46800?style=flat-square&logo=grafana)
![OpenAI](https://img.shields.io/badge/OpenAI-GPT--4o-00A67E?style=flat-square&logo=openai)

</div>

---

## 📊 Escalabilidade

<div align="center">

| 🏭 Capacidade | 💻 Solução | ⚡ Performance |
|:--------------|:-----------|:---------------|
| **1-20 estufas** | Raspberry Pi 4 | Ideal para pequenos produtores |
| **21-50 estufas** | Raspberry Pi 5 | Médios produtores |
| **51-1.000 estufas** | Servidor dedicado | Grande escala |
| **1.000+ estufas** | Edge + Cloud | Arquitetura distribuída |

</div>

---

## 🚀 Início Rápido

### 1️⃣ Clone o Repositório

```bash
git clone https://github.com/marcelinoandrade/greense.git
cd greense
```

### 2️⃣ Explore as Soluções

- 📦 **Hardware**: Explore `client/` - cada projeto possui README específico com instruções detalhadas
- 🖥️ **Servidor**: Consulte `server/N01_RASP4_LAB/` para documentação de instalação e configuração

### 3️⃣ Documentação

Cada módulo cliente possui seu próprio README com:
- 📋 Requisitos de hardware
- 🔧 Instruções de instalação
- ⚙️ Configuração específica
- 🧪 Guia de testes

---

## 📁 Estrutura do Repositório

```
greense/
├── 📦 client/                          # Soluções hardware (ESP32)
│   ├── N01_Estufa_Germinar_C/         # 🌱 Monitoramento básico (germinação)
│   ├── N02_Estufa_Maturar_C/          # 🌿 Monitoramento completo (maturação)
│   ├── N03_Estufa_P/                  # 💧 Hidroponia (MicroPython)
│   ├── N04_Estufa_Camera_C/           # 📷 Câmera visual
│   ├── N05_Estufa_Termica_C/          # 🌡️ Câmera térmica
│   ├── N06_Sensor_Campo_C/            # 🌍 Sensores de campo
│   └── N07_Estufa_Artigo_C/           # 🔬 Solução completa térmica
├── 🖥️ server/                          # Sistema servidor
│   └── N01_RASP4_LAB/                 # Backend Raspberry Pi 4
└── 📄 README.md                        # Este arquivo
```

---

## 📝 Licença

Este projeto está sob a licença **MIT**. Veja o arquivo [LICENSE](LICENSE) para mais detalhes.

---

## 📧 Contato e Suporte

<div align="center">

| 🌐 Website | 💼 LinkedIn | 🏛️ Instituição |
|:----------|:--------|:---------------|
| [www.greense.com.br](https://www.greense.com.br) | [![LinkedIn](https://img.shields.io/badge/LinkedIn-Prof.%20Marcelino%20Andrade-0077B5?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/in/marcelino-andrade-b164b369) | Universidade de Brasília |

**Coordenação**: Prof. Marcelino Monteiro de Andrade  
**Faculdade**: Ciências e Tecnologias em Engenharia (FCTE) – Universidade de Brasília

</div>

---

<div align="center">

### 🌱 Desenvolvido na Universidade de Brasília

**greenSe** - Transformando a agricultura através da tecnologia IoT e inteligência artificial

[⬆️ Voltar ao topo](#topo)

</div>

