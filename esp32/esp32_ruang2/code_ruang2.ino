#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
// Library: "DHT sensor library" by Adafruit (+ "Adafruit Unified Sensor")

// ====== KONFIGURASI JARINGAN ======
const char* WIFI_SSID = "WIFI_SSID";
const char* WIFI_PASS = "WIFI_PASS";
const char* BROKER_IP = "broker.emqx.io";

// ====== IDENTIFIKASI RUANGAN ======
const char* ROOM_NAME = "ruang2";

// ====== KONFIGURASI PIN HARDWARE (Sesuai Skema) ======
const int DHT_PIN = 32; // Pin data DHT11
const int L1_PIN  = 33; // LED Lampu 1
const int L2_PIN  = 25; // LED Lampu 2

#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

WiFiClient net;
PubSubClient mqtt(net);
String clientId;

// Topic buffers (pms/klsA/<room>/...)
char tTemp[50], tHum[50], tL1[50], tL2[50], tStatus[50];

unsigned long lastPublish = 0;
unsigned long lastWifiRetry = 0;

// ====== CALLBACK MENERIMA (SUBSCRIBE) ======
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();

  Serial.print("Pesan masuk [Topic: ");
  Serial.print(topic);
  Serial.print("] Value: ");
  Serial.println(message);

  int state = (message == "1") ? HIGH : LOW;

  if (strcmp(topic, tL1) == 0) {
    if (message == "1" || message == "0") {
      digitalWrite(L1_PIN, state);
      Serial.println(state == HIGH ? "Lampu 1 MENYALA" : "Lampu 1 MATI");
    }
  }
  else if (strcmp(topic, tL2) == 0) {
    if (message == "1" || message == "0") {
      digitalWrite(L2_PIN, state);
      Serial.println(state == HIGH ? "Lampu 2 MENYALA" : "Lampu 2 MATI");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(L1_PIN, OUTPUT);
  pinMode(L2_PIN, OUTPUT);
  digitalWrite(L1_PIN, LOW);
  digitalWrite(L2_PIN, LOW);

  dht.begin();

  snprintf(tTemp,   sizeof(tTemp),   "pms/klsA/%s/temperature", ROOM_NAME);
  snprintf(tHum,    sizeof(tHum),    "pms/klsA/%s/humidity",    ROOM_NAME);
  snprintf(tL1,     sizeof(tL1),     "pms/klsA/%s/lampu1",      ROOM_NAME);
  snprintf(tL2,     sizeof(tL2),     "pms/klsA/%s/lampu2",      ROOM_NAME);
  snprintf(tStatus, sizeof(tStatus), "pms/klsA/%s/status",      ROOM_NAME);

  Serial.println("\nData Ruangan 2");
  Serial.println(tTemp);
  Serial.println(tHum);
  Serial.println(tL1);
  Serial.println(tL2);
  Serial.println(tStatus);
  Serial.println("=====================\n");

  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  clientId = "esp32-devkit-v1-" + WiFi.macAddress();

  mqtt.setServer(BROKER_IP, 1883);
  mqtt.setCallback(callback);
  mqtt.setKeepAlive(15);
}

void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;

  static unsigned long lastConnectAttempt = 0;
  if (millis() - lastConnectAttempt >= 5000) {
    lastConnectAttempt = millis();

    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(clientId.c_str(), tStatus, 0, true, "offline")) {
      Serial.println