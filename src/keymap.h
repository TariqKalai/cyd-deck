#pragma once
// Logique pure (sans Arduino ni BLE) : transforme "ctrl+shift+m" en codes HID.
// Testable sur PC avec g++.
#include <stdint.h>
#include <stddef.h>

// Modificateurs HID (bits du rapport clavier) : LCTRL=1, LSHIFT=2, LALT=4, LGUI=8, RALT=0x40
struct KeyPlan {
  uint8_t mods = 0;      // masque de modificateurs
  uint8_t usage = 0;     // code HID "usage" de la touche (0 = aucune)
  bool ok = false;
};

// Combinaison de raccourci. 'azerty' : les lettres designent le caractere produit
// sur un clavier AZERTY belge/francais (donc a<->q, z<->w, m -> position ';').
KeyPlan keymapParseCombo(const char* combo, bool azerty);

// Caractere unique pour la frappe de texte sur un hote AZERTY belge.
// Renvoie false si le caractere n'est pas supporte.
bool keymapAzertyChar(char c, uint8_t& usage, bool& shift);

// Codes HID des modificateurs -> octet de modificateur ("press(0x80+bit)" cote BleKeyboard)
inline uint8_t keymapModBit(uint8_t mask) {
  uint8_t b = 0;
  while (mask > 1) { mask >>= 1; b++; }
  return b;
}
