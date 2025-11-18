import os

class LoggerInterno:
    def __init__(self, path="/log_temp.csv"):
        self.path = path
        self.idx = self._ler_ultimo_indice()
        self._garantir_cabecalho()

    def _garantir_cabecalho(self):
        """Cria o arquivo se não existir e adiciona o cabeçalho."""
        if not self._arquivo_existe():
            with open(self.path, "w") as f:
                f.write("N,temp_ar_C,umid_ar_pct,temp_solo_C,umid_solo_pct\n")

    def _arquivo_existe(self):
        try:
            os.stat(self.path)
            return True
        except OSError:
            return False

    def _ler_ultimo_indice(self):
        """Lê o último índice salvo no arquivo e retorna o próximo."""
        if not self._arquivo_existe():
            return 0
        try:
            with open(self.path, "r") as f:
                lines = f.readlines()
                if len(lines) <= 1:
                    return 0
                ultima = lines[-1].strip()
                partes = ultima.split(",")
                return int(partes[0])
        except Exception:
            return 0

    def append(self, temp_ar, umid_ar, temp_solo, umid_solo):
        """Grava uma nova linha com índice sequencial."""
        self.idx += 1
        try:
            with open(self.path, "a") as f:
                linha = "{},{:.1f},{:.1f},{:.1f},{:.1f}\n".format(
                    self.idx, temp_ar, umid_ar, temp_solo, umid_solo
                )
                f.write(linha)
        except OSError as e:
            print("Erro ao gravar:", e)

    def ler_linhas(self):
        try:
            with open(self.path, "r") as f:
                return f.readlines()
        except OSError:
            return []
