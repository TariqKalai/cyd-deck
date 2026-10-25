#pragma once
#include <Arduino.h>

void keysBegin();                       // demarre le clavier Bluetooth (BLE HID)
bool keysConnected();                   // un PC est-il connecte ?
// Execute une action ("key" | "media" | "text"). Renvoie false + message si impossible.
bool keysRun(const char* action, const char* value, bool azerty, String& msg);
