#include "keys.h"
#include "keymap.h"
#include "config.h"
#include <BleKeyboard.h>

static BleKeyboard kb(BLE_NAME, "DIY", 100);

void keysBegin() { kb.begin(); }
bool keysConnected() { return kb.isConnected(); }

// Les "usage" HID s'envoient via press(usage + 136) (convention de BleKeyboard pour les touches non-ASCII).
static void pressUsage(uint8_t usage) { kb.press((uint8_t)(usage + 136)); }

static void pressMods(uint8_t mask) {
  for (uint8_t bit = 0; bit < 8; bit++)
    if (mask & (1u << bit)) kb.press((uint8_t)(0x80 + bit));
}

static bool runMedia(const char* v) {
  if (!strcmp(v, "playpause")) kb.write(KEY_MEDIA_PLAY_PAUSE);
  else if (!strcmp(v, "next")) kb.write(KEY_MEDIA_NEXT_TRACK);
  else if (!strcmp(v, "prev")) kb.write(KEY_MEDIA_PREVIOUS_TRACK);
  else if (!strcmp(v, "stop")) kb.write(KEY_MEDIA_STOP);
  else if (!strcmp(v, "mute")) kb.write(KEY_MEDIA_MUTE);
  else if (!strcmp(v, "volup")) kb.write(KEY_MEDIA_VOLUME_UP);
  else if (!strcmp(v, "voldown")) kb.write(KEY_MEDIA_VOLUME_DOWN);
  else return false;
  return true;
}

bool keysRun(const char* action, const char* value, bool azerty, String& msg) {
  if (!kb.isConnected()) { msg = "Pas connecte en Bluetooth"; return false; }

  if (!strcmp(action, "media")) {
    if (!runMedia(value)) { msg = "Touche media inconnue"; return false; }
    msg = "OK";
    return true;
  }

  if (!strcmp(action, "text")) {
    if (!azerty) {                       // hote QWERTY US : BleKeyboard sait deja faire
      kb.print(value);
      msg = "OK";
      return true;
    }
    int skipped = 0;
    for (const char* p = value; *p; ++p) {
      uint8_t u; bool sh;
      if (!keymapAzertyChar(*p, u, sh)) { skipped++; continue; }
      if (sh) kb.press(KEY_LEFT_SHIFT);
      pressUsage(u);
      delay(12);
      kb.releaseAll();
      delay(12);
    }
    msg = skipped ? String("OK (") + skipped + " car. ignores)" : "OK";
    return true;
  }

  // action == "key"
  KeyPlan plan = keymapParseCombo(value, azerty);
  if (!plan.ok) { msg = "Raccourci invalide"; return false; }
  pressMods(plan.mods);
  delay(10);
  if (plan.usage) pressUsage(plan.usage);
  delay(40);
  kb.releaseAll();
  msg = "OK";
  return true;
}
