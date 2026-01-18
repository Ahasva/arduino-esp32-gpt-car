#pragma once
#include <Arduino.h>
#include <WebServer.h>

bool handleFileRead(WebServer& server, const String& inPath);
void listFiles();