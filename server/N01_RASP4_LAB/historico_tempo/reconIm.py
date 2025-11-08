import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
from matplotlib.cm import ScalarMappable
import csv, os

# --- Parâmetros do GRID ---
LINHAS, COLUNAS = 2, 2   # ajuste aqui (ex.: 5,5)
N = LINHAS * COLUNAS
# --------------------------

ARQUIVO_CSV = "termica_global.csv"
VMIN, VMAX = 20, 40

if not os.path.exists(ARQUIVO_CSV):
    print(f"❌ Erro: Arquivo não encontrado no caminho: {ARQUIVO_CSV}")
    exit()

with open(ARQUIVO_CSV, "r", encoding="utf-8") as f:
    reader = list(csv.reader(f))

if len(reader) < 2:
    print("❌ Erro: O arquivo CSV está vazio ou contém apenas o cabeçalho.")
    exit()

header = reader[0]
dados = reader[1:]
colunas_temp = header[2:]

ultimas_colunas = colunas_temp[-N:]
num_imagens = len(ultimas_colunas)
print(f"Total de {num_imagens} imagens para plotar em {LINHAS}x{COLUNAS}.")

n_linhas, n_colunas = 24, 32
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

# colormap com contraste reforçado
cmap = plt.cm.inferno  # contraste alto e suave
cmap.set_under("black")
cmap.set_over("white")

for idx, nome_coluna in enumerate(ultimas_colunas):
    ax = axs_flat[idx]
    try:
        col_index = header.index(nome_coluna)
        vetor_temp = [float(linha[col_index].replace(",", ".")) for linha in dados]
    except (ValueError, IndexError):
        ax.set_title(f"Erro: {nome_coluna}", fontsize=8, color="red")
        ax.axis("off")
        continue

    matriz = np.array(vetor_temp).reshape((n_linhas, n_colunas))

    # imagem mais suave e com contraste realçado
    im = ax.imshow(
        matriz,
        cmap=cmap,
        aspect="auto",
        interpolation="bicubic",  # suaviza a imagem
        vmin=VMIN, vmax=VMAX
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

for i in range(num_imagens, N):
    axs_flat[i].axis("off")

# colorbar único
sm = ScalarMappable(cmap=cmap)
sm.set_clim(VMIN, VMAX)
sm.set_array([])
cbar = fig.colorbar(sm, ax=axs, location="right", fraction=0.03, pad=0.02)
cbar.ax.tick_params(colors="white", labelsize=8)

fig.suptitle(
    f"Últimas {num_imagens} Imagens Térmicas • Grid {LINHAS}×{COLUNAS} • {VMIN}°C–{VMAX}°C",
    fontsize=16, color="white"
)

plt.show()
