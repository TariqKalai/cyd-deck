#include "keymap.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

struct Named { const char* name; uint8_t usage; };

static const Named NAMED[] = {
  {"enter", 0x28}, {"return", 0x28}, {"esc", 0x29}, {"escape", 0x29},
  {"backspace", 0x2a}, {"tab", 0x2b}, {"space", 0x2c}, {"capslock", 0x39},
  {"printscreen", 0x46}, {"prtsc", 0x46}, {"scrolllock", 0x47}, {"pause", 0x48},
  {"insert", 0x49}, {"home", 0x4a}, {"pageup", 0x4b}, {"delete", 0x4c}, {"del", 0x4c},
  {"end", 0x4d}, {"pagedown", 0x4e}, {"right", 0x4f}, {"left", 0x50},
  {"down", 0x51}, {"up", 0x52}, {"menu", 0x65},
};

static uint8_t modFromName(const char* t) {
  if (!strcmp(t, "ctrl") || !strcmp(t, "control")) return 0x01;
  if (!strcmp(t, "shift")) return 0x02;
  if (!strcmp(t, "alt")) return 0x04;
  if (!strcmp(t, "win") || !strcmp(t, "gui") || !strcmp(t, "cmd") ||
      !strcmp(t, "meta") || !strcmp(t, "super")) return 0x08;
  if (!strcmp(t, "altgr")) return 0x40;
  return 0;
}

// Caractere -> usage HID *position physique US*. En AZERTY on permute pour obtenir le bon caractere.
static bool charUsage(char c, bool azerty, uint8_t& u) {
  c = (char)tolower((unsigned char)c);
  if (azerty) {
    switch (c) {
      case 'a': c = 'q'; break;
      case 'q': c = 'a'; break;
      case 'z': c = 'w'; break;
      case 'w': c = 'z'; break;
      case 'm': u = 0x33; return true;   // 'm' est a la position du ';' US
      default: break;
    }
  }
  if (c >= 'a' && c <= 'z') { u = 0x04 + (c - 'a'); return true; }
  if (c >= '1' && c <= '9') { u = 0x1e + (c - '1'); return true; }
  if (c == '0') { u = 0x27; return true; }
  switch (c) {
    case '-': u = 0x2d; return true;
    case '=': u = 0x2e; return true;
    case '[': u = 0x2f; return true;
    case ']': u = 0x30; return true;
    case '\\': u = 0x31; return true;
    case ';': u = 0x33; return true;
    case '\'': u = 0x34; return true;
    case '`': u = 0x35; return true;
    case ',': u = 0x36; return true;
    case '.': u = 0x37; return true;
    case '/': u = 0x38; return true;
    default: return false;
  }
}

KeyPlan keymapParseCombo(const char* combo, bool azerty) {
  KeyPlan plan;
  if (!combo) return plan;
  char buf[64];
  strncpy(buf, combo, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;

  char* save = nullptr;
  for (char* tok = strtok_r(buf, "+ ", &save); tok; tok = strtok_r(nullptr, "+ ", &save)) {
    for (char* p = tok; *p; ++p) *p = (char)tolower((unsigned char)*p);

    uint8_t m = modFromName(tok);
    if (m) { plan.mods |= m; continue; }
    if (plan.usage) return plan;               // une seule touche "normale" par combinaison

    bool found = false;
    for (size_t i = 0; i < sizeof(NAMED) / sizeof(NAMED[0]); ++i) {
      if (!strcmp(tok, NAMED[i].name)) { plan.usage = NAMED[i].usage; found = true; break; }
    }
    if (!found && tok[0] == 'f' && isdigit((unsigned char)tok[1])) {
      int n = atoi(tok + 1);
      if (n >= 1 && n <= 12) { plan.usage = (uint8_t)(0x3a + n - 1); found = true; }
      else if (n >= 13 && n <= 24) { plan.usage = (uint8_t)(0x68 + n - 13); found = true; }
    }
    if (!found && tok[1] == 0) {
      uint8_t u;
      if (charUsage(tok[0], azerty, u)) { plan.usage = u; found = true; }
    }
    if (!found) return plan;                   // touche inconnue -> ok = false
  }
  plan.ok = (plan.usage != 0 || plan.mods != 0);
  return plan;
}

bool keymapAzertyChar(char c, uint8_t& usage, bool& shift) {
  shift = false;
  if (c == ' ') { usage = 0x2c; return true; }
  if (c >= 'A' && c <= 'Z') { shift = true; return charUsage(c, true, usage); }
  if (c >= 'a' && c <= 'z') return charUsage(c, true, usage);
  if (c >= '0' && c <= '9') { shift = true; return charUsage(c, true, usage); }   // chiffres = Maj sur AZERTY
  switch (c) {
    // rangee des chiffres (sans Maj) : & e " ' ( S e ! c a ) -
    case '&':  usage = 0x1e; return true;
    case '"':  usage = 0x20; return true;
    case '\'': usage = 0x21; return true;
    case '(':  usage = 0x22; return true;
    case '!':  usage = 0x25; return true;
    case ')':  usage = 0x2d; return true;
    case '-':  usage = 0x2e; return true;
    // rangee du bas : , ; : =  (et . ? / + avec Maj)
    case ',':  usage = 0x10; return true;
    case ';':  usage = 0x36; return true;
    case ':':  usage = 0x37; return true;
    case '=':  usage = 0x38; return true;
    case '.':  usage = 0x36; shift = true; return true;
    case '?':  usage = 0x10; shift = true; return true;
    case '/':  usage = 0x37; shift = true; return true;
    case '+':  usage = 0x38; shift = true; return true;
    default:   return false;
  }
}
