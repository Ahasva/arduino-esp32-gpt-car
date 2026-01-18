#include "car_executor.h"
#include <ArduinoJson.h>
#include <WebServer.h>
#include "telemetry.h"

// Provided by main.cpp
extern WebServer server;
extern HardwareSerial Serial2;

// ---------------- low-level car I/O ----------------

static String gCurrentMotion = "stop"; // forward/backward/left/right/stop

static void sendCarChar(char c) {
  Serial2.print(c);
  Serial2.print('\n');
}

static void sendCarActionWord(const String& action) {
  if (action == "forward") { gCurrentMotion = "forward"; sendCarChar('F'); }
  else if (action == "backward") { gCurrentMotion = "backward"; sendCarChar('B'); }
  else if (action == "left") { gCurrentMotion = "left"; sendCarChar('L'); }
  else if (action == "right") { gCurrentMotion = "right"; sendCarChar('R'); }
  else { gCurrentMotion = "stop"; sendCarChar('S'); }
}

void sendCarStop() {
  gCurrentMotion = "stop";
  sendCarChar('S');
}

// ---------------- waits ----------------

static bool execConditionWait(const String& sensor, bool desired, uint32_t timeoutMs) {
  uint32_t start = millis();

  while (true) {
    // keep HTTP responsive + keep telemetry fresh
    server.handleClient();
    pumpTelemetry();

    bool current = obstacleForSensor(sensor);
    if (current == desired) return true;
    if (timeoutMs > 0 && (millis() - start) > timeoutMs) return false;
    delay(20);
  }
}

// ---------------- executor ----------------

String executePlanJson(const String& planJson) {
  // Parse plan JSON
  DynamicJsonDocument planDoc(4096);
  DeserializationError err = deserializeJson(planDoc, planJson);
  if (err) {
    return "{\"ok\":false,\"error\":\"Plan JSON invalid\"}";
  }

  JsonArray seq = planDoc["sequence"];
  if (seq.isNull()) {
    return "{\"ok\":false,\"error\":\"Missing sequence\"}";
  }

  // Safety limits
  const size_t maxSteps = 25;
  if (seq.size() == 0) {
    return "{\"ok\":false,\"error\":\"Empty sequence\"}";
  }
  if (seq.size() > maxSteps) {
    return "{\"ok\":false,\"error\":\"Too many steps\"}";
  }

  // Execute steps
  for (size_t i = 0; i < seq.size(); i++) {
    server.handleClient();
    pumpTelemetry();

    JsonObject step = seq[i];
    const char* actionC = step["action"];

    if (!actionC) {
      sendCarStop();
      return "{\"ok\":false,\"error\":\"Step missing action\"}";
    }

    String action = String(actionC);
    action.toLowerCase();

    // ---------------- movement actions (NEW DEFAULT BURST) ----------------
    // stop remains immediate
    if (action == "stop") {
      sendCarStop();
      continue;
    }

    // forward/back/left/right -> do a short burst then stop
    if (action == "forward" || action == "backward" || action == "left" || action == "right") {
      sendCarActionWord(action);

      // default burst (prevents infinite driving if model forgets durationWait)
      uint32_t start = millis();
      while (millis() - start < 2000) {  // 600ms burst
        server.handleClient();
        pumpTelemetry();

        // emergency stop if obstacle appears while driving forward
        if (gCurrentMotion == "forward" && gObsCenter) {
          sendCarStop();
          return "{\"ok\":false,\"error\":\"Emergency stop: obstacle center\"}";
        }
        delay(10);
      }

      // after burst, stop automatically
      sendCarStop();
      continue;
    }

    // ---------------- durationWait ----------------
    if (action == "durationwait") {
      long duration = step["parameters"]["duration"] | 0;
      if (duration < 0) duration = 0;
      if (duration > 15000) duration = 15000;

      uint32_t start = millis();
      while ((millis() - start) < (uint32_t)duration) {
        server.handleClient();
        pumpTelemetry();

        if (gCurrentMotion == "forward" && gObsCenter) {
          sendCarStop();
          return "{\"ok\":false,\"error\":\"Emergency stop: obstacle center\"}";
        }
        delay(10);
      }
      continue;
    }

    // ---------------- conditionWait ----------------
    if (action == "conditionwait") {
      const char* sensorC = step["parameters"]["condition"]["sensor"] | "";
      bool desired = step["parameters"]["condition"]["state"] | false;
      uint32_t timeoutMs = step["parameters"]["timeoutMs"] | 15000;

      String sensor = String(sensorC);
      sensor.toLowerCase();

      // validate sensor name (same as your original)
      if (!(sensor == "left" || sensor == "center" || sensor == "right")) {
        sendCarStop();
        return "{\"ok\":false,\"error\":\"Invalid sensor in conditionWait\"}";
      }

      bool ok = execConditionWait(sensor, desired, timeoutMs);
      if (!ok) {
        sendCarStop();
        return "{\"ok\":false,\"error\":\"conditionWait timeout\"}";
      }
      continue;
    }

    // unknown action
    sendCarStop();
    return "{\"ok\":false,\"error\":\"Unknown action\"}";
  }
  return "{\"ok\":true}";
}