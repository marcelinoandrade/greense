

# GreenSe - Software para Cultivo Protegido em Ambiente Controlado

<div align="center">
  <img src="https://github.com/marcelinoandrade/greense/blob/main/logo_greense.svg" alt="GreenSe Logo" width="200">
</div>

O **GreenSe** é uma solução inovadora para **monitoramento e automação de cultivos protegidos**, com foco em **hidroponia** e outros sistemas em ambiente controlado. Integrando sensores, atuadores e sistemas inteligentes, o GreenSe otimiza as condições de cultivo, garantindo maior eficiência no uso de água, nutrientes e energia. Saiba mais em [www.greense.com.br](https://www.greense.com.br).

---

## Funcionalidades Principais

Este sistema oferece monitoramento em tempo real de temperatura, umidade, pH, condutividade elétrica e outros parâmetros essenciais para o cultivo. Conta com automação inteligente para controle de irrigação, ventilação, iluminação e demais processos, tudo gerenciado por uma interface web intuitiva. Os dados são registrados e analisados continuamente, permitindo a otimização do cultivo ao longo do tempo. A solução integra tecnologias de IoT com dispositivos como ESP32 e Raspberry Pi, utilizando InfluxDB e Grafana para armazenamento e visualização dos dados. A comunicação é feita de forma segura via MQTT com WebSockets (WSS) protegidos por TLS, além de oferecer acesso remoto seguro por meio do Cloudflared. Sua arquitetura modular permite fácil expansão, possibilitando a adição de novas funcionalidades conforme as necessidades evoluem.

---

## Tecnologias Utilizadas

### Hardware
- **Sensores** para monitoramento de umidade, temperatura, luminosidade, CO₂, entre outros.
- **Atuadores** para controle de irrigação, ventilação, iluminação e outros.
- **ESP32 e ESP8266** para coleta de dados e comunicação.
- **Raspberry Pi** como servidor e processador de dados.
- **Estufas** como ambiente controlado de cultivo.

### Software & Servidores
- **C/C++** para backend e lógica de controle (Produção - ESP/IDE - VSCode).
- **Python** para backend e lógica de controle (Prototipagem - MicroPython - Thonny).
- **MQTT (WebSocket seguro - WSS)** para comunicação criptografada entre dispositivos e broker remoto.
- **InfluxDB** para armazenamento eficiente de dados temporais.
- **Grafana** para dashboards interativos.
- **Cloudflared** para conexão segura e acesso remoto.
- **NGINX** para hospedagem da página oficial.

---

## Objetivos do Projeto
O **GreenSe** busca aprimorar a **eficiência e sustentabilidade** dos cultivos protegidos, oferecendo uma solução confiável para produtores e pesquisadores. Entre os benefícios, destacam-se:

- **Redução do consumo de água e nutrientes** por meio de controle inteligente.
- **Melhoria na qualidade das plantas**, ajustando automaticamente o ambiente.
- **Facilidade de operação e escalabilidade** para diferentes tipos de cultivo.
- **Acesso remoto seguro**, permitindo monitoramento e controle em qualquer lugar.

---

## Expansão e Melhorias Futuras
O **GreenSe** é projetado para crescer e evoluir continuamente, garantindo escalabilidade e desempenho. As estratégias de expansão incluem:

- **Até 20 estufas (Atual)**: Utilização do **Raspberry Pi 4** para processamento local.  
- **Até 50 estufas**: Atualização para o **Raspberry Pi 5**, aumentando a capacidade de processamento.  
- **Até 1.000 estufas**: Implementação de um **servidor central dedicado** para maior eficiência.  
- **Mais de 1.000 estufas**: Uso de uma **arquitetura distribuída**, incluindo:  
  - **Edge Computing** para otimização de dados em tempo real.  
  - **Servidores em nuvem** para maior capacidade de armazenamento e processamento.  
  - **Banco de dados otimizado** para garantir rapidez e confiabilidade. 

---

## Aplicação de Inteligência Artificial
O **GreenSe** também prevê a integração de **IA** para aprimorar a automação e eficiência do cultivo. As principais aplicações incluem:

- **Modelos preditivos** para otimizar irrigação e uso de nutrientes.
- **Análise de imagens** para detecção automática de pragas e anomalias nas plantas.
- **Otimização de energia**, reduzindo custos e desperdícios no controle de ventilação e iluminação.

A implementação da **IA** tornará o **GreenSe** mais eficiente, automatizando processos e facilitando a tomada de decisões baseadas em dados.

---

## Como Contribuir
Este projeto está em desenvolvimento e **qualquer colaboração é bem-vinda!** Para contribuir, siga os passos abaixo:

1. **Clone o repositório**
   ```bash
   git clone https://github.com/seu-repositorio/greense.git

## Contato e Suporte
Para dúvidas ou sugestões, entre em contato através do nosso site oficial:
[www.greense.com.br](https://www.greense.com.br)

Juntos, podemos construir um futuro mais sustentável! 🚀
