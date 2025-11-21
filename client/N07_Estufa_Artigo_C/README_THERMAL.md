# Visualização de Dados Térmicos

Script Python para visualizar os arquivos binários gerados pela câmera térmica MLX90640.

## Instalação

```bash
pip install numpy matplotlib pillow
```

## Uso Básico

```bash
# Visualizar um arquivo (exibe as imagens)
python visualize_thermal.py THM12345.BIN

# Salvar imagens em um diretório
python visualize_thermal.py THM12345.BIN --output-dir ./output

# Especificar número de frames (se diferente do padrão)
python visualize_thermal.py THM12345.BIN --frames 2

# Criar animação GIF
python visualize_thermal.py THM12345.BIN --gif animation.gif

# Usar diferentes mapas de cores
python visualize_thermal.py THM12345.BIN --colormap viridis
```

## Formato dos Dados

- **Câmera**: MLX90640
- **Resolução**: 24 linhas × 32 colunas = 768 pixels
- **Formato**: Floats (4 bytes cada)
- **Estrutura**: `THERMAL_SAVE_INTERVAL` frames consecutivos
- **Tamanho do arquivo**: `THERMAL_SAVE_INTERVAL × 768 × 4 bytes`

## Exemplos

### Visualizar e salvar todas as imagens
```bash
python visualize_thermal.py THM12345.BIN --output-dir ./thermal_images
```

### Criar animação de todos os frames
```bash
python visualize_thermal.py THM12345.BIN --gif thermal_animation.gif --frames 2
```

### Processar múltiplos arquivos
```bash
for file in THM*.BIN; do
    python visualize_thermal.py "$file" --output-dir ./output --no-display
done
```

## Mapas de Cores Disponíveis

- `hot` (padrão) - Preto → Vermelho → Amarelo → Branco
- `jet` - Azul → Verde → Amarelo → Vermelho
- `viridis` - Roxo → Azul → Verde → Amarelo
- `plasma` - Roxo → Rosa → Amarelo
- `inferno` - Preto → Roxo → Amarelo
- `magma` - Preto → Roxo → Branco
- `coolwarm` - Azul (frio) → Branco → Vermelho (quente)

## Saída

O script gera:
- Imagens PNG individuais para cada frame
- Estatísticas de temperatura (mínima, máxima, média)
- Opcionalmente, uma animação GIF

Cada imagem inclui:
- Visualização térmica com mapa de cores
- Barra de cores com escala em °C
- Informações de temperatura no rodapé

