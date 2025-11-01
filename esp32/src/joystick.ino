#include <WiFi.h>
#include <PubSubClient.h>

// ---- CONFIGURAÇÕES Wi-Fi ----
const char* SSID = "iPhone de Maria Luiza";
const char* PASSWORD = "malu1234";

// ---- CONFIGURAÇÕES MQTT ----
const char* BROKER = "test.mosquitto.org";
const int BROKER_PORT = 1883;
const char* TOPICO_COMANDO = "robo/comando";

// ---- PINOS DO JOYSTICK (para ESP32-S3) ----
#define JOY_X 34  
#define JOY_Y 35   
#define BOTAO_JOY 32

// ---- OBJETOS Wi-Fi e MQTT ----
WiFiClient espClient;
PubSubClient client(espClient);

// ---- VARIÁVEIS DE CONTROLE ----
String ultimoComando = "";
bool roboLigado = true;
bool estadoAnteriorBotao = HIGH;

// ---- CONFIGURAÇÕES DE ZONA MORTA ----
int centroX = 2048;
int centroY = 2048;
const int zonaMorta = 400;

void setup() {
  Serial.begin(115200);
  pinMode(BOTAO_JOY, INPUT_PULLUP);

  Serial.println("Conectando ao Wi-Fi...");
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi conectado!");

  client.setServer(BROKER, BROKER_PORT);
  Serial.println("Iniciando leitura do joystick...");
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // ---- Lê botão do joystick ----
  int leituraBotao = digitalRead(BOTAO_JOY);
  if (leituraBotao == LOW && estadoAnteriorBotao == HIGH) {
    roboLigado = !roboLigado;
    if (roboLigado) {
      client.publish(TOPICO_COMANDO, "LIGAR");
      Serial.println("🟢 Comando enviado: LIGAR");
    } else {
      client.publish(TOPICO_COMANDO, "DESLIGAR");
      Serial.println("🔴 Comando enviado: DESLIGAR");
    }
    delay(500);
  }
  estadoAnteriorBotao = leituraBotao;

  if (!roboLigado) return;

  // ---- Lê e imprime valores analógicos do joystick ----
  int eixoX = analogRead(JOY_X);
  int eixoY = analogRead(JOY_Y);

  Serial.print("Eixo X: ");
  Serial.print(eixoX);
  Serial.print(" | Eixo Y: ");
  Serial.println(eixoY);

  // Ajuste automático de centro (na primeira leitura)
  static bool centrado = false;
  if (!centrado && eixoX > 1000 && eixoY > 1000) {
    centroX = eixoX;
    centroY = eixoY;
    centrado = true;
    Serial.print("Centro calibrado! X=");
    Serial.print(centroX);
    Serial.print(" Y=");
    Serial.println(centroY);
  }

  // ---- Determina comando ----
  String comando = definirComando(eixoX, eixoY);

  if (comando != ultimoComando) {
    ultimoComando = comando;
    client.publish(TOPICO_COMANDO, comando.c_str());
    Serial.print("📡 Comando enviado: ");
    Serial.println(comando);
  }

  delay(200);
}

String definirComando(int x, int y) {
  // Verifica desvios
  if (y > centroY + zonaMorta) return "FRENTE";
  else if (y < centroY - zonaMorta) return "TRAS";
  else if (x > centroX + zonaMorta) return "DIREITA";
  else if (x < centroX - zonaMorta) return "ESQUERDA";
  else return "PARAR";
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando ao broker MQTT...");
    if (client.connect("ESP32-Joystick")) {
      Serial.println("conectado!");
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5s...");
      delay(5000);
    }
  }
}
