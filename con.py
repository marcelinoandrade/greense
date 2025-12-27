import matplotlib.path as mpath
import matplotlib.textpath as mtextpath
import matplotlib.font_manager as mfont
import numpy as np

# --- CONFIGURAÇÕES ---
OUTPUT_FILE = "greense.svg"

# Cores e Estilos
LAYERS = [
    {
        "text": "Se",
        "size": 36,
        "x": 78,      # Posição ajustada (Fundo)
        "y": 44,      # Baseline
        "color": "#2E7D32", # Verde Escuro
        "z_index": 0
    },
    {
        "text": "green",
        "size": 28,
        "x": 10,      # Posição (Frente)
        "y": 44,      # Baseline
        "color": "#00C853", # Verde Claro
        "z_index": 1
    }
]

def find_font_path(font_name="Arial"):
    """Tenta encontrar o caminho real da fonte no sistema."""
    try:
        font = mfont.findfont(mfont.FontProperties(family=font_name, weight='bold'))
        print(f"Fonte usada: {font}")
        return font
    except:
        print("Fonte Arial não encontrada, usando padrão.")
        return None

def path_to_svg_d(text_path, offset_x, offset_y):
    """
    Converte um objeto Path do Matplotlib para uma string SVG 'd'.
    O Matplotlib usa Y crescrente (cartesiano), o SVG usa Y decrescente (tela).
    Precisamos inverter o Y da fonte.
    """
    parts = []
    
    # Iterar sobre os vértices e códigos do caminho
    for vertices, code in text_path.iter_segments():
        # Aplicar offset e inverter Y (para coordenadas de tela SVG)
        # Nota: O TextPath do matplotlib gera coordenadas locais. 
        # Precisamos virar o Y porque em SVG o Y cresce para baixo.
        x = vertices[0] + offset_x
        y = -vertices[1] + offset_y 
        
        # Comandos SVG
        if code == mpath.Path.MOVETO:
            parts.append(f"M {x:.2f} {y:.2f}")
        elif code == mpath.Path.LINETO:
            parts.append(f"L {x:.2f} {y:.2f}")
        elif code == mpath.Path.CURVE3: # Quadrática
            x1 = vertices[0] + offset_x
            y1 = -vertices[1] + offset_y
            x2 = vertices[2] + offset_x
            y2 = -vertices[3] + offset_y
            parts.append(f"Q {x1:.2f} {y1:.2f} {x2:.2f} {y2:.2f}")
        elif code == mpath.Path.CURVE4: # Cúbica (Bezier padrão)
            x1 = vertices[0] + offset_x
            y1 = -vertices[1] + offset_y
            x2 = vertices[2] + offset_x
            y2 = -vertices[3] + offset_y
            x3 = vertices[4] + offset_x
            y3 = -vertices[5] + offset_y
            parts.append(f"C {x1:.2f} {y1:.2f} {x2:.2f} {y2:.2f} {x3:.2f} {y3:.2f}")
        elif code == mpath.Path.CLOSEPOLY:
            parts.append("Z")
            
    return " ".join(parts)

def generate_svg():
    font_path = find_font_path("Arial")
    
    svg_content = [
        f'<svg width="128" height="64" viewBox="0 0 128 64" xmlns="http://www.w3.org/2000/svg">',
        f'  '
    ]
    
    # Ordenar por Z-index para desenhar na ordem certa
    layers_sorted = sorted(LAYERS, key=lambda k: k['z_index'])
    
    for layer in layers_sorted:
        # Criar o caminho do texto
        # prop=mfont.FontProperties(fname=font_path) garante que use a fonte do arquivo
        fp = mfont.FontProperties(fname=font_path, size=layer['size'])
        tpath = mtextpath.TextPath((0, 0), layer['text'], prop=fp)
        
        # Converter para string SVG
        # O Y no TextPath é relativo à baseline (0).
        # No SVG, queremos posicionar a baseline em layer['y']
        path_data = path_to_svg_d(tpath, layer['x'], layer['y'])
        
        svg_content.append(f'  ')
        svg_content.append(f'  <path d="{path_data}" fill="{layer["color"]}" />')

    svg_content.append('</svg>')
    
    full_svg = "\n".join(svg_content)
    
    with open(OUTPUT_FILE, "w") as f:
        f.write(full_svg)
        
    print(f"Sucesso! Arquivo '{OUTPUT_FILE}' gerado.")
    print("Este arquivo pode ser aberto em qualquer dispositivo e será idêntico.")

if __name__ == "__main__":
    generate_svg()