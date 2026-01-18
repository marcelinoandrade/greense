# N06-C · Sensor de Campo · Monitoramento de Solo (ESP32)

Sistema embarcado para monitoramento ambiental e de solo em agricultura inteligente. Desenvolvido com **ESP-IDF v5.x**, integra múltiplos sensores com armazenamento local, interface web embarcada e análise estatística configurável.

<div align="center">
<img src="imagens/esp32_battery.png" width="200" alt="ESP32 com Bateria">
</div>

---

## Descrição Geral

Firmware que cria uma rede Wi-Fi Access Point local e hospeda interface web acessível via navegador. Sistema coleta dados de sensores ambientais e de solo, armazena em CSV e permite visualização em tempo real com gráficos, estatísticas e configuração de tolerâncias de cultivo.

**Acesso**: `http://greense.local/` ou `http://192.168.4.1/`  
**Rede Wi-Fi**: `greenSe_Campo` (senha: `12345678`)

---

## Sensores Implementados

| Sensor | Tipo | Interface | GPIO | Status |
|--------|------|-----------|------|--------|
| **DS18B20** | Temperatura do solo | OneWire | GPIO4 | ✅ |
| **ADC** | Umidade do solo | ADC | GPIO34 | ✅ |
| **AHT10** | Temperatura e umidade do ar | I2C | SDA:21, SCL:22 | ✅ |
| **BH1750** | Luminosidade | I2C | SDA:21, SCL:22 | ✅ |
| **DPV** | Déficit de Pressão de Vapor | Calculado | — | ✅ |

**Nota**: O sistema é robusto e continua funcionando mesmo com sensores ausentes, mantendo a última leitura válida ou retornando NAN.

---

## Funcionalidades

- Dashboard em tempo real com gráficos e estatísticas (min/max/média)
- Configuração de período de amostragem: 10s, 1min, 10min, 1h, 6h, 12h
- Janela estatística configurável: 5, 10, 15 ou 20 amostras
- Tolerâncias de cultivo personalizáveis para cada parâmetro (linhas de referência nos gráficos)
- Calibração do sensor de umidade do solo (valores seco/molhado)
- Download do histórico completo em CSV
- Limpeza de dados via interface web
- mDNS para acesso por nome (`greense.local`)
- LED de status indicando estado do AP e gravação

### Protocolo de Mudança de Frequência

Ao alterar o período de amostragem, o sistema:
1. Exibe confirmação com aviso sobre perda de dados
2. Limpa todos os dados registrados (garantindo integridade estatística)
3. Reinicia o dispositivo automaticamente

---

## Interface Web

### Rotas HTTP

| Rota | Método | Descrição |
|------|--------|-----------|
| `/` | GET | Painel principal com ações rápidas |
| `/dashboard` | GET | Dashboard com gráficos e estatísticas |
| `/history` | GET | JSON com histórico de amostras |
| `/sampling` | GET | Configuração de período e janela estatística |
| `/set_sampling` | GET | Aplica configurações de amostragem |
| `/calibra` | GET | Calibração do sensor de umidade |
| `/set_calibra` | GET | Salva valores de calibração |
| `/download` | GET | Download do arquivo CSV completo |
| `/clear_data` | POST | Limpa todos os dados registrados |

---

## Estrutura de Dados

### Arquivo CSV (`/spiffs/log_temp.csv`)

Formato do cabeçalho:
```
N,temp_ar_C,umid_ar_pct,temp_solo_C,umid_solo_pct,luminosidade_lux,dpv_kPa
```

Exemplo:
```
1,25.30,65.20,22.15,45.80,850.50,1.234
```

| Campo | Descrição | Unidade |
|-------|-----------|---------|
| `N` | Índice sequencial | — |
| `temp_ar_C` | Temperatura do ar | °C |
| `umid_ar_pct` | Umidade relativa do ar | % |
| `temp_solo_C` | Temperatura do solo | °C |
| `umid_solo_pct` | Umidade do solo calibrada | % |
| `luminosidade_lux` | Intensidade luminosa | lux |
| `dpv_kPa` | Déficit de Pressão de Vapor | kPa |

---

## Arquitetura

```
projeto/
├── CMakeLists.txt         # Configuração do projeto ESP-IDF
├── partitions.csv         # Tabela de partições
├── sdkconfig              # Configurações do ESP-IDF
├── idf_component.yml      # Dependências do Component Manager
├── main/
│   ├── CMakeLists.txt     # Registro de fontes do componente main
│   ├── idf_component.yml  # Dependências (cjson, mdns)
│   ├── app/               # Lógica de aplicação
│   │   ├── app_main.c                    # Inicialização e tarefas FreeRTOS
│   │   ├── app_data_logger.c/.h          # Armazenamento em SPIFFS
│   │   ├── app_sensor_manager.c/.h        # Gerenciamento de sensores
│   │   ├── app_sampling_period.c/.h      # Período de amostragem (NVS)
│   │   ├── app_stats_window.c/.h         # Janela estatística (NVS)
│   │   ├── app_cultivation_tolerance.c/.h # Tolerâncias configuráveis (NVS)
│   │   ├── app_atuadores.c/.h            # Controle de LED
│   │   └── gui_services.c/.h             # Interface APP ↔ GUI
│   ├── bsp/               # Board Support Package
│   │   ├── board.h                       # Configurações da placa
│   │   ├── sensors/                      # Drivers de sensores
│   │   ├── actuators/                    # Controle de atuadores
│   │   └── network/                      # Wi-Fi AP
│   └── gui/web/           # Interface web
│       └── gui_http_server.c/.h          # Servidor HTTP e páginas HTML
└── imagens/               # Imagens de hardware e interface
```

---

## Como Executar

### Requisitos

- ESP-IDF ≥ v5.0
- Python 3.x
- Ferramentas: `idf.py`, `esptool.py`

### Build e Flash

```bash
idf.py set-target esp32
idf.py build flash monitor
```

### Conexão

1. Conecte os sensores conforme pinagem
2. Conecte-se ao Wi-Fi **greenSe_Campo** (senha: `12345678`)
3. Acesse `http://greense.local/` ou `http://192.168.4.1/`

---

## Conexões

### I2C (Barramento Compartilhado)

- **SDA**: GPIO21
- **SCL**: GPIO22
- **VCC**: 3.3V
- **GND**: GND
- **Pull-ups**: 4.7kΩ (geralmente incluídos nos módulos)

**Sensores**:
- AHT10: endereço 0x38
- BH1750: endereço 0x23

### OneWire

- **DS18B20**: GPIO4 (com pull-up 4.7kΩ)

### ADC

- **Sensor de Umidade do Solo**: GPIO34 (ADC1_CH6)

### Outros

- **LED de Status**: GPIO2

---

## Componentes ESP-IDF

- `esp_wifi`, `esp_netif`, `esp_http_server`
- `esp_event`, `lwip`
- `esp_adc`, `nvs_flash`, `spiffs`, `driver`
- `freertos`, `esp_rom`, `vfs`
- `mdns` (via Component Manager: `espressif/mdns`)

---

## Testes

Testado em:
- ESP32-WROOM-32
- ESP32-S3

Navegadores validados:
- Chrome (Android e Desktop)
- Edge (Desktop)
- Samsung Browser

---

## Troubleshooting

### Sensores não detectados

**I2C**:
- Verifique conexões SDA/SCL (GPIO21/22)
- Confirme pull-ups de 4.7kΩ
- Verifique alimentação 3.3V e GND
- Confirme endereços: AHT10 (0x38), BH1750 (0x23)

**DS18B20**:
- Verifique pull-up de 4.7kΩ no GPIO4
- Confirme alimentação e GND

### Valores NAN

Comportamento esperado quando o sensor não está conectado. O sistema mantém a última leitura válida ou retorna NAN se nunca houve leitura.

### Erros HTTP 104 (Connection Reset)

Comportamento normal quando o navegador fecha a conexão. Não afeta o funcionamento do sistema.

---

## Licença

Este projeto faz parte do Projeto GreenSe da Universidade de Brasília.

**Autoria**: Prof. Marcelino Monteiro de Andrade  
**Instituição**: Faculdade de Ciências e Tecnologias em Engenharia (FCTE) – Universidade de Brasília  
**Email**: [andrade@unb.br](mailto:andrade@unb.br)  
**Website**: [https://greense.com.br](https://greense.com.br)
