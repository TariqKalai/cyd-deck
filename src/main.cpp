#include <Arduino.h>
#include "store.h"
#include "ui.h"
#include "keys.h"
#include "net.h"

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nCYD Deck demarre");
  storeBegin();   // LittleFS + SD + NVS
  storeLoad();    // pages/boutons/reglages
  uiBegin();      // ecran + tactile + LVGL
  keysBegin();    // clavier Bluetooth
  netBegin();     // Wi-Fi + page web + OTA (non bloquant)
}

void loop() {
  uiLoop();
  netLoop();
}
