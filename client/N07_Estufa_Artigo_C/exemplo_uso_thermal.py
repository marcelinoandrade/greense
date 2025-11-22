#!/usr/bin/env python3
"""
Exemplo simples de uso do script de visualização térmica
Formato atualizado: arquivos contêm timestamps Unix para cada frame
"""

from visualize_thermal import read_thermal_file, visualize_thermal_frame
import numpy as np

# Exemplo 1: Ler e visualizar um arquivo
def exemplo_basico():
    """Exemplo básico de leitura e visualização com timestamps"""
    print("📂 Exemplo 1: Leitura básica")
    
    # Substitua pelo caminho do seu arquivo
    arquivo = "THM0016L.BIN"  # Formato atual: THM#####L.BIN ou THM#####S.BIN
    
    try:
        # Lê os frames (assumindo 3 frames por padrão - THERMAL_SAVE_INTERVAL)
        frames = read_thermal_file(arquivo, thermal_save_interval=3)
        
        print(f"✅ {len(frames)} frame(s) lido(s)")
        
        # Visualiza cada frame
        for i, (timestamp, frame) in enumerate(frames):
            temp_min = np.min(frame)
            temp_max = np.max(frame)
            temp_avg = np.mean(frame)
            time_str = timestamp.strftime('%Y-%m-%d %H:%M:%S')
            
            print(f"\n📊 Frame {i+1}:")
            print(f"   Horário: {time_str}")
            print(f"   Min: {temp_min:.2f}°C")
            print(f"   Max: {temp_max:.2f}°C")
            print(f"   Média: {temp_avg:.2f}°C")
            
            # Visualiza o frame com timestamp
            visualize_thermal_frame(
                timestamp,
                frame, 
                title=f"Frame Térmico #{i+1} - {timestamp.strftime('%H:%M:%S')}",
                save_path=f"thermal_frame_{i+1}_{timestamp.strftime('%H%M%S')}.png"
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
            frames = read_thermal_file(arquivo, thermal_save_interval=3)
            
            # Salva cada frame com timestamp
            base_name = arquivo.replace(".BIN", "")
            for i, (timestamp, frame) in enumerate(frames):
                time_str = timestamp.strftime('%H%M%S')
                visualize_thermal_frame(
                    timestamp,
                    frame,
                    title=f"{base_name} - Frame {i+1} - {timestamp.strftime('%H:%M:%S')}",
                    save_path=f"{base_name}_frame_{i+1}_{time_str}.png"
                )
                
        except Exception as e:
            print(f"❌ Erro ao processar {arquivo}: {e}")
            import traceback
            traceback.print_exc()


# Exemplo 3: Análise estatística
def exemplo_analise():
    """Exemplo de análise estatística dos dados"""
    print("\n📂 Exemplo 3: Análise estatística")
    
    arquivo = "THM0017L.BIN"
    
    try:
        frames = read_thermal_file(arquivo, thermal_save_interval=3)
        
        # Concatena todos os frames para análise
        all_temps = np.concatenate([frame.flatten() for _, frame in frames])
        
        print(f"\n📊 Estatísticas Gerais ({len(frames)} frames):")
        print(f"   Temperatura Mínima: {np.min(all_temps):.2f}°C")
        print(f"   Temperatura Máxima: {np.max(all_temps):.2f}°C")
        print(f"   Temperatura Média:  {np.mean(all_temps):.2f}°C")
        print(f"   Desvio Padrão:     {np.std(all_temps):.2f}°C")
        print(f"   Mediana:           {np.median(all_temps):.2f}°C")
        
        # Análise por frame com timestamps
        print(f"\n📈 Análise por Frame:")
        for i, (timestamp, frame) in enumerate(frames):
            time_str = timestamp.strftime('%Y-%m-%d %H:%M:%S')
            print(f"   Frame {i+1} ({time_str}):")
            print(f"      Média: {np.mean(frame):.2f}°C")
            print(f"      Std:   {np.std(frame):.2f}°C")
            print(f"      Min:   {np.min(frame):.2f}°C")
            print(f"      Max:   {np.max(frame):.2f}°C")
            
    except FileNotFoundError:
        print(f"❌ Arquivo não encontrado: {arquivo}")


if __name__ == '__main__':
    print("=" * 60)
    print("Exemplos de Uso - Visualização de Dados Térmicos")
    print("=" * 60)
    
    # Descomente o exemplo que deseja executar:
    
    #exemplo_basico()
    exemplo_multiplos_arquivos()
    #exemplo_analise()
    
    print("\n💡 Dica: Descomente os exemplos no código para executá-los")

