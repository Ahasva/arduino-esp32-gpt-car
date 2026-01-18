#include "telemetry.h"

String obstacleData = "";
String distanceCm = "";

float temperature = NAN;
int humidity = 0;
float windSpeed = 0;
String city = "";
String weatherDescription = "";

// Latest sensor state from Arduino telemetry (S:xyz,dist)
bool gObsLeft = false;    // obstacle present?
bool gObsCenter = false;  // obstacle present?
bool gObsRight = false;   // obstacle present?
float gDistanceCm = NAN;

bool obstacleForSensor(const String& sensor) {
  if (sensor == "left") return gObsLeft;
  if (sensor == "center") return gObsCenter;
  if (sensor == "right") return gObsRight;
  return false;
}

void pumpTelemetry() {
  static String buf;

  while (Serial2.available()) {
    char c = (char)Serial2.read();

    if (c == '\n') {
      buf.trim();

      // Only accept lines like: S:101,19.7
      if (buf.startsWith("S:")) {
        int comma = buf.indexOf(',');
        if (comma >= 0) {
          String bits = buf.substring(2, comma);
          String dist = buf.substring(comma + 1);
          bits.trim();
          dist.trim();

          if (bits.length() == 3) {
            obstacleData = bits;
            distanceCm = dist;

            // telemetry: '1' = clear, '0' = obstacle
            gObsLeft   = (bits[0] == '0');
            gObsCenter = (bits[1] == '0');
            gObsRight  = (bits[2] == '0');

            gDistanceCm = dist.toFloat();
          }
        }
      }

      buf = "";
    } else if (c != '\r') {
      buf += c;
      if (buf.length() > 200) buf = ""; // safety
    }
  }
}