#pragma once
#include <Arduino.h>
#include "config.h"

struct Btn {
  bool used = false;
  char label[24] = "";
  uint32_t color = 0x3A6EA5;     // 0xRRGGBB
  char action[8] = "key";        // key | media | text
  char value[64] = "";
};

struct Page {
  char name[20] = "";
  Btn btn[GRID_SIZE];
};

struct Deck {
  Page pages[MAX_PAGES];
  uint8_t nPages = 0;
  bool invert = false;           // inversion des couleurs de l'ecran
  uint8_t brightness = 200;      // 10..255
  bool azerty = true;            // disposition du clavier du PC
};

extern Deck deck;

void   storeBegin();                                   // monte LittleFS et la carte SD (si presente)
bool   storeHasSd();
void   storeLoad();                                    // charge la config (SD > LittleFS > defaut)
bool   storeApplyJson(const String& json, String& err);// valide, applique et enregistre
String storeJson();                                    // config actuelle en JSON
void   storeSave();                                    // enregistre la config actuelle (fichier)
void   storeFactoryReset();                            // efface config + reglages

// Petits reglages persistants (Wi-Fi, mot de passe) - NVS
String prefGet(const char* key, const char* def = "");
void   prefSet(const char* key, const String& v);
