#pragma once
#include <Arduino.h>

String sendToOpenAI(const char* systemPrompt, const String& userMessage);