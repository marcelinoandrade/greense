#!/usr/bin/env python3
"""
Exemplo simples de uso do script de visualização térmica
"""

from visualize_thermal import read_thermal_file, visualize_thermal_frame
import numpy as np

# Exemplo 1: Ler e visualizar um arquivo
def exemplo_basico():
    """Exemplo básico de leitura e visualização"""
    print("📂 Exemplo 1: Leitura básica")
    
    # Substitua pelo caminho do seu arquivo
    arquivo = "THM46455.BIN"
    
    try:
        # Lê os frames (assumindo 2 frames por padrão)
        frames = read_thermal_file(arquivo, thermal_save_interval=2)
        
        print(f"✅ {len(frames)} frame(s) lido(s)")
        
        # Visualiza cada frame
        for i, frame in enumerate(frames):
            temp_min = np.min(frame)
            temp_max = np.max(frame)
            temp_avg = np.mean(frame)
            
            print(f"\n📊 Frame {i+1}:")
            print(f"   Min: {temp_min:.2f}°C")
            print(f"   Max: {temp_max:.2f}°C")
            print(f"   Média: {temp_avg:.2f}°C")
            
            # Visualiza o frame
            visualize_thermal_frame(
                frame, 
                title=f"Frame Térmico #{i+1}",
                save_path=f"thermal_frame_{i+1}.png"
            )
            
    except FileNotFoundError:
        print(f"❌ Arquivo não encontrado: {arquivo}")
        print("   Certifique-se de que o arquivo existe no diretório atual")


# Exemplo 2: Processar múltiplos arquivos
def exemplo_multiplos_arquivos():
    """Exemplo de processamento em lote"""
    import glob
    
    print("\n📂 Exemplo 2: Processamento em lote")
    
    # Encontra todos os arquivos .BIN
    arquivos = glob.glob("THM*.BIN")
    
    if not arquivos:
        print("❌ Nenhum arquivo THM*.BIN encontrado")
        return
    
    print(f"📁 Encontrados {len(arquivos)} arquivo(s)")
    
    for arquivo in arquivos:
        print(f"\n📄 Processando: {arquivo}")
        try:
            frames = read_thermal_file(arquivo, thermal_save_interval=2)
            
            # Salva cada frame
            base_name = arquivo.replace(".BIN", "")
            for i, frame in enumerate(frames):
                visualize_thermal_frame(
                    frame,
                    title=f"{base_name} - Frame {i+1}",
                    save_path=f"{base_name}_frame_{i+1}.png"
                )
                
        except Exception as e:
            print(f"❌ Erro ao processar {arquivo}: {e}")


# Exemplo 3: Análise estatística
def exemplo_analise():
    """Exemplo de análise estatística dos dados"""
    print("\n📂 Exemplo 3: Análise estatística")
    
    arquivo = "THM46455.BIN"
    
    try:
        frames = read_thermal_file(arquivo, thermal_save_interval=2)
        
        # Concatena todos os frames para análise
        all_temps = np.concatenate([frame.flatten() for frame in frames])
        
        print(f"\n📊 Estatísticas Gerais ({len(frames)} frames):")
        print(f"   Temperatura Mínima: {np.min(all_temps):.2f}°C")
        print(f"   Temperatura Máxima: {np.max(all_temps):.2f}°C")
        print(f"   Temperatura Média:  {np.mean(all_temps):.2f}°C")
        print(f"   Desvio Padrão:     {np.std(all_temps):.2f}°C")
        print(f"   Mediana:           {np.median(all_temps):.2f}°C")
        
        # Análise por frame
        print(f"\n📈 Análise por Frame:")
        for i, frame in enumerate(frames):
            print(f"   Frame {i+1}:")
            print(f"      Média: {np.mean(frame):.2f}°C")
            print(f"      Std:   {np.std(frame):.2f}°C")
            
    except FileNotFoundError:
        print(f"❌ Arquivo não encontrado: {arquivo}")


if __name__ == '__main__':
    print("=" * 60)
    print("Exemplos de Uso - Visualização de Dados Térmicos")
    print("=" * 60)
    
    # Descomente o exemplo que deseja executar:
    
    #exemplo_basico()
    #exemplo_multiplos_arquivos()
    exemplo_analise()
    
    print("\n💡 Dica: Descomente os exemplos no código para executá-los")

