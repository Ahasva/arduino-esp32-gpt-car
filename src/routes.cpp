#include "routes.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "openai_client.h"
#include "weather_client.h"
#include "car_executor.h"
#include "prompts.h"
#include "telemetry.h"
#include "fs_web.h"

extern String obstacleData;
extern String distanceCm;

// --- helper: build planner input (moved from main.cpp) ---
static String buildCarPlannerInput(const String& userGoal) {
  String s;
  s += "User goal:\n";
  s += userGoal;
  s += "\n\n";

  s += "Current sensor snapshot (obstaclePresent booleans):\n";
  s += "{";
  s += "\"left\":";
  s += (gObsLeft ? "true" : "false");
  s += ",\"center\":";
  s += (gObsCenter ? "true" : "false");
  s += ",\"right\":";
  s += (gObsRight ? "true" : "false");
  s += ",\"distanceCm\":";
  if (isnan(gDistanceCm)) s += "null";
  else s += String(gDistanceCm, 1);
  s += "}\n";

  s += "\nNotes:\n";
  s += "- If center obstaclePresent is true, forward is unsafe.\n";
  s += "- If left obstaclePresent is true, turning left is risky.\n";
  s += "- If right obstaclePresent is true, turning right is risky.\n";
  s += "- Prefer short plans (<= 8 steps) so we can replan often.\n";

  return s;
}

void setupRoutes(WebServer& server) {

  // ---------------- API ROUTES ----------------

  server.on("/ping", HTTP_GET, [&server]() {
    server.send(200, "text/plain", "pong");
  });

  server.on("/favicon.ico", HTTP_GET, [&server]() {
    server.send(204); // no content
  });

  server.on("/status", HTTP_GET, [&server]() {
    server.send(200, "text/plain", obstacleData + "," + distanceCm);
  });

  server.on("/stopAll", HTTP_POST, [&server]() {
    sendCarStop();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  // Manual driving endpoint (direct, no burst)
  // Body: {"command":"forward"|"backward"|"left"|"right"|"stop"}
  server.on("/car", HTTP_POST, [&server]() {
    String raw = server.arg("plain");
    if (raw.length() == 0) raw = server.arg(0);

    if (raw.length() == 0) {
      server.send(400, "application/json", "{\"error\":\"Empty body\"}");
      return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, raw);
    if (err) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    String cmd = doc["command"] | "";
    cmd.toLowerCase();

    // direct UART commands to Arduino (no auto-stop here)
    if (cmd == "forward")       Serial2.print("F\n");
    else if (cmd == "backward") Serial2.print("B\n");
    else if (cmd == "left")     Serial2.print("L\n");
    else if (cmd == "right")    Serial2.print("R\n");
    else if (cmd == "stop")     Serial2.print("S\n");
    else {
      server.send(400, "application/json", "{\"error\":\"Unknown command\"}");
      return;
    }

    StaticJsonDocument<128> out;
    out["ok"] = true;
    out["command"] = cmd;
    String json;
    serializeJson(out, json);
    server.send(200, "application/json", json);
  });

  // 1) Planner endpoint (human text -> plan JSON)
  server.on("/carPlan", HTTP_POST, [&server]() {
    String raw = server.arg("plain");
    if (raw.length() == 0) raw = server.arg(0);

    if (raw.length() == 0) {
      server.send(400, "application/json", "{\"error\":\"Empty body\"}");
      return;
    }

    StaticJsonDocument<512> in;
    DeserializationError err = deserializeJson(in, raw);
    if (err) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    String msg = in["message"] | "";
    if (msg.length() == 0) {
      server.send(400, "application/json", "{\"error\":\"Empty message\"}");
      return;
    }

    String plannerInput = buildCarPlannerInput(msg);

    String planText = sendToOpenAI(CAR_SYSTEM, plannerInput);
    if (planText.length() == 0) {
      server.send(500, "application/json", "{\"error\":\"OpenAI failed\"}");
      return;
    }

    // trim + strip to JSON object (in case model adds text)
    planText.trim();
    int a = planText.indexOf('{');
    int b = planText.lastIndexOf('}');
    if (a >= 0 && b > a) planText = planText.substring(a, b + 1);

    // validate JSON + has sequence
    DynamicJsonDocument planDoc(4096);
    DeserializationError perr = deserializeJson(planDoc, planText);
    if (perr || planDoc["sequence"].isNull()) {
      server.send(500, "application/json", "{\"error\":\"Planner returned invalid plan\"}");
      return;
    }

    server.send(200, "application/json", planText);
  });

  // 2) Executor endpoint (plan JSON -> runs motors)
  server.on("/carExec", HTTP_POST, [&server]() {
    String body = server.arg("plain");
    if (body.length() == 0) body = server.arg(0);

    if (body.length() == 0) {
      server.send(400, "application/json", "{\"error\":\"Empty body\"}");
      return;
    }

    String res = executePlanJson(body);
    server.send(200, "application/json", res);
  });

  // Chat endpoint
  server.on("/chat", HTTP_POST, [&server]() {
    String raw = server.arg("plain");
    if (raw.length() == 0) raw = server.arg(0);

    StaticJsonDocument<256> in;
    DeserializationError err = deserializeJson(in, raw);
    if (err) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    String msg = in["message"] | "";
    if (msg.length() == 0) {
      server.send(400, "application/json", "{\"error\":\"Empty message\"}");
      return;
    }

    String out = sendToOpenAI(CHAT_SYSTEM, msg);

    StaticJsonDocument<768> resp;
    resp["response"] = out;
    String json;
    serializeJson(resp, json);
    server.send(200, "application/json", json);
  });

  // Weather endpoint
  server.on("/getWeather", HTTP_POST, [&server]() {
    String raw = server.arg("plain");
    if (raw.length() == 0) raw = server.arg(0);

    StaticJsonDocument<256> in;
    DeserializationError err = deserializeJson(in, raw);
    if (err) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    String msg = in["message"] | "";
    if (msg.length() == 0) {
      server.send(400, "application/json", "{\"error\":\"Empty message\"}");
      return;
    }

    String engineered =
      "Extract ONLY the city name from this user input. Return only the city name:\n" + msg;

    String city = sendToOpenAI(WEATHER_SYSTEM, engineered);
    city.trim();

    if (city.length() == 0) {
      StaticJsonDocument<256> resp;
      resp["response"] = "Could not detect city";
      String json;
      serializeJson(resp, json);
      server.send(200, "application/json", json);
      return;
    }

    city.replace(" ", "%20");
    String weather = getWeatherData(city);

    StaticJsonDocument<768> resp;
    resp["response"] = weather;
    String json;
    serializeJson(resp, json);
    server.send(200, "application/json", json);
  });

  // ---------------- STATIC FILE ROUTES ----------------
  server.on("/", HTTP_GET, [&server]() {
    if (!handleFileRead(server, "/index.html")) {
      server.send(404, "text/plain", "Missing /index.html");
    }
  });

  server.onNotFound([&server]() {
    if (!handleFileRead(server, server.uri())) {
      server.send(404, "text/plain", "FileNotFound: " + server.uri());
    }
  });
}