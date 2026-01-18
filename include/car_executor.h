#pragma once
#include <Arduino.h>

// Executes {"sequence":[...]} and returns a JSON result string: {"ok":true} or {"ok":false,"error":"..."}
String executePlanJson(const String& planJson);

// Emergency stop (sends 'S' to the car)
void sendCarStop();