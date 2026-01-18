#include "weather_client.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

static const char* units = "metric";

String getWeatherData(const String& city) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected (OpenWeather)");
    return "";
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String uri =
    "https://api.openweathermap.org/data/2.5/weather?q=" + city +
    "&appid=" + String(Secrets::OpenWeather) +
    "&units=" + String(units);

  if (!http.begin(client, uri)) {
    Serial.println("Weather: http.begin failed");
    return "";
  }

  http.setTimeout(15000);
  int httpCode = http.GET();
  String payload = http.getString();
  http.end();

  //Serial.print("Weather HTTP code: ");
  //Serial.println(httpCode);

  if (httpCode != 200) {
    Serial.println("Weather HTTP error");
    Serial.println(payload);
    return "";
  }

  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, payload)) {
    Serial.println("Weather JSON parse error");
    return "";
  }

  const char* desc = doc["weather"][0]["description"] | "";
  float temp = doc["main"]["temp"] | NAN;
  int hum = doc["main"]["humidity"] | 0;
  float wind = doc["wind"]["speed"] | 0.0;

  return "The weather in " + city +
         " is " + String(desc) +
         ", temperature " + String(temp) + " C" +
         ", humidity " + String(hum) + " %" +
         ", wind " + String(wind) + " m/s.";
}