#include <WiFi.h>
#include <PubSubClient.h>

// ===== KONFIGURASI JARINGAN =====
const char* WIFI_SSID = "WIFI_SSID";
const char* WIFI_PASS = "WIFI_PASS";
const char* BROKER_IP = "broker.emqx.io"; // Pastikan sama dengan Server Broker di Node-RED

// ===== IDENTIFIKASI RUANGAN =====
const char* ROOM_NAME = "ruang3";

// ===== PEMETAAN PIN HARDWARE (ESP32 DevKit V1) =====
const int POT_PIN = 32; // ADC1 GPIO32 untuk Potensiometer (Suhu 0-60°C & Kelembapan 100-0% berkebalikan)
const int L1_PIN  = 16; // LED Lampu 1
const int L2_PIN  = 18; // LED Lampu 2

WiFiClient net;
PubSubClient mqtt(net);
String clientId;

// Topic Buffers (Mengikuti pola pens/klp9/<room>/...)
char tTemp[50], tHum[50], tL1[50], tL2[50], tStatus[50];

unsigned long lastPublish   = 0;
unsigned long lastWiFiRetry = 0;

// ===== CALLBACK HANDLING (SUBSCRIBE) =====
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim(); // Hapus spasi atau karakter tersembunyi

  Serial.print("Pesan masuk [Topic: ");
  Serial.print(topic);
  Serial.print("] Value: ");
  Serial.println(message);

  // Tentukan state (HIGH/LOW)
  int state = (message == "1") ? HIGH : LOW;

  // Cek dan kontrol Lampu 1
  if (strcmp(topic, tL1) == 0) {
    if (message == "1" || message == "0") {
      digitalWrite(L1_PIN, state);
      Serial.println(state == HIGH ? "Lampu 1 MENYALA" : "Lampu 1 MATI");
    }
  } 
  // Cek dan kontrol Lampu 2
  else if (strcmp(topic, tL2) == 0) {
    if (message == "1" || message == "0") {
      digitalWrite(L2_PIN, state);
      Serial.println(state == HIGH ? "Lampu 2 MENYALA" : "Lampu 2 MATI");
    }
  }
}

void setup() {
  Serial.begin(9600);
  delay(1000); 

  pinMode(L1_PIN, OUTPUT);
  pinMode(L2_PIN, OUTPUT);
  pinMode(POT_PIN, INPUT);
  digitalWrite(L1_PIN, LOW);
  digitalWrite(L2_PIN, LOW);

  // Resolusi ADC 12-bit (0 - 4095)
  analogReadResolution(12);

  snprintf(tTemp, sizeof(tTemp), "pens/klp9/%s/temperature", ROOM_NAME);
  snprintf(tHum, sizeof(tHum), "pens/klp9/%s/humidity", ROOM_NAME);
  snprintf(tL1, sizeof(tL1), "pens/klp9/%s/lamp1", ROOM_NAME);
  snprintf(tL2, sizeof(tL2), "pens/klp9/%s/lamp2", ROOM_NAME);
  snprintf(tStatus, sizeof(tStatus), "pens/klp9/%s/status", ROOM_NAME);

  Serial.println("\nData Ruangan 3");
  Serial.println(tTemp);
  Serial.println(tHum);
  Serial.println(tL1);
  Serial.println(tL2);
  Serial.println(tStatus);
  Serial.println("__________________________");

  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  clientId = "esp32-devkit-pens-" + WiFi.macAddress();

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
      Serial.println("connected!");
      mqtt.subscribe(tL1, 1);
      mqtt.subscribe(tL2, 1);
      mqtt.publish(tStatus, "online", true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
    }
  }
}

void loop() {
  // Penanganan Koneksi WiFi yang Aman dari Error "sta is connecting"
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWiFiRetry >= 10000) { 
      lastWiFiRetry = millis();
      Serial.println("WiFi disconnected. Reconnecting...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
    return;
  }

  // Penanganan Koneksi MQTT
  if (!mqtt.connected()) {
    connectMQTT();
    return;
  }

  mqtt.loop();

  // Kirim data sensor setiap 5 detik
  if (millis() - lastPublish >= 5000) {
    lastPublish = millis();

    // Baca nilai ADC dari Potensiometer di GPIO 32 (rentang 0 - 4095)
    int rawADC = analogRead(POT_PIN);

    // 1. Suhu naik lurus: saat ADC 0 -> 0.0 °C, saat ADC 4095 -> 60.0 °C
    float suhu = (rawADC / 4095.0) * 60.0;

    char tempBuf[8];
    snprintf(tempBuf, sizeof(tempBuf), "%.1f", suhu);
    mqtt.publish(tTemp, tempBuf);

    // 2. Kelembapan berkebalikan: saat suhu naik ke 60°C, kelembapan turun ke 0%
    //    saat ADC 0 -> 100.0 %, saat ADC 4095 -> 0.0 %
    float kelembapan =(rawADC / 4095.0) * 100.0;

    char humBuf[8];
    snprintf(humBuf, sizeof(humBuf), "%.1f", kelembapan);
    mqtt.publish(tHum, humBuf);
    
    // Cetak ke Serial Monitor
    Serial.print("[Publish] ADC: ");
    Serial.print(rawADC);
    Serial.print(" | Suhu: ");
    Serial.print(tempBuf);
    Serial.print(" °C | Kelembapan: ");
    Serial.print(humBuf);
    Serial.println(" %");
  }
}