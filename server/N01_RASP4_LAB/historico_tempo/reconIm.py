import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
from matplotlib.cm import ScalarMappable
import csv
import os
import math

# --- Parâmetros do GRID INICIAL (SELETOR) ---
LINHAS, COLUNAS = 4, 4# ajuste aqui (ex.: 5,5)
N = LINHAS * COLUNAS
# --------------------------------------------

ARQUIVO_CSV = "termica_global.csv"
VMIN, VMAX = 22, 32 

# --- Dimensões da imagem térmica ---
# (altere aqui se sua matriz de sensor for diferente, ex: 8x8)
IMG_LINHAS, IMG_COLUNAS = 24, 32
# -----------------------------------

# --- Variáveis globais para seleção ---
imagens_selecionadas = []  # Armazena tuplas (nome_coluna, matriz)
mapeamento_axs = {}        # Mapeia {ax: (nome_coluna, matriz)}
# --------------------------------------

def carregar_dados(arquivo_csv):
    """Carrega e valida os dados do arquivo CSV."""
    if not os.path.exists(arquivo_csv):
        print(f"❌ Erro: Arquivo não encontrado no caminho: {arquivo_csv}")
        return None, None, None

    with open(arquivo_csv, "r", encoding="utf-8") as f:
        reader = list(csv.reader(f))

    if len(reader) < 2:
        print("❌ Erro: O arquivo CSV está vazio ou contém apenas o cabeçalho.")
        return None, None, None

    header = reader[0]
    dados = reader[1:]
    colunas_temp = header[2:]  # As duas primeiras são 'timestamp' e 'sensor'

    if not colunas_temp:
        print("❌ Erro: Nenhuma coluna de temperatura encontrada no cabeçalho.")
        return None, None, None

    return header, dados, colunas_temp

def plotar_imagem(ax, matriz, nome_coluna, cmap, vmin, vmax):
    """Função auxiliar para plotar uma única imagem térmica no eixo (ax)."""
    ax.clear()  # Limpa o eixo caso esteja sendo redesenhado

    im = ax.imshow(
        matriz,
        cmap=cmap,
        aspect="auto",
        interpolation="bicubic",  # suaviza a imagem
        vmin=vmin, vmax=vmax
    )

    ax.set_title(f"{nome_coluna}", fontsize=8, color="white")
    ax.set_xlabel("Coluna", fontsize=6, color="gray")
    ax.set_ylabel("Linha", fontsize=6, color="gray")

    ax.xaxis.set_major_locator(MaxNLocator(nbins=6, integer=True, prune="both"))
    ax.yaxis.set_major_locator(MaxNLocator(nbins=6, integer=True, prune="both"))

    ax.tick_params(axis="x", colors="gray", labelsize=6)
    ax.tick_params(axis="y", colors="gray", labelsize=6)

    for spine in ax.spines.values():
        spine.set_edgecolor("gray")
        spine.set_linewidth(0.8)
    
    return im

def on_click(event):
    """Função chamada quando o mouse é clicado na figura."""
    ax = event.inaxes
    if ax is None or ax not in mapeamento_axs:
        # Ignora cliques fora dos subplots
        return

    # Recupera os dados associados a este eixo
    nome, matriz = mapeamento_axs[ax]
    item_selecionado = (nome, matriz)

    if item_selecionado in imagens_selecionadas:
        # --- DESMARCAR ---
        imagens_selecionadas.remove(item_selecionado)
        # Restaura a aparência original
        ax.set_title(f"{nome}", fontsize=8, color="white")
        for spine in ax.spines.values():
            spine.set_edgecolor("gray")
    else:
        # --- MARCAR ---
        imagens_selecionadas.append(item_selecionado)
        # Destaca o subplot selecionado
        ax.set_title(f"[ {nome} ]", fontsize=8, color="yellow")
        for spine in ax.spines.values():
            spine.set_edgecolor("yellow")
            
    # Redesenha a figura para mostrar a seleção
    event.canvas.draw()

# --- 1. Carregamento de Dados ---
header, dados, colunas_temp = carregar_dados(ARQUIVO_CSV)
if header is None:
    exit()

# Seleciona as colunas para o plot seletor (baseado no seu N)
colunas_para_selecao = colunas_temp[-N:]
num_imagens_seletor = len(colunas_para_selecao)

if num_imagens_seletor == 0:
    print("❌ Erro: Não há imagens suficientes para exibir no grid seletor.")
    exit()

# --- 2. Configuração do Plot Seletor (Plot 1) ---
print(f"Abrindo seletor com {num_imagens_seletor} imagens. Clique para selecionar e feche a janela para continuar.")

fig_w = max(8, 3.6 * COLUNAS)
fig_h = max(8, 3.2 * LINHAS)

fig, axs = plt.subplots(
    LINHAS, COLUNAS,
    figsize=(fig_w, fig_h),
    facecolor="black",
    constrained_layout=True
)

axs_flat = axs.ravel() if isinstance(axs, np.ndarray) else np.array([axs])
fig.set_constrained_layout_pads(h_pad=0.03, w_pad=0.03, hspace=0.08, wspace=0.08)

cmap = plt.cm.inferno
cmap.set_under("black")
cmap.set_over("white")

# --- 3. Plotagem das Imagens no Seletor ---
for idx, nome_coluna in enumerate(colunas_para_selecao):
    ax = axs_flat[idx]
    try:
        col_index = header.index(nome_coluna)
        # Lê o vetor de temperaturas e converte (assume vírgula como decimal)
        vetor_temp = [float(linha[col_index].replace(",", ".")) for linha in dados]
        
        # Valida se o vetor tem o tamanho esperado
        if len(vetor_temp) != (IMG_LINHAS * IMG_COLUNAS):
            raise ValueError(f"Tamanho inesperado de dados: {len(vetor_temp)}")
            
        matriz = np.array(vetor_temp).reshape((IMG_LINHAS, IMG_COLUNAS))
        
        # Plota a imagem
        plotar_imagem(ax, matriz, nome_coluna, cmap, VMIN, VMAX)
        
        # Armazena os dados para o clique
        mapeamento_axs[ax] = (nome_coluna, matriz)

    except (ValueError, IndexError) as e:
        ax.set_title(f"Erro: {nome_coluna}\n{e}", fontsize=8, color="red")
        ax.axis("off")
        continue

# Desliga eixos não usados no seletor
for i in range(num_imagens_seletor, N):
    axs_flat[i].axis("off")

# Título do Seletor
fig.suptitle(
    f"SELETOR DE IMAGENS: Clique para selecionar/desmarcar • Feche esta janela para gerar o plot final",
    fontsize=16, color="yellow"
)

# Conecta o evento de clique
# <--- CORREÇÃO APLICADA AQUI --->
fig.canvas.mpl_connect('button_press_event', on_click)

# Mostra o Plot Seletor. O script pausa aqui até a janela ser fechada.
plt.show()

# --- 4. Verificação Pós-Seleção ---
if not imagens_selecionadas:
    print("\nNenhuma imagem foi selecionada. Encerrando.")
    exit()

N_selecionadas = len(imagens_selecionadas)
print(f"\n{N_selecionadas} imagens selecionadas. Gerando plot final...")

# --- 5. Configuração do Plot Final (Plot 2) ---

# Calcula o grid ótimo para as imagens selecionadas
COLUNAS_novo = int(math.ceil(math.sqrt(N_selecionadas)))
LINHAS_novo = int(math.ceil(N_selecionadas / COLUNAS_novo))

fig_w_novo = max(8, 3.6 * COLUNAS_novo)
fig_h_novo = max(8, 3.2 * LINHAS_novo)

fig2, axs2 = plt.subplots(
    LINHAS_novo, COLUNAS_novo,
    figsize=(fig_w_novo, fig_h_novo),
    facecolor="black",
    constrained_layout=True
)

axs_flat_novo = axs2.ravel() if isinstance(axs2, np.ndarray) else np.array([axs2])
fig2.set_constrained_layout_pads(h_pad=0.03, w_pad=0.03, hspace=0.08, wspace=0.08)

# --- 6. Plotagem das Imagens Selecionadas ---
for idx, (nome, matriz) in enumerate(imagens_selecionadas):
    ax = axs_flat_novo[idx]
    plotar_imagem(ax, matriz, nome, cmap, VMIN, VMAX)

# Desliga eixos não usados no plot final
for i in range(N_selecionadas, LINHAS_novo * COLUNAS_novo):
    axs_flat_novo[i].axis("off")

# Colorbar único para o plot final
sm = ScalarMappable(cmap=cmap)
sm.set_clim(VMIN, VMAX)
sm.set_array([])
cbar = fig2.colorbar(sm, ax=axs2, location="right", fraction=0.03, pad=0.02)
cbar.ax.tick_params(colors="white", labelsize=8)

fig2.suptitle(
    f"Imagens Térmicas Selecionadas ({N_selecionadas}) • {VMIN}°C–{VMAX}°C",
    fontsize=16, color="white"
)

# Mostra o Plot Final
plt.show()

print("\nConcluído.")     