#include "DHT.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>   


#define DHTPIN 4     
#define DHTTYPE DHT11 
#define LDR_PIN 34 
#define PIR_PIN 27
#define LED_VERDE 12
#define LED_VERMELHO 13
#define SERVO_ESQ 14
#define SERVO_DIR 15

// Configurações do Wi-Fi
const char* SSID = "iPhone de Maria Luiza";
const char* PASSWORD = "malu1234";

// ---- CONFIGURAÇÕES MQTT ----
const char* BROKER = "test.mosquitto.org";  // ou o IP do seu broker local
const int BROKER_PORT = 1883;
const char* TOPICO_COMANDO = "robo/comando";
const char* TOPICO_TELEMETRIA_BASE = "robo/lab/leituras/";  // backend assina robo/lab/leituras/#


// Configurações do Callmebot
String phoneNumber = "+557191772210"; 
String apiKey = "8002058";

DHT dht(DHTPIN, DHTTYPE); 
int valorLuz = 0;
Servo motorEsquerdo;
Servo motorDireito;
unsigned long ultimoEnvio = 0;
bool roboLigado = true; 


// ---- OBJETOS WiFi e MQTT ----
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200); 
  dht.begin();        
  Serial.println("Iniciando sensores...");
  pinMode(PIR_PIN, INPUT); 
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  motorEsquerdo.attach(SERVO_ESQ);
  motorDireito.attach(SERVO_DIR);

  // Conexão Wi-Fi
  Serial.println("Conectando ao WiFi...");
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");

  client.setServer(BROKER, BROKER_PORT);
  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) reconnect();
  client.loop();

  if (!roboLigado) {
    delay(500);
    return;
  }
  
  if (millis() - ultimoEnvio >= 2000) {
    ultimoEnvio = millis();

    // Sensor de Temperatura
  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();
  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println("Falha ao ler o sensor DHT11!");
    delay(2000);
    return;
  }

  //Fotoresistor
  valorLuz = analogRead(LDR_PIN);

  //Sensor de presença
  int estadoPIR = digitalRead(PIR_PIN);

  //Cálculo de Probailidade
  float probabilidade = 0;
  // Temperatura entre 15°C e 30°C
  if (temperatura >= 15 && temperatura <= 30) {
    probabilidade += 25;
  }
  // Umidade entre 40% e 70%
  if (umidade >= 40 && umidade <= 70) {
    probabilidade += 25;
  }
  // Luz adequada (valor ADC acima de 2000)
  if (valorLuz > 1000) {
    probabilidade += 20;
  }
  // Presença detectada
  if (estadoPIR == HIGH) {
    probabilidade += 30;
  }


  //Monitor Serial
  Serial.println("=====================================");
  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.print(" °C | Umidade: ");
  Serial.print(umidade);
  Serial.println(" %");

  Serial.print("Intensidade de Luz (ADC): ");
  Serial.println(valorLuz);

  if (estadoPIR == HIGH) {
    Serial.println("Presença detectada!");
  } else {
    Serial.println("Sem presença.");
  }

  Serial.print("Probabilidade de vida: ");
  Serial.print(probabilidade);
  Serial.println(" %");


  // --- Decisão com base na probabilidade ---
  if (probabilidade <= 75) {
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(LED_VERMELHO, LOW);
    Serial.println("Exploração normal. Nenhum indício relevante detectado.");
  } else {
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_VERMELHO, HIGH);
    Serial.println("⚠️ ALERTA! Alta probabilidade de vida detectada!");

    // --- Envio de mensagem via Callmebot ---
    enviarMensagemCallmebot("Alerta! Alta probabilidade de vida detectada no planeta.");
  }

  Serial.println("=====================================");
    publicarTelemetria(temperatura, umidade, valorLuz, estadoPIR == HIGH ? 1 : 0, probabilidade);

  delay(2000); 
}
}

// --- Função para enviar mensagem no WhatsApp via Callmebot ---
void enviarMensagemCallmebot(String mensagem) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + 
                 "&text=" + urlencode(mensagem) + 
                 "&apikey=" + apiKey;

    Serial.println("Enviando mensagem via Callmebot...");
    Serial.println(url);  // Mostra a URL para debug

    http.begin(url);
    int httpResponseCode = http.GET();

    // Aceita qualquer resposta 2xx como sucesso (200-299)
    if (httpResponseCode >= 200 && httpResponseCode < 300) {
      String payload = http.getString();
      Serial.print("✅ Mensagem enviada com sucesso! Código HTTP: ");
      Serial.println(httpResponseCode);
      Serial.print("Resposta: ");
      Serial.println(payload);
    } else {
      Serial.print("❌ Erro ao enviar mensagem. Código HTTP: ");
      Serial.println(httpResponseCode);
      // Opcional: ler corpo mesmo em erro
      String payload = http.getString();
      if (payload.length() > 0) {
        Serial.print("Corpo da resposta: ");
        Serial.println(payload);
      }
    }

    http.end();
  } else {
    Serial.println("❌ WiFi desconectado. Mensagem não enviada.");
  }
}



// --- Função simples para codificar a URL ---
String urlencode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  char code2;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encodedString += c;
    } else {
      encodedString += '%';
      code1 = (c >> 4) & 0xF;
      code2 = c & 0xF;
      code1 += code1 > 9 ? 'A' - 10 : '0';
      code2 += code2 > 9 ? 'A' - 10 : '0';
      encodedString += code1;
      encodedString += code2;
    }
  }
  return encodedString;
}

void moverFrente() {
  motorEsquerdo.write(180);
  motorDireito.write(0);
  Serial.println("➡️ Comando: FRENTE");
}

void moverTras() {
  motorEsquerdo.write(0);
  motorDireito.write(180);
  Serial.println("⬅️ Comando: TRÁS");
}

void virarEsquerda() {
  motorEsquerdo.write(90);
  motorDireito.write(0);
  Serial.println("↩️ Comando: ESQUERDA");
}

void virarDireita() {
  motorEsquerdo.write(180);
  motorDireito.write(90);
  Serial.println("↪️ Comando: DIREITA");
}

void pararMotores() {
  motorEsquerdo.write(90);
  motorDireito.write(90);
  Serial.println("⛔ Motores parados");
}


void callback(char* topic, byte* message, unsigned int length) { 
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)message[i];
  }
  msg.trim();
  msg.toUpperCase();

  Serial.println("=====================================");
  Serial.print("📩 Mensagem recebida via MQTT: ");
  Serial.println(msg);

  if (msg == "LIGAR") {
    roboLigado = true;
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(LED_VERMELHO, LOW);
    Serial.println("✅ Robô ligado remotamente!");
    return;
  }

  if (msg == "DESLIGAR") {
    roboLigado = false;
    pararMotores();
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_VERMELHO, HIGH);
    Serial.println("⚠️ Robô desligado remotamente!");
    return;
  }

  // --- Bloqueia movimento se o robô estiver desligado ---
  if (!roboLigado) {
    Serial.println("⚠️ Robô desligado — comando ignorado.");
    return;
  }

  // --- Movimentos controlados ---
  if (msg == "FRENTE") moverFrente();
  else if (msg == "TRAS") moverTras();
  else if (msg == "ESQUERDA") virarEsquerda();
  else if (msg == "DIREITA") virarDireita();
  else if (msg == "PARAR") pararMotores();
  else Serial.println("❓ Comando desconhecido.");

  Serial.println("=====================================");
}

String genIdemp() {
  char buf[37];
  sprintf(buf, "%08X-%04X-%04X-%04X-%012X",
          (unsigned)esp_random(), (unsigned)esp_random() & 0xFFFF,
          (unsigned)esp_random() & 0xFFFF, (unsigned)esp_random() & 0xFFFF,
          (unsigned long long)esp_random());
  return String(buf);
}

String nowIsoUtc() { 
  // TODO: usar NTPClient para o horário real
  return "2025-09-02T14:35:00Z"; 
}

void publicarTelemetria(float temperatura, float umidade, int luz, int presenca, float prob) {
  StaticJsonDocument<256> doc;
  doc["idempotency_key"]    = genIdemp();
  doc["timestamp"]          = nowIsoUtc();
  doc["temperatura_c"]      = temperatura;
  doc["umidade_pct"]        = (int)umidade;
  doc["luminosidade"]       = luz;
  doc["presenca"]           = presenca;
  doc["probabilidade_vida"] = prob;

  char payload[256];
  size_t n = serializeJson(doc, payload, sizeof(payload));

  String topic = String(TOPICO_TELEMETRIA_BASE) + "esp32-01"; // seu device_id
  bool ok = client.publish(topic.c_str(), payload, n);
  Serial.print("MQTT telemetria -> ");
  Serial.print(topic);
  Serial.print(" | ");
  Serial.println(ok ? "OK" : "FALHA");
}

// ---- CONEXÃO AO BROKER ----
void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando ao broker MQTT...");
    if (client.connect("ESP32-Robo")) {
      Serial.println("conectado!");
      client.subscribe(TOPICO_COMANDO);
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5s...");
      delay(5000);
    }
  }
}