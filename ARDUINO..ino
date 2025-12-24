#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Adafruit_MAX31865.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

// 🟢 Flask API endpoints
const char* dataUrl = "https://polyhouse-qqiy.onrender.com/sensors/data";
const char* controlUrl = "https://polyhouse-qqiy.onrender.com/sensors/control/waterTemp";

// 🟢 MAX31865 setup
Adafruit_MAX31865 thermo = Adafruit_MAX31865(D8, D7, D6, D5);
#define RREF 430.0
#define RNOMINAL 100.0

// 🟢 Pins
#define RELAY_PIN D0
#define LED_PIN LED_BUILTIN  // built-in LED for AP indication

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED off (active low on ESP8266)
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Relay OFF

  Serial.begin(115200);
  delay(2000);
  Serial.println("\n🚀 Booting Polyhouse Device...");

  // 🕐 Allow power stabilization
  delay(3000);

  thermo.begin(MAX31865_3WIRE);

  connectToWiFi();

  Serial.println("✅ System Ready!");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    float temperature = thermo.temperature(RNOMINAL, RREF);
    Serial.printf("🌡 Temperature = %.2f °C\n", temperature);
    sendTemperature(temperature);
    checkRelayState();
  } else {
    Serial.println("⚠️ WiFi lost. Retrying...");
    connectToWiFi();
  }

  delay(5000);
}

void connectToWiFi() {
  Serial.println("📡 Checking WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin();

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected to saved WiFi!");
    digitalWrite(LED_PIN, HIGH);
    Serial.print("🌐 IP: ");
    Serial.println(WiFi.localIP());
    return;
  }

  Serial.println("\n⚠️ Saved WiFi not found. Starting Access Point...");

  // 🟠 Indicate AP mode
  blinkLED(LED_PIN, 3, 300);

  WiFiManager wifiManager;
  wifiManager.setTimeout(180);

  if (!wifiManager.startConfigPortal("Polyhouse_CUTM")) {
    Serial.println("⏳ Config portal timeout. Restarting...");
    delay(2000);
    ESP.restart();
  }

  Serial.println("✅ Connected via new WiFi!");
  digitalWrite(LED_PIN, HIGH);
}

// 🔵 Blink LED helper
void blinkLED(int pin, int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, LOW);
    delay(delayMs);
    digitalWrite(pin, HIGH);
    delay(delayMs);
  }
}

// 🟢 Send temperature data securely
void sendTemperature(float temperature) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  http.begin(client, dataUrl);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(128);
  doc["temperature"] = temperature;
  String jsonData;
  serializeJson(doc, jsonData);

  int httpCode = http.POST(jsonData);
  if (httpCode > 0)
    Serial.printf("✅ POST /sensors/data -> %d\n", httpCode);
  else
    Serial.printf("❌ POST error: %s\n", http.errorToString(httpCode).c_str());

  http.end();
}

// 🟢 Fetch relay state securely
void checkRelayState() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  http.begin(client, controlUrl);
  int code = http.GET();

  if (code == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
      String state = doc["state"];
      if (state == "ON") digitalWrite(RELAY_PIN, LOW);
      else digitalWrite(RELAY_PIN, HIGH);
      Serial.printf("💡 Relay state: %s\n", state.c_str());
    }
  }

  http.end();
}
