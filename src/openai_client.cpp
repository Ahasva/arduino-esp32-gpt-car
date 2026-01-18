#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "openai_client.h"

String sendToOpenAI(const char* systemPrompt, const String& userMessage) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected (OpenAI)");
    return "";
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  const String uri = "https://api.openai.com/v1/chat/completions";
  if (!http.begin(client, uri)) {
    Serial.println("OpenAI: http.begin() failed");
    return "";
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(Secrets::OpenAI));

  StaticJsonDocument<1200> doc;
  doc["model"] = "gpt-4o-mini";
  doc["temperature"] = 0.2;

  JsonArray messages = doc.createNestedArray("messages");

  JsonObject sys = messages.createNestedObject();
  sys["role"] = "system";
  sys["content"] = systemPrompt;

  JsonObject usr = messages.createNestedObject();
  usr["role"] = "user";
  usr["content"] = userMessage;

  String payload;
  serializeJson(doc, payload);

  int code = http.POST(payload);
  String resp = http.getString();
  http.end();        // frees HTTPClient resources
  client.stop();     // clise TLS socket

  Serial.print("OpenAI HTTP code: ");
  Serial.println(code);

  if (code <= 0) {
    Serial.println(resp);
    return "";
  }

  // Use a filter so ArduinoJson only keeps what we need
  StaticJsonDocument<256> filter;
  filter["choices"][0]["message"]["content"] = true;

  DynamicJsonDocument respDoc(2048);
  DeserializationError err = deserializeJson(respDoc, resp, DeserializationOption::Filter(filter));
  if (err) {
    Serial.print("OpenAI parse error: ");
    Serial.println(err.c_str());
    return "";
  }

  const char* content = respDoc["choices"][0]["message"]["content"];
  if (!content) return "";
  return String(content);
}