import time
import json
import network
import socket

class TempWebServer:
    # agora recebe logger e guarda o caminho do CSV
    def __init__(self, logger, ap_ssid="ESP32_TEMP", ap_password="12345678"):
        self.logger = logger
        self.csv_path = logger.path  # ex: "/log_temp.csv"
        self.ip = None
        self.sock = None

        self._start_ap(ap_ssid, ap_password)
        self._start_http_server()

    # --------------------------------------------------
    # Access Point
    # --------------------------------------------------
    def _start_ap(self, ssid, password):
        ap = network.WLAN(network.AP_IF)
        ap.active(False)
        time.sleep_ms(200)
        ap.active(True)
        ap.config(essid=ssid, password=password, authmode=network.AUTH_WPA_WPA2_PSK)

        for _ in range(20):
            if ap.active():
                break
            time.sleep_ms(200)

        cfg = ap.ifconfig()
        self.ip = cfg[0]
        print("AP ativo. SSID =", ssid, "IP =", self.ip)
        self.ap = ap

    # --------------------------------------------------
    # Socket HTTP
    # --------------------------------------------------
    def _start_http_server(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(('', 80))
        s.listen(2)
        s.setblocking(False)
        self.sock = s
        print("Servidor HTTP escutando porta 80")

    # --------------------------------------------------
    # Utilitário para enviar resposta HTTP
    # --------------------------------------------------
    def _http_response(self, client, status, content_type, body, extra_headers=None):
        hdr = "HTTP/1.1 %s\r\nContent-Type: %s\r\nCache-Control: no-cache\r\nConnection: close\r\n" % (
            status,
            content_type
        )
        if extra_headers:
            for k, v in extra_headers.items():
                hdr += "%s: %s\r\n" % (k, v)
        hdr += "\r\n"

        client.sendall(hdr.encode("utf-8"))

        if isinstance(body, str):
            client.sendall(body.encode("utf-8"))
        else:
            client.sendall(body)

        client.close()

    # --------------------------------------------------
    # /history  -> dados já processados para gráfico
    # --------------------------------------------------
    def _read_history_as_json(self):
        lines = self.logger.ler_linhas()

        raw_pts = []
        for i, line in enumerate(lines):
            line = line.strip()
            if not line:
                continue
            if i == 0 and "timestamp_s" in line:
                continue

            parts = line.split(",")
            # timestamp_s,temp_ar_C,umid_ar_pct,temp_solo_C,umid_solo_pct
            if len(parts) < 2:
                continue

            try:
                ts = float(parts[0])
                temp_ar = float(parts[1])
            except:
                continue

            # filtro simples anti-ruído
            if ts < 1_000_000:
                continue
            if temp_ar < -40 or temp_ar > 100:
                continue

            raw_pts.append([ts, temp_ar])

        if not raw_pts:
            return json.dumps({"points": []})

        # ordena por timestamp
        raw_pts.sort(key=lambda p: p[0])

        # cada ponto = [indice_sequencial, temperatura]
        indexed_pts = []
        for idx, p in enumerate(raw_pts):
            indexed_pts.append([idx, p[1]])

        return json.dumps({"points": indexed_pts})

    # --------------------------------------------------
    # /download -> envia o CSV bruto
    # --------------------------------------------------
    def _send_csv_download(self, client):
        # tenta ler arquivo inteiro da flash
        try:
            with open(self.csv_path, "r") as f:
                csv_data = f.read()
        except OSError:
            # se não conseguir ler, devolve vazio
            csv_data = "timestamp_s,temp_ar_C,umid_ar_pct,temp_solo_C,umid_solo_pct\n"

        # Cabeçalhos extras:
        # Content-Disposition: attachment     -> força download
        # filename=log_temp.csv               -> nome sugerido
        self._http_response(
            client,
            "200 OK",
            "text/csv",
            csv_data,
            extra_headers={
                "Content-Disposition": "attachment; filename=log_temp.csv"
            }
        )

    # --------------------------------------------------
    # Página HTML principal
    # --------------------------------------------------
    def _page_html(self):
        return """<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Histórico de temperatura ar</title>
<style>
body {
    font-family: sans-serif;
    background:#111;
    color:#eee;
    padding:1rem;
}
#status {
    font-size:0.8rem;
    color:#888;
    margin-top:0.5rem;
}
canvas {
    background:#222;
    border:1px solid #444;
    width:100%%;
    max-width:400px;
    height:200px;
}
#actions {
    margin-top:1rem;
}
button {
    background:#222;
    color:#eee;
    border:1px solid #444;
    padding:0.5rem 1rem;
    font-size:1rem;
    border-radius:4px;
}
button:active {
    background:#333;
}
a.dllink {
    display:inline-block;
    background:#222;
    color:#eee;
    border:1px solid #444;
    padding:0.5rem 1rem;
    font-size:1rem;
    border-radius:4px;
    text-decoration:none;
}
a.dllink:active {
    background:#333;
}
</style>
</head>
<body>

<h2>Histórico de temperatura ar</h2>

<div id="status">Carregando histórico...</div>

<canvas id="chart" width="400" height="200"></canvas>

<div id="actions">
    <a class="dllink" href="/download">Baixar CSV</a>
</div>

<script>
function drawChart(points){
    const c = document.getElementById('chart');
    const ctx = c.getContext('2d');
    ctx.clearRect(0,0,c.width,c.height);

    if(points.length < 2){
        ctx.fillStyle="#eee";
        ctx.fillText("Poucos dados",10,20);
        document.getElementById('status').innerText =
            "Total de pontos: " + points.length;
        return;
    }

    let xArr = points.map(p=>p[0]);
    let yArr = points.map(p=>p[1]);

    let xmin = Math.min.apply(null, xArr);
    let xmax = Math.max.apply(null, xArr);
    let ymin = Math.min.apply(null, yArr);
    let ymax = Math.max.apply(null, yArr);

    if (ymax === ymin){
        ymax += 0.5;
        ymin -= 0.5;
    }

    function xs(x){
        return ((x - xmin)/(xmax - xmin))*(c.width-40)+20;
    }
    function ys(v){
        return (1-(v - ymin)/(ymax - ymin))*(c.height-40)+20;
    }

    // moldura
    ctx.strokeStyle = "#333";
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.rect(20,20,c.width-40,c.height-40);
    ctx.stroke();

    // curva
    ctx.strokeStyle = "#0f0";
    ctx.lineWidth = 2;
    ctx.beginPath();
    for (let i=0; i<points.length; i++){
        const xp = xs(points[i][0]);
        const yp = ys(points[i][1]);
        if (i===0) ctx.moveTo(xp,yp);
        else ctx.lineTo(xp,yp);
    }
    ctx.stroke();

    ctx.fillStyle="#eee";
    ctx.fillText(ymax.toFixed(2)+" C",25,25);
    ctx.fillText(ymin.toFixed(2)+" C",25,c.height-25);

    document.getElementById('status').innerText =
        "Total de pontos: " + points.length;
}

function loadHistory(){
    fetch("/history").then(r=>r.json()).then(obj=>{
        if(!obj.points || obj.points.length===0){
            document.getElementById('status').innerText = "Sem dados.";
            drawChart([]);
            return;
        }
        drawChart(obj.points);
    }).catch(e=>{
        document.getElementById('status').innerText = "Falha ao ler histórico.";
        drawChart([]);
    });
}

loadHistory();
</script>
</body>
</html>
"""

    # --------------------------------------------------
    # Loop de atendimento HTTP
    # --------------------------------------------------
    def poll_once(self):
        try:
            client, addr = self.sock.accept()
        except OSError:
            return  # sem conexões novas

        # tenta ler a requisição
        try:
            req = client.recv(512)
        except OSError:
            try:
                client.close()
            except:
                pass
            return

        try:
            line1 = req.decode().split("\r\n")[0]
        except:
            line1 = ""
        parts = line1.split(" ")
        path = "/"
        if len(parts) >= 2:
            path = parts[1]

        # roteamento
        try:
            if path == "/":
                self._http_response(client, "200 OK", "text/html", self._page_html())
            elif path == "/history":
                body = self._read_history_as_json()
                self._http_response(client, "200 OK", "application/json", body)
            elif path == "/download":
                self._send_csv_download(client)
            else:
                self._http_response(client, "404 Not Found", "text/plain", "not found")
        except OSError:
            try:
                client.close()
            except:
                pass
            return
