from flask import Flask, request, jsonify
import json
import os

app = Flask(__name__)

ARQUIVO = "leituras.json"

# Garante que o arquivo exista
if not os.path.exists(ARQUIVO):
    with open(ARQUIVO, "w") as f:
        json.dump([], f)

@app.route('/dados', methods=['POST'])
def receber_dados():
    try:
        data = request.get_json()

        # Verifica se os dados vieram no formato certo
        if not data:
            return jsonify({"status": "erro", "mensagem": "JSON inválido ou ausente"}), 400

        nivel = data.get('nivel')
        timestamp = data.get('timestamp')

        # Lê o conteúdo atual
        with open(ARQUIVO, "r") as f:
            leituras = json.load(f)

        # Adiciona a nova leitura
        leituras.append({"nivel": nivel, "timestamp": timestamp})

        # Grava novamente no arquivo
        with open(ARQUIVO, "w") as f:
            json.dump(leituras, f, indent=4)

        return jsonify({"status": "OK", "mensagem": "Dado salvo com sucesso"}), 200

    except Exception as e:
        return jsonify({"status": "erro", "mensagem": str(e)}), 500


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
