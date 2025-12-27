# Upload de Presets de Cultivo

## Descrição

O sistema agora suporta upload de presets de cultivo personalizados via arquivo JSON. Isso permite adicionar, modificar ou remover presets sem precisar recompilar o firmware.

## Como Usar

### 1. Preparar o Arquivo JSON

Use o arquivo `presets_exemplo.json` como base. O formato deve seguir esta estrutura:

```json
{
  "presets": [
    {
      "id": "identificador_unico",
      "name": "Nome do Preset",
      "temp_ar_min": 20.0,
      "temp_ar_max": 30.0,
      "umid_ar_min": 50.0,
      "umid_ar_max": 80.0,
      "temp_solo_min": 18.0,
      "temp_solo_max": 25.0,
      "umid_solo_min": 40.0,
      "umid_solo_max": 80.0,
      "luminosidade_min": 500.0,
      "luminosidade_max": 2000.0,
      "dpv_min": 0.5,
      "dpv_max": 2.0
    }
  ]
}
```

### 2. Fazer Upload

1. Acesse a página **Cultivo** (`/calibra`)
2. Role até a seção **"Carregar Presets"**
3. Clique em **"Escolher arquivo"** e selecione seu arquivo JSON
4. Clique em **"Enviar Arquivo de Presets"**
5. Aguarde a confirmação de sucesso
6. A página será recarregada automaticamente com os novos presets

### 3. Usar os Presets

Após o upload, os presets estarão disponíveis no menu dropdown **"Escolha um tipo de planta"** na mesma página.

## Validações

O sistema valida automaticamente:
- ✅ Formato JSON válido
- ✅ Estrutura correta (array "presets" não vazio)
- ✅ Todos os campos numéricos obrigatórios presentes
- ✅ Valores mínimos < máximos para cada parâmetro

## Endpoints da API

- **GET `/presets.json`**: Retorna os presets atuais em formato JSON
- **POST `/upload_presets`**: Recebe arquivo JSON e salva em `/spiffs/presets.json`

## Fallback

Se o arquivo `/spiffs/presets.json` não existir ou estiver corrompido, o sistema usa automaticamente os presets hardcoded no firmware como fallback.

## Limitações

- Tamanho máximo do arquivo: 4KB
- Arquivo é salvo em SPIFFS (partição de armazenamento)
- Presets são carregados dinamicamente no frontend via JavaScript

## Exemplo de Preset Personalizado

```json
{
  "presets": [
    {
      "id": "pimenta",
      "name": "Pimenta",
      "temp_ar_min": 22.0,
      "temp_ar_max": 32.0,
      "umid_ar_min": 50.0,
      "umid_ar_max": 70.0,
      "temp_solo_min": 20.0,
      "temp_solo_max": 28.0,
      "umid_solo_min": 50.0,
      "umid_solo_max": 75.0,
      "luminosidade_min": 1200.0,
      "luminosidade_max": 3500.0,
      "dpv_min": 0.7,
      "dpv_max": 2.0
    }
  ]
}
```

