# Projeto IoT - Etapa de Serviço 1

Projeto de IoT que visa realizar uma integração entre dois ESP32, com controle realizado através de um Joystick Analógico e resposta via CallMeBot, no WhatsApp.

De dois em dois segundos, o mqtt registra (através de um backend em Python) em um banco de dados SQLite os dados captados pelos sensores do ESP:
- Temperatura,
- Umidade,
- Presença, 
- Luminosidade.

Estrutura:
- `esp32/robo/src/robo.ino`: firmware do robô (sensores, telemetria, resposta a comandos).
- `esp32/joystick/src/joystick.ino`: firmware do joystick de controle.
- `config.h`: tópicos MQTT, pinos e IDs.
- `secrets.h` (não versionado): credenciais de Wi-Fi e chaves opcionais.

### Como compilar
1. Abra a Arduino IDE.
2. Instale placas **ESP32** (Board Manager).
3. Libraries: **PubSubClient**, **ArduinoJson**, **DHT sensor library**, **ESP32Servo**.
4. Copie `esp32/robo/src/secrets.h.example` para `secrets.h` e preencha suas credenciais.
5. Selecione a placa e porta corretas.
6. Compile e carregue.

Tópicos MQTT:
- Comandos: `robo/comando`
- Telemetria: `robo/lab/leituras/