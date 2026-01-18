#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include <LittleFS.h>
#define FILESYSTEM LittleFS

#include "fs_web.h"
#include "routes.h"
#include "telemetry.h"

WebServer server(80);

void setup() {
  Serial.begin(115200);

  // UART to Arduino
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  Serial2.setTimeout(10);

  // Filesystem
  if (!FILESYSTEM.begin(false)) {
    Serial.println("LittleFS mount failed! Try: pio run -t uploadfs");
  } else {
    Serial.println("LittleFS mounted OK");
    listFiles();
  }

  // WiFi
  WiFi.mode(WIFI_AP_STA);
  WiFiManager wm;
  bool ok = wm.autoConnect("ESP", "esp_test_pw");
  if (!ok) {
    Serial.println("Not connected!");
  } else {
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
  }

  // mDNS
  if (MDNS.begin("esp32")) {
    Serial.println("mDNS started: http://esp32.local/");
  } else {
    Serial.println("mDNS failed");
  }

  // All HTTP routes live in routes.cpp (including static file serving)
  setupRoutes(server);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  pumpTelemetry();
}