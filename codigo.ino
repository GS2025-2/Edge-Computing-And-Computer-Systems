#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ===== CONFIGURAÇÃO WIFI =====
const char* ssid = "Wokwi-GUEST";   // Wi-Fi da simulação Wokwi
const char* password = "";

// ===== CONFIGURAÇÃO MQTT =====
const char* mqtt_server = "broker.hivemq.com"; // broker público
const int mqtt_port = 1883;
const char* mqtt_topic = "esp32/sensores";

WiFiClient espClient;
PubSubClient client(espClient);

// ===== PINOS =====
const int ldrPin = 34;        // pino analógico do LDR
const int potPin = 35;        // pino analógico do potenciômetro
const int dhtPin = 2;         // pino digital do DHT22

DHT dht(dhtPin, DHT22);

// ===== VARIÁVEIS DE CONTROLE =====
unsigned long lastMsg = 0;
const long intervalo = 2000; // 2 segundos

// ===== FUNÇÕES =====
void setupWiFi() {
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) { // tenta por 10s
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Wi-Fi conectado!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Falha ao conectar ao Wi-Fi!");
  }
}

void reconnect() {
  int tentativas = 0;
  while (!client.connected() && tentativas < 10) { // tenta até 10 vezes
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("❌ Wi-Fi desconectado, tentando reconectar...");
      setupWiFi();
    }

    Serial.print("Conectando ao MQTT...");
    String clientId = "ESP32Client-" + String(WiFi.macAddress());
    if (client.connect(clientId.c_str())) {
      Serial.println("✅ Conectado ao broker MQTT!");
      break;
    } else {
      Serial.print("❌ Falha, rc=");
      Serial.print(client.state());
      Serial.println(" — tentando novamente em 2s");
      delay(2000);
    }
    tentativas++;
  }

  if (!client.connected()) {
    Serial.println("⚠️ Não conseguiu conectar ao MQTT após várias tentativas.");
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  setupWiFi();
  client.setServer(mqtt_server, mqtt_port);
  reconnect();
  Serial.println("🌡️ Sistema iniciado. Aguardando leituras...");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long agora = millis();
  if (agora - lastMsg > intervalo) {
    lastMsg = agora;

    int ldrValue = analogRead(ldrPin);
    int potValue = analogRead(potPin);
    float temperature = dht.readTemperature();

    String payload = "{";

    // === Tratamento dos valores ===
    if (isnan(temperature)) {
      payload += "\"temperatura\":null,";
    } else {
      payload += "\"temperatura\":" + String(temperature) + ",";
    }

    if (ldrValue == 0 && potValue == 0 && isnan(temperature)) {
      payload += "\"status\":\"Dados não captados pelos sensores\"";
    } else {
      String status;
      if (potValue < 1500 && ldrValue < 1500) {
        status = "Ambiente tranquilo — ótimo para foco criativo";
      } else if (potValue < 2500 && ldrValue < 2500) {
        status = "Ambiente equilibrado — conforto e concentração";
      } else if (potValue < 3500 && ldrValue < 3500) {
        status = "Ambiente moderadamente agitado — atenção moderada";
      } else {
        status = "Ambiente barulhento ou muito iluminado — foco prejudicado";
      }

      payload += "\"luminosidade\":" + String(ldrValue) + ",";
      payload += "\"som\":" + String(potValue) + ",";
      payload += "\"status\":\"" + status + "\"";
    }

    payload += "}";

    Serial.println("📤 Enviando: " + payload);
    client.publish(mqtt_topic, payload.c_str());
  }
}
