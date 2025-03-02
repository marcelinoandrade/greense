from flask import Flask, render_template

app = Flask(__name__)

@app.route('/')
def home():
    grafana_url = "http://localhost:3000/d-solo/deefsj9h9fqbkf/estufa-01?orgId=1&from=1740952234728&to=1740952534728&timezone=browser&refresh=auto&panelId=3&__feature.dashboardSceneSolo"
    return render_template('grafana_dashboard.html', grafana_url=grafana_url)

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5000)
