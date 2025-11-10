/*
  Nano33IoT_AWS_MQTT_Demo.ino
  --------------------------------------
  Secure MQTT example connecting Arduino Nano 33 IoT 
  to AWS IoT Core using mutual TLS (mTLS).

  Publishes uptime every 5 seconds to:
      tomevans/nano33iot/telemetry
  Subscribes to:
      tomevans/nano33iot/cmd
  and toggles the onboard LED based on messages:
      "LED ON" / "LED OFF"

  Developed as part of an IoT learning exercise using ChatGPT.
*/

#include <WiFiNINA.h>
#include <ArduinoECCX08.h>
#include <ArduinoBearSSL.h>
#include <ArduinoMqttClient.h>
#include "credentials.h"
#include "aws_root_ca.h"
#include "device_cert_der.h"

// -----------------------------------------------------------------------------
// MQTT / AWS IoT Core configuration
// -----------------------------------------------------------------------------
#define MQTT_PORT        8883
#define MQTT_PUB_TOPIC   "tomevans/nano33iot/telemetry"
#define MQTT_SUB_TOPIC   "tomevans/nano33iot/cmd"
#define MQTT_STATE_TOPIC "tomevans/nano33iot/state"

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------
WiFiClient wifi;
BearSSLClient ssl(wifi);
MqttClient mqtt(ssl);

unsigned long lastPublish = 0;
bool ledState = false;

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------
bool connectWiFi();
bool connectMQTT_TLS();
void handleIncomingMessage(int len);
void publishTelemetry();
void publishState();

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== Nano 33 IoT → AWS IoT Core Secure MQTT Demo ===");

  if (!ECCX08.begin()) {
    Serial.println("❌ ATECC608A not detected");
    while (1);
  }

  if (!connectWiFi()) {
    Serial.println("❌ WiFi failed");
    while (1);
  }

  if (!connectMQTT_TLS()) {
    Serial.println("❌ MQTT/TLS connection failed");
    while (1);
  }

  mqtt.onMessage(handleIncomingMessage);
  mqtt.subscribe(MQTT_SUB_TOPIC);

  Serial.println("✅ Setup complete. Running...");
}

// -----------------------------------------------------------------------------
// MAIN LOOP
// -----------------------------------------------------------------------------
void loop() {
  mqtt.poll();

  unsigned long now = millis();
  if (now - lastPublish > 5000) {
    lastPublish = now;
    publishTelemetry();
  }
}

// -----------------------------------------------------------------------------
// CONNECT TO WIFI
// -----------------------------------------------------------------------------
bool connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("✅ WiFi connected. IP=");
    Serial.println(WiFi.localIP());
    return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
// CONNECT TO AWS IoT CORE WITH TLS + ECC CERTIFICATE
// -----------------------------------------------------------------------------
bool connectMQTT_TLS() {
  // Initialize TLS clock (used when cert verification is enabled)
  ArduinoBearSSL.onGetTime([]() { return (uint32_t)WiFi.getTime(); });

  // Client authentication: ECC slot 0 + your device certificate (DER)
  // (device_cert_der/device_cert_der_len come from device_cert_der.h)
  ssl.setEccSlot(0, device_cert_der, device_cert_der_len);

  // Try to enable secure server verification using the Root CA PEM.
  bool ca_ok = configureAwsRootCA(ssl);

  if (!ca_ok) {
    // TEMPORARY fallback: skip server verification but keep SNI ON (enum 1).
    // This keeps mTLS client auth intact while you add proper trust anchors.
    ssl.setInsecure((BearSSLClient::SNI)1);
    Serial.println("⚠️  Root CA helper not available — running TEMPORARILY without server verification.");
  }

  // Connect using the hostname (ensures SNI uses AWS endpoint)
  const int port = 8883;
  if (!mqtt.connect(AWS_ENDPOINT, port)) {
    Serial.print("❌ MQTT connect failed: ");
    Serial.println(mqtt.connectError());
    return false;
  }

  Serial.println("✅ MQTT connected to AWS IoT Core");
  return true;
}


// -----------------------------------------------------------------------------
// HANDLE INCOMING MESSAGE
// -----------------------------------------------------------------------------
void handleIncomingMessage(int len) {
  String cmd = "";
  while (mqtt.available()) {
    cmd += (char)mqtt.read();
  }

  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "LED ON") {
    ledState = true;
    digitalWrite(LED_BUILTIN, HIGH);
  } else if (cmd == "LED OFF") {
    ledState = false;
    digitalWrite(LED_BUILTIN, LOW);
  }

  publishState();
}

// -----------------------------------------------------------------------------
// PUBLISH TELEMETRY MESSAGE
// -----------------------------------------------------------------------------
void publishTelemetry() {
  char msg[64];
  snprintf(msg, sizeof(msg), "{\"uptime_ms\":%lu}", millis());
  mqtt.beginMessage(MQTT_PUB_TOPIC);
  mqtt.print(msg);
  mqtt.endMessage();
}

// -----------------------------------------------------------------------------
// PUBLISH LED STATE
// -----------------------------------------------------------------------------
void publishState() {
  char msg[64];
  snprintf(msg, sizeof(msg), "{\"led\":%s}", ledState ? "true" : "false");
  mqtt.beginMessage(MQTT_STATE_TOPIC);
  mqtt.print(msg);
  mqtt.endMessage();
}
