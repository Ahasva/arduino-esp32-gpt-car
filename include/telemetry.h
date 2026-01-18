#pragma once
#include <Arduino.h>

extern String obstacleData;
extern String distanceCm;

extern bool gObsLeft;
extern bool gObsCenter;
extern bool gObsRight;
extern float gDistanceCm;

void pumpTelemetry();
bool obstacleForSensor(const String& sensor);