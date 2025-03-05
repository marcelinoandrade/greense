from flask import Flask, render_template

app = Flask(__name__)

# URLs dos Dashboards no Grafana
GRAFANA_ESTUFA_1 = "http://127.0.0.1:3000/d/deefsj9h9fqbkf/estufa-01?orgId=1&from=now-5m&to=now&timezone=browser&refresh=auto&kiosk"
GRAFANA_ESTUFA_2 = "http://127.0.0.1:3000/d/deefsj9h9fqbkf/estufa-02?orgId=1&from=now-5m&to=now&timezone=browser&refresh=auto&kiosk"

@app.route('/')
def home():
    return render_template('index_greense.html', grafana_estufa_1=GRAFANA_ESTUFA_1, grafana_estufa_2=GRAFANA_ESTUFA_2)

@app.route('/estufa1')
def estufa1():
    return render_template('grafana_dashboard.html', grafana_url=GRAFANA_ESTUFA_1)

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=80)
