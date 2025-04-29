# GreenSe - Software para Cultivo Protegido em Ambiente Controlado

<div align="center">
  <img src="https://github.com/marcelinoandrade/greense/blob/main/logo_greense.svg" alt="GreenSe Logo" width="150">
</div>

O **GreenSe** é uma solução inovadora para **monitoramento e automação de cultivos protegidos**, com foco em **hidroponia** e outros sistemas em ambiente controlado. Integrando sensores, atuadores e inteligência artificial, o GreenSe otimiza as condições de cultivo, promovendo maior eficiência no uso de água, nutrientes e energia. Saiba mais em [www.greense.com.br](https://www.greense.com.br).

---

## Funcionalidades Principais

- **Monitoramento em tempo real** de temperatura, umidade, pH, condutividade elétrica, nível de CO₂ e luminosidade.
- **Automação inteligente** para irrigação, ventilação, iluminação e controle ambiental.
- **Registro e análise contínua de dados** para otimização progressiva das condições de cultivo.
- **Interface web intuitiva** para visualização e controle das estufas.
- **Comunicação segura** via **MQTT** sobre **WebSocket seguro (WSS)** com **TLS**, usando **Cloudflared** para acesso remoto seguro.
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
- **Análise automática de imagens** para detecção precoce de pragas e doenças (em desenvolvimento).

Essa camada de inteligência torna o GreenSe mais autônomo, eficiente e capaz de reduzir a necessidade de intervenção manual no dia a dia do cultivo.


---

## Tecnologias Utilizadas

### Hardware
- Sensores de umidade, temperatura, luminosidade e CO₂.
- Atuadores para irrigação, ventilação e iluminação.
- Microcontrolador **ESP32** para coleta e comunicação de dados.
- **Raspberry Pi** para processamento e armazenamento local.
- Estufas como ambiente de cultivo controlado.

### Software e Servidores
- **C/C++** para desenvolvimento de firmware de produção.
- **Python (MicroPython)** para prototipagem de sistemas.
- **Flask** para disponibilização de API REST leve e segura.
- **MQTT** com **WebSocket seguro (WSS)** para comunicação de dados.
- **InfluxDB** para armazenamento de séries temporais.
- **Grafana** para dashboards interativos.
- **Cloudflared** para tunelamento seguro de conexões.
- **NGINX** para hospedagem da página oficial.
- **OpenAI GPT-4o** para suporte de IA no Eng. GePeTo.

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
