import matplotlib.pyplot as plt

# Ler o arquivo diretamente
with open('termica_19691231_210032.csv', 'r') as file:
    lines = file.readlines()

# Encontrar dimensões máximas
max_linha = 0
max_coluna = 0
data = []

for line in lines[1:]:  # Pular cabeçalho
    parts = line.strip().split(',')
    if len(parts) == 3:
        linha = int(parts[0])
        coluna = int(parts[1])
        temperatura = float(parts[2])
        
        data.append((linha, coluna, temperatura))
        
        if linha > max_linha:
            max_linha = linha
        if coluna > max_coluna:
            max_coluna = coluna

# Criar matriz usando listas Python
matriz_temperatura = []
for i in range(max_linha + 1):
    linha = [0] * (max_coluna + 1)
    matriz_temperatura.append(linha)

# Preencher a matriz
for linha, coluna, temperatura in data:
    matriz_temperatura[linha][coluna] = temperatura

# Plotar a imagem
plt.figure(figsize=(12, 8))
plt.imshow(matriz_temperatura, cmap='hot', aspect='auto')
plt.colorbar(label='Temperatura (°C)')
plt.title('Imagem Térmica - 27/10/1995 04:29:49')
plt.xlabel('Coluna')
plt.ylabel('Linha')
plt.grid(True, alpha=0.3)
plt.tight_layout()
plt.show()

print("Dimensões da imagem:", len(matriz_temperatura), "x", len(matriz_temperatura[0]))