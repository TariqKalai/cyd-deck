#include "net.h"
#include "config.h"
#include "store.h"
#include "keys.h"
#include "ui.h"
#include "webui.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <ArduinoJson.h>

static WebServer server(80);
static bool servicesUp = false;
static bool apMode = false;
static uint32_t wifiStart = 0;
static uint32_t restartAt = 0;
static bool uploadAuthOk = false;

static String adminPass() { return prefGet("pass", DEFAULT_PASS); }

static bool auth() {
  if (server.authenticate("admin", adminPass().c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, "CYD Deck");
  return false;
}

static void sendJson(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

static void sendErr(int code, const String& msg) {
  JsonDocument d;
  d["error"] = msg;
  String s;
  serializeJson(d, s);
  sendJson(code, s);
}

// ------------------------------------------------------------------ routes

static void routeIndex() {
  if (!auth()) return;
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void routeState() {
  if (!auth()) return;
  JsonDocument d;
  d["fw"] = FW_VERSION;
  d["ble"] = keysConnected();
  d["ip"] = netIp();
  d["ap"] = apMode;
  d["sd"] = storeHasSd();
  d["heap"] = ESP.getFreeHeap();
  d["uptime"] = millis() / 1000;
  d["host"] = String(DEVICE_HOSTNAME) + ".local";
  String s;
  serializeJson(d, s);
  sendJson(200, s);
}

static void routeConfigGet() {
  if (!auth()) return;
  sendJson(200, storeJson());
}

static void routeConfigPost() {
  if (!auth()) return;
  String err;
  if (!storeApplyJson(server.arg("plain"), err)) { sendErr(400, err); return; }
  uiRebuild();
  sendJson(200, "{\"ok\":true}");
}

static void routePress() {
  if (!auth()) return;
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) { sendErr(400, "JSON invalide"); return; }
  String msg;
  bool ok = keysRun(d["action"] | "key", d["value"] | "", deck.azerty, msg);
  JsonDocument r;
  r["ok"] = ok;
  r["msg"] = msg;
  String s;
  serializeJson(r, s);
  sendJson(ok ? 200 : 409, s);
}

static void routeWifi() {
  if (!auth()) return;
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) { sendErr(400, "JSON invalide"); return; }
  String ssid = d["ssid"] | "";
  if (ssid.isEmpty()) { sendErr(400, "SSID vide"); return; }
  prefSet("ssid", ssid);
  prefSet("wpass", String((const char*)(d["pass"] | "")));
  sendJson(200, "{\"ok\":true,\"restart\":true}");
  restartAt = millis() + 800;
}

static void routePass() {
  if (!auth()) return;
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) { sendErr(400, "JSON invalide"); return; }
  String p = d["pass"] | "";
  if (p.length() < 4) { sendErr(400, "Minimum 4 caracteres"); return; }
  prefSet("pass", p);
  sendJson(200, "{\"ok\":true,\"restart\":true}");   // le mot de passe OTA change aussi
  restartAt = millis() + 800;
}

static void routeUpdateDone() {
  if (!uploadAuthOk) { server.requestAuthentication(BASIC_AUTH, "CYD Deck"); return; }
  bool ok = !Update.hasError();
  server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : String("ECHEC: ") + Update.errorString());
  if (ok) restartAt = millis() + 800;
}

static void routeUpdateUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    uploadAuthOk = server.authenticate("admin", adminPass().c_str());
    if (!uploadAuthOk) return;
    uiToast("Mise a jour du firmware...");
    uiRenderNow();
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (uploadAuthOk) Update.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (uploadAuthOk) Update.end(true);
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (uploadAuthOk) Update.abort();
  }
}

static void startServices() {
  if (servicesUp) return;
  servicesUp = true;

  MDNS.begin(DEVICE_HOSTNAME);
  MDNS.addService("http", "tcp", 80);

  server.on("/", HTTP_GET, routeIndex);
  server.on("/api/state", HTTP_GET, routeState);
  server.on("/api/config", HTTP_GET, routeConfigGet);
  server.on("/api/config", HTTP_POST, routeConfigPost);
  server.on("/api/press", HTTP_POST, routePress);
  server.on("/api/wifi", HTTP_POST, routeWifi);
  server.on("/api/pass", HTTP_POST, routePass);
  server.on("/update", HTTP_POST, routeUpdateDone, routeUpdateUpload);
  server.onNotFound([]() { server.send(404, "text/plain", "404"); });
  server.begin();

  ArduinoOTA.setHostname(DEVICE_HOSTNAME);
  ArduinoOTA.setPassword(adminPass().c_str());
  ArduinoOTA.onStart([]() { uiToast("Mise a jour OTA..."); uiRenderNow(); });
  ArduinoOTA.begin();
}

// ------------------------------------------------------------------ Wi-Fi

static void startAp() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("[net] point d'acces %s (mdp %s) -> %s\n", AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());
  startServices();
}

void netBegin() {
  String ssid = prefGet("ssid");
  WiFi.setHostname(DEVICE_HOSTNAME);
  if (ssid.isEmpty()) { startAp(); return; }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), prefGet("wpass").c_str());
  wifiStart = millis();
  Serial.printf("[net] connexion a %s...\n", ssid.c_str());
}

String netIp() { return apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); }
bool netIsAp() { return apMode; }
bool netConnected() { return !apMode && WiFi.status() == WL_CONNECTED; }

// ------------------------------------------------------------------ console serie (Web Serial)

static void handleCommand(String line) {
  line.trim();
  if (line.isEmpty()) return;
  if (line == "help") {
    Serial.println("Commandes: help | status | wifi SSID|MOTDEPASSE | pass NOUVEAU | reboot | factory");
  } else if (line == "status") {
    Serial.printf("fw=%s ble=%d ip=%s ap=%d sd=%d heap=%u\n", FW_VERSION, keysConnected(),
                  netIp().c_str(), apMode, storeHasSd(), ESP.getFreeHeap());
  } else if (line.startsWith("wifi ")) {
    String arg = line.substring(5);
    int bar = arg.indexOf('|');
    String ssid = bar < 0 ? arg : arg.substring(0, bar);
    String pass = bar < 0 ? "" : arg.substring(bar + 1);
    ssid.trim();
    if (ssid.isEmpty()) { Serial.println("SSID vide"); return; }
    prefSet("ssid", ssid);
    prefSet("wpass", pass);
    Serial.println("Wi-Fi enregistre, redemarrage...");
    restartAt = millis() + 500;
  } else if (line.startsWith("pass ")) {
    String p = line.substring(5);
    p.trim();
    if (p.length() < 4) { Serial.println("Minimum 4 caracteres"); return; }
    prefSet("pass", p);
    Serial.println("Mot de passe enregistre, redemarrage...");
    restartAt = millis() + 500;
  } else if (line == "reboot") {
    restartAt = millis() + 200;
  } else if (line == "factory") {
    storeFactoryReset();
    Serial.println("Reinitialise, redemarrage...");
    restartAt = millis() + 500;
  } else {
    Serial.println("Commande inconnue (help)");
  }
}

static void serialPoll() {
  static String buf;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') { handleCommand(buf); buf = ""; }
    else if (buf.length() < 160) buf += c;
  }
}

// ------------------------------------------------------------------ boucle

void netLoop() {
  serialPoll();

  if (!servicesUp) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[net] connecte, IP %s\n", WiFi.localIP().toString().c_str());
      startServices();
    } else if (millis() - wifiStart > 15000) {
      Serial.println("[net] Wi-Fi injoignable -> point d'acces de configuration");
      startAp();
    }
  } else {
    server.handleClient();
    ArduinoOTA.handle();
  }

  if (restartAt && (int32_t)(millis() - restartAt) >= 0) ESP.restart();
}
