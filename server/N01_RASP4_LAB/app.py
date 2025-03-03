from flask import Flask, render_template

app = Flask(__name__)

@app.route('/')
def home():
    grafana_url = "http://localhost:3000/d-solo/deefsj9h9fqbkf/estufa-01?orgId=1&from=now-5m&to=now&timezone=browser&refresh=5s&panelId=3"
    grafana_url = "http://localhost:3000/public-dashboards/4d9136643ade492fbe122c38ab6ae5be"
    return render_template('grafana_dashboard.html', grafana_url=grafana_url)

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5000)
