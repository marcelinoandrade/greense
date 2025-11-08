import numpy as np
import matplotlib.pyplot as plt
import csv
import os # Adicionado para verificar se o arquivo existe

# --- Configuração ---
# ATENÇÃO: Verifique se este é o nome correto do arquivo que você está salvando
# No script anterior, definimos como "dados_termicos_acumulados.csv"
# O script que você colou usa "termica_global.csv"
ARQUIVO_CSV = "dados_termicos_acumulados.csv" 
ARQUIVO_CSV = "termica_global.csv" # <- Use este se for o nome correto

# Limites de temperatura para a escala de cor
VMIN = 20
VMAX = 40
# --- Fim da Configuração ---

# Verificar se o arquivo existe
if not os.path.exists(ARQUIVO_CSV):
    print(f"❌ Erro: Arquivo não encontrado no caminho: {ARQUIVO_CSV}")
    print("Verifique a variável ARQUIVO_CSV neste script.")
    exit()

# Ler CSV completo
try:
    with open(ARQUIVO_CSV, "r", encoding='utf-8') as f:
        reader = list(csv.reader(f))
except Exception as e:
    print(f"❌ Erro ao ler o arquivo CSV: {e}")
    exit()

if len(reader) < 2:
    print("❌ Erro: O arquivo CSV está vazio ou contém apenas o cabeçalho.")
    exit()

header = reader[0]
dados = reader[1:]

# Colunas de temperatura (a partir da 3ª coluna, índice 2)
colunas_temp = header[2:]

if not colunas_temp:
    print("❌ Erro: O arquivo CSV não contém colunas de temperatura (deve ter mais de 2 colunas).")
    exit()

# Pegar as últimas 36 colunas para plotar
ultimas_colunas = colunas_temp[-36:]
num_imagens = len(ultimas_colunas)
print(f"Total de {num_imagens} imagens para plotar.")

# Dimensões da câmera térmica
n_linhas, n_colunas = 24, 32

# Figura 6x6
fig, axs = plt.subplots(6, 6, figsize=(18, 18), facecolor='black') # Fundo preto
axs_flat = axs.flatten()

# --- MUDANÇA DO COLORMAP ---
# Trocado de 'summer' para 'viridis'
# 'viridis' é um colormap perceptualmente uniforme, ótimo para realçar diferenças.
cmap = plt.cm.viridis
cmap.set_under('black') # O que estiver abaixo de VMIN (20°C) fica preto
cmap.set_over('white') # O que estiver acima de VMAX (40°C) fica branco/amarelo brilhante
# --- Fim da Mudança ---

for idx, nome_coluna in enumerate(ultimas_colunas):
    ax = axs_flat[idx]

    try:
        col_index = header.index(nome_coluna)
        # Converte para float, tratando possíveis erros de formatação
        vetor_temp = [float(linha[col_index].replace(",", ".")) for linha in dados]

    except (ValueError, IndexError) as e:
        print(f"Erro ao processar dados da coluna {nome_coluna}: {e}")
        ax.set_title(f"Erro nos dados: {nome_coluna}", fontsize=8, color='red')
        ax.set_facecolor('gray')
        ax.axis('off')
        continue

    # Converte para matriz
    matriz = np.array(vetor_temp).reshape((n_linhas, n_colunas))

    # Rotaciona 180°
    matriz_rotacionada = np.rot90(matriz, k=2)

    # Menos suavização (usar 'nearest' para um visual mais "pixelado")
    im = ax.imshow(
        matriz_rotacionada,
        cmap=cmap,
        aspect="auto",
        interpolation='bilinear', # MUDANÇA: 'nearest' (pixelado) -> 'bilinear' (suave)
        vmin=VMIN,
        vmax=VMAX
    )

    # Títulos e eixos com cores claras para contrastar com o fundo preto
    ax.set_title(f"{nome_coluna}", fontsize=8, color='white')
    ax.set_xlabel("Coluna", fontsize=6, color='gray')
    ax.set_ylabel("Linha", fontsize=6, color='gray')
    
    # Cor dos números dos eixos (ticks)
    ax.tick_params(axis='x', colors='gray', labelsize=6)
    ax.tick_params(axis='y', colors='gray', labelsize=6)
    
    # Cor da borda do gráfico
    for spine in ax.spines.values():
        spine.set_edgecolor('gray')

    # Colorbar
    cbar = plt.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
    cbar.ax.tick_params(colors='white', labelsize=6) # Cor dos números do colorbar

# Desliga eixos extras
if num_imagens < 36:
    for i in range(num_imagens, 36):
        axs_flat[i].axis('off')

plt.suptitle(f"Últimas {num_imagens} Imagens Térmicas (Suavizadas: {VMIN}°C–{VMAX}°C)", fontsize=16, color='white')
plt.tight_layout(rect=[0, 0.03, 1, 0.98]) # Ajusta para o supertítulo
plt.show()