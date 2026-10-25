#include "store.h"
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <memory>

Deck deck;

static bool sdOk = false;
static SPIClass sdSpi(VSPI);
static Preferences prefs;
static const char* DECK_FILE = "/deck.json";

static const char DEFAULT_JSON[] PROGMEM = R"JSON({
"settings":{"invert":false,"brightness":200,"azerty":true},
"pages":[
{"name":"Media","buttons":[
{"i":0,"label":"Play/Pause","color":"#2E8B57","action":"media","value":"playpause"},
{"i":1,"label":"Precedent","color":"#3A6EA5","action":"media","value":"prev"},
{"i":2,"label":"Suivant","color":"#3A6EA5","action":"media","value":"next"},
{"i":3,"label":"Mute","color":"#C0392B","action":"media","value":"mute"},
{"i":4,"label":"Vol -","color":"#8E44AD","action":"media","value":"voldown"},
{"i":5,"label":"Vol +","color":"#8E44AD","action":"media","value":"volup"}]},
{"name":"Raccourcis","buttons":[
{"i":0,"label":"Bureau","color":"#2C3E50","action":"key","value":"win+d"},
{"i":1,"label":"Copier","color":"#16A085","action":"key","value":"ctrl+c"},
{"i":2,"label":"Coller","color":"#16A085","action":"key","value":"ctrl+v"},
{"i":3,"label":"Annuler","color":"#D35400","action":"key","value":"ctrl+z"},
{"i":4,"label":"Alt+Tab","color":"#7F8C8D","action":"key","value":"alt+tab"},
{"i":5,"label":"Capture","color":"#2980B9","action":"key","value":"win+shift+s"}]}
]})JSON";

// ---------------------------------------------------------------- JSON <-> Deck

static bool parseInto(const String& json, Deck& out, String& err) {
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, json);
  if (e) { err = String("JSON invalide: ") + e.c_str(); return false; }

  JsonArray pages = doc["pages"].as<JsonArray>();
  if (pages.isNull() || pages.size() == 0) { err = "Il faut au moins une page"; return false; }
  if (pages.size() > MAX_PAGES) { err = "Trop de pages (max " + String(MAX_PAGES) + ")"; return false; }

  // 'out' est toujours un Deck neuf (alloue sur le tas par l'appelant) : pas de copie temporaire sur la pile.
  JsonObject st = doc["settings"].as<JsonObject>();
  if (!st.isNull()) {
    out.invert = st["invert"] | false;
    int br = st["brightness"] | 200;
    out.brightness = (uint8_t)constrain(br, 10, 255);
    out.azerty = st["azerty"] | true;
  }

  for (JsonObject p : pages) {
    Page& pg = out.pages[out.nPages++];
    strlcpy(pg.name, p["name"] | "Page", sizeof(pg.name));
    for (JsonObject b : p["buttons"].as<JsonArray>()) {
      int i = b["i"] | -1;
      if (i < 0 || i >= GRID_SIZE) continue;
      Btn& bt = pg.btn[i];
      bt.used = true;
      strlcpy(bt.label, b["label"] | "", sizeof(bt.label));
      const char* col = b["color"] | "#3A6EA5";
      bt.color = (col[0] == '#') ? (uint32_t)strtoul(col + 1, nullptr, 16) : 0x3A6EA5;
      const char* act = b["action"] | "key";
      if (strcmp(act, "key") && strcmp(act, "media") && strcmp(act, "text")) act = "key";
      strlcpy(bt.action, act, sizeof(bt.action));
      strlcpy(bt.value, b["value"] | "", sizeof(bt.value));
    }
  }
  return true;
}

String storeJson() {
  JsonDocument doc;
  JsonObject st = doc["settings"].to<JsonObject>();
  st["invert"] = deck.invert;
  st["brightness"] = deck.brightness;
  st["azerty"] = deck.azerty;
  JsonArray pages = doc["pages"].to<JsonArray>();
  for (uint8_t p = 0; p < deck.nPages; p++) {
    JsonObject jp = pages.add<JsonObject>();
    jp["name"] = deck.pages[p].name;
    JsonArray bs = jp["buttons"].to<JsonArray>();
    for (int i = 0; i < GRID_SIZE; i++) {
      const Btn& b = deck.pages[p].btn[i];
      if (!b.used) continue;
      JsonObject jb = bs.add<JsonObject>();
      char col[8];
      snprintf(col, sizeof(col), "#%06X", (unsigned)(b.color & 0xFFFFFF));
      jb["i"] = i;
      jb["label"] = b.label;
      jb["color"] = col;
      jb["action"] = b.action;
      jb["value"] = b.value;
    }
  }
  String out;
  serializeJson(doc, out);
  return out;
}

// ---------------------------------------------------------------- fichiers

static bool writeFile(fs::FS& fs, const String& txt) {
  File f = fs.open(DECK_FILE, FILE_WRITE);
  if (!f) return false;
  size_t n = f.print(txt);
  f.close();
  return n == txt.length();
}

static String readFile(fs::FS& fs) {
  if (!fs.exists(DECK_FILE)) return "";
  File f = fs.open(DECK_FILE, FILE_READ);
  if (!f) return "";
  String s = f.readString();
  f.close();
  return s;
}

void storeBegin() {
  if (!LittleFS.begin(true)) Serial.println("[store] LittleFS indisponible");
  sdSpi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  sdOk = SD.begin(PIN_SD_CS, sdSpi, 20000000);
  Serial.printf("[store] carte SD: %s\n", sdOk ? "ok" : "absente");
  prefs.begin("deck", false);
}

bool storeHasSd() { return sdOk; }

void storeSave() {
  String js = storeJson();
  bool a = writeFile(LittleFS, js);
  bool b = sdOk ? writeFile(SD, js) : true;
  if (!a || !b) Serial.println("[store] ecriture partielle");
}

void storeLoad() {
  String js;
  if (sdOk) js = readFile(SD);
  if (js.isEmpty()) js = readFile(LittleFS);
  std::unique_ptr<Deck> tmp(new Deck());
  String err;
  if (!js.isEmpty() && parseInto(js, *tmp, err)) {
    deck = *tmp;
    Serial.println("[store] config chargee");
    return;
  }
  if (!js.isEmpty()) Serial.println("[store] config illisible (" + err + "), valeurs par defaut");
  String def = FPSTR(DEFAULT_JSON);
  parseInto(def, *tmp, err);
  deck = *tmp;
  storeSave();
}

bool storeApplyJson(const String& json, String& err) {
  std::unique_ptr<Deck> tmp(new Deck());
  if (!parseInto(json, *tmp, err)) return false;
  deck = *tmp;
  storeSave();
  return true;
}

void storeFactoryReset() {
  LittleFS.remove(DECK_FILE);
  if (sdOk) SD.remove(DECK_FILE);
  prefs.clear();
}

String prefGet(const char* key, const char* def) { return prefs.getString(key, def); }
void prefSet(const char* key, const String& v) { prefs.putString(key, v); }
