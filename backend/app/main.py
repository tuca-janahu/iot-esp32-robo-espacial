from __future__ import annotations
import os, uuid
from datetime import datetime, timezone
import paho.mqtt.client as mqtt
import json, threading
from flask import Flask, request, jsonify
from dotenv import load_dotenv
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError

from .db import engine, SessionLocal, Base      # <— com ponto
from .model import Leitura                      # <— com ponto



# ---------------- Config ----------------
load_dotenv()
API_KEY = os.getenv("API_KEY", "dev-key")
PORT    = int(os.getenv("PORT", "8000"))
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT   = int(os.getenv("MQTT_PORT", "1883"))
MQTT_USER   = os.getenv("MQTT_USERNAME", "") or None
MQTT_PASS   = os.getenv("MQTT_PASSWORD", "") or None
MQTT_TOPIC  = os.getenv("MQTT_TOPIC", "robo/lab/leituras/#")
MQTT_QOS    = int(os.getenv("MQTT_QOS", "1"))

app = Flask(__name__)

# garante que a(s) tabela(s) existam
Base.metadata.create_all(bind=engine)

# ---------------- Helpers ----------------
def require_api_key():
    key = (request.headers.get("X-API-Key")
           or request.args.get("api_key")
           or request.cookies.get("api_key"))
    if not key or key != API_KEY:
        return jsonify({"error": "missing or invalid API key"}), 401
    
    
def require_header(name: str) -> str:
    val = request.headers.get(name)
    if not val:
        raise ValueError(f"missing header: {name}")
    return val

def parse_iso8601(value) -> datetime:
    """Aceita string ISO 8601 (com 'Z' ou offset) ou datetime; retorna datetime aware (UTC)."""
    if isinstance(value, datetime):
        return value
    s = str(value)
    if s.endswith("Z"):
        s = s[:-1] + "+00:00"
    return datetime.fromisoformat(s)

def _i(v):  # int opcional
    return None if v is None else int(v)

def _f(v):  # float opcional
    return None if v is None else float(v)

def handle_payload_from_mqtt(device_id: str, data: dict):
    # exige idempotency_key e timestamp
    idem = str(data.get("idempotency_key") or "")
    if not idem:
        return
    try:
        uuid.UUID(idem)
    except Exception:
        return
    ts = data.get("timestamp")
    if not ts:
        return

    leitura = Leitura(
        device_id=device_id,
        idempotency_key=idem,
        timestamp=parse_iso8601(ts),
        temperatura_c=_f(data.get("temperatura_c")),
        umidade_pct=_i(data.get("umidade_pct")),
        luminosidade=_i(data.get("luminosidade")),
        presenca=_i(data.get("presenca")),
        probabilidade_vida=_f(data.get("probabilidade_vida")),
    )

    with SessionLocal() as s:
        s.add(leitura)
        try:
            s.commit()
        except IntegrityError:
            s.rollback()  # idempotente: já inserido, ignore
            pass

def make_mqtt_client():
    client = mqtt.Client(protocol=mqtt.MQTTv311)
    if MQTT_USER and MQTT_PASS:
        client.username_pw_set(MQTT_USER, MQTT_PASS)

    def on_connect(cli, userdata, flags, rc):
        print(f"[MQTT] Connected rc={rc}, subscribing {MQTT_TOPIC}")
        cli.subscribe(MQTT_TOPIC, qos=MQTT_QOS)

    def on_message(cli, userdata, msg):
        try:
            parts = msg.topic.split("/")
            device_id = parts[-1] if parts else "unknown"
            data = json.loads(msg.payload.decode("utf-8"))
            handle_payload_from_mqtt(device_id, data)
        except Exception as e:
            print(f"[MQTT] on_message error: {e}")

    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
    return client

def start_mqtt_background():
    client = make_mqtt_client()
    t = threading.Thread(target=client.loop_forever, daemon=True)
    t.start()
    print("[MQTT] loop started in background thread")

Base.metadata.create_all(bind=engine)
start_mqtt_background()

# ---------------- Rotas ----------------
@app.get("/")
def root():
    return {
        "name": "iot-esp32-robo-espacial",
        "endpoints": ["/health", "/leituras (GET, POST)"]
    }

@app.get("/health")
def health():
    try:
        # simples ping ao banco
        with SessionLocal() as s:
            s.execute(select(1))
        return {"app": "ok", "db": "ok"}, 200
    except Exception as e:
        return {"app": "ok", "db": "error", "detail": str(e)}, 500

@app.post("/leituras")
def post_leituras():
    # auth
    # auth = require_api_key()
    # if auth: 
    #     return auth

    try:
        device_id = require_header("X-Device-Id")
        idemp     = require_header("Idempotency-Key")

        # valida UUID (idempotency-key precisa ser um UUID válido)
        try:
            uuid.UUID(idemp)
        except Exception:
            return {"error": "Idempotency-Key must be a valid UUID"}, 400

        # corpo JSON
        data = request.get_json(force=True) or {}

        if "timestamp" not in data:
            return {"error": "timestamp is required"}, 400

        leitura = Leitura(
            device_id=device_id,
            idempotency_key=idemp,
            timestamp=parse_iso8601(data["timestamp"]),
            temperatura_c=_f(data.get("temperatura_c")),
            umidade_pct=_i(data.get("umidade_pct")),
            luminosidade=_i(data.get("luminosidade")),
            presenca=_i(data.get("presenca")),
            probabilidade_vida=_f(data.get("probabilidade_vida")),
            # received_at tem default no modelo; se quiser forçar agora:
            # received_at=datetime.now(timezone.utc),
        )

        with SessionLocal() as s:
            s.add(leitura)
            try:
                s.commit()
                s.refresh(leitura)
                return {
                    "ack": True,
                    "id": leitura.id,
                    "stored_at": leitura.received_at.isoformat()
                }, 201
            except IntegrityError:
                s.rollback()
                # idempotência: se já existe a mesma idempotency_key, retorne o existente
                existing = s.execute(
                    select(Leitura).where(Leitura.idempotency_key == idemp)
                ).scalar_one_or_none()
                if existing:
                    return {
                        "ack": True,
                        "id": existing.id,
                        "stored_at": existing.received_at.isoformat(),
                        "idempotent": True
                    }, 200
                return {"error": "idempotency conflict but record not found"}, 409

    except ValueError as ve:
        return {"error": str(ve)}, 400
    except Exception as e:
        return {"error": "internal error", "detail": str(e)}, 500

@app.get("/leituras")
def get_leituras():
    # se quiser público, remova estas duas linhas
    # auth = require_api_key()
    # if auth:
    #     return auth

    try:
        limit = int(request.args.get("limit", 100))
        limit = max(1, min(limit, 1000))
    except Exception:
        limit = 100

    with SessionLocal() as s:
        rows = s.execute(
            select(Leitura).order_by(Leitura.timestamp.desc()).limit(limit)
        ).scalars().all()

    return [{
        "id": r.id,
        "device_id": r.device_id,
        "timestamp": r.timestamp.isoformat(),
        "temperatura_c": r.temperatura_c,
        "umidade_pct": r.umidade_pct,
        "luminosidade": r.luminosidade,
        "presenca": r.presenca,
        "probabilidade_vida": r.probabilidade_vida,
        "received_at": r.received_at.isoformat(),
    } for r in rows], 200

# ---------------- Run ----------------
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=PORT)
