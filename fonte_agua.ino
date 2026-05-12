#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// --- Configurações de WiFi e MQTT ---
const char* ssid = "SALA";
const char* password = "g0mesp1res";
const char* mqtt_server = "192.168.1.201";
const char* topicPIR = "home/pir1";
const char* topicTemp = "home/temp/2";
const char* topicHum = "home/hum/2";

// --- Pinos ---
const int pinPIR = 5;    // D1
const int pinRelay = 4;  // D2
const int pinDHT = 2;    // D4 no NodeMCU é o GPIO2

// --- Variáveis de Controlo ---
unsigned long lastMovementTime = 0;
unsigned long lastDHTRead = 0;
unsigned long lastRestartCheck = 0;

const long intervalPIR = 120000; // 2 minutos para a fonte
const long intervalDHT = 5000;   // 5 segundos para sensores
const long restartInterval = 10800000; // Restart a cada 3 horas (3 * 3600 * 1000)

bool fonteLigada = false;

DHT dht(pinDHT, DHT11);
WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP8266_Fonte_Pet")) {
      client.publish("home/status", "online");
    } else { delay(5000); }
  }
}

void setup() {
  pinMode(pinPIR, INPUT);
  pinMode(pinRelay, OUTPUT);
  digitalWrite(pinRelay, LOW); 
  
  dht.begin();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long currentMillis = millis();

  // --- Lógica do PIR ---
  int movimento = digitalRead(pinPIR);
  if (movimento == HIGH) {
    if (!fonteLigada) {
      digitalWrite(pinRelay, HIGH);
      client.publish(topicPIR, "1");
      fonteLigada = true;
    }
    lastMovementTime = currentMillis;
  }

  if (fonteLigada && (currentMillis - lastMovementTime >= intervalPIR)) {
    digitalWrite(pinRelay, LOW);
    client.publish(topicPIR, "0");
    fonteLigada = false;
  }

  // --- Lógica do DHT11 (a cada 5s) ---
  if (currentMillis - lastDHTRead >= intervalDHT) {
    lastDHTRead = currentMillis;
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      client.publish(topicTemp, String(t).c_str());
      client.publish(topicHum, String(h).c_str());
    }
  }

  // --- Restart Automático (a cada 3 horas) ---
  if (currentMillis >= restartInterval) {
    client.publish("home/status", "restarting");
    delay(1000);
    ESP.restart();
  }
}