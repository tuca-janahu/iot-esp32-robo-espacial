from flask import Flask, request, jsonify
import sqlite3
import os

app = Flask(__name__)
DB_FILE = "leituras.db"

# === Cria o banco e a tabela, se não existirem ===
def criar_banco():
    if not os.path.exists(DB_FILE):
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        cursor.execute('''
            CREATE TABLE leituras (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                nivel REAL,
                timestamp TEXT
            )
        ''')
        conn.commit()
        conn.close()

criar_banco()

# === Rota para receber dados do ESP32 ===
@app.route('/dados', methods=['POST'])
def receber_dados():
    try:
        data = request.get_json()

        if not data:
            return jsonify({"status": "erro", "mensagem": "JSON inválido"}), 400

        nivel = data.get('nivel')
        timestamp = data.get('timestamp')

        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        cursor.execute('INSERT INTO leituras (nivel, timestamp) VALUES (?, ?)', (nivel, timestamp))
        conn.commit()
        conn.close()

        return jsonify({"status": "OK", "mensagem": "Dado salvo com sucesso"}), 200

    except Exception as e:
        return jsonify({"status": "erro", "mensagem": str(e)}), 500


# === Rota para visualizar dados gravados ===
@app.route('/dados', methods=['GET'])
def listar_dados():
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    cursor.execute('SELECT * FROM leituras ORDER BY id DESC')
    linhas = cursor.fetchall()
    conn.close()

    dados = [{"id": l[0], "nivel": l[1], "timestamp": l[2]} for l in linhas]
    return jsonify(dados)


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
