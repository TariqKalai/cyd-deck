# CYD Deck

Un Stream Deck Bluetooth sur ton CYD (ESP32-2432S028R) : l'écran tactile affiche des boutons (LVGL), le CYD se connecte à ton PC comme un **clavier Bluetooth** et envoie raccourcis, touches multimédia ou texte. Les boutons se configurent depuis une **page web hébergée par le CYD**, et le firmware se met à jour **par USB (Web Serial)** ou **par Wi-Fi (OTA)**.

> **État du projet :** le code n'a pas pu être compilé là où je l'ai écrit (accès réseau de PlatformIO bloqué). Le moteur de raccourcis (`keymap.cpp`) et la page de configuration ont été testés sur PC, le reste doit être compilé chez toi. Si `pio run` affiche une erreur, envoie-la-moi et je corrige.

## Ce que ça fait

- Grille de 3×3 boutons par page, jusqu'à 6 pages (flèches ou balayage du doigt).
- Actions : raccourci clavier (`ctrl+shift+m`, `win+d`, `alt+tab`, `f13`…), touche multimédia (lecture, volume, muet…), texte à taper.
- Réglages sur l'écran (roue crantée) : luminosité, **inversion des couleurs**, clavier AZERTY/QWERTY.
- Page web (`http://cyd-deck.local`) : éditeur de pages/boutons avec test en direct, réglages, envoi du firmware, Wi-Fi, mot de passe, import/export.
- Config sauvegardée dans la mémoire interne **et** sur la carte SD (`/deck.json`) si elle est présente.

## Installation (Linux + PlatformIO)

```bash
cd cyd-deck
pio run -e cyd_ili9341 -t upload      # CYD à 1 port USB (ILI9341)
# ou
pio run -e cyd_st7789  -t upload      # CYD à 2 ports USB (ST7789)
pio device monitor                    # console série (115200)
```

Si le port est refusé : `sudo usermod -aG dialout $USER` (ou `uucp` selon la distribution), puis reconnecte-toi.

### Flasher depuis le navigateur (Web Serial)

Après un premier `pio run`, les images sont générées dans `web/firmware/` :

```bash
cd web && python3 -m http.server 8000
```

Ouvre `http://localhost:8000` dans Chrome ou Edge : bouton **Installer** (ESP Web Tools) + **console série** + formulaire Wi-Fi. Tu peux aussi publier le dossier `web/` sur GitHub Pages (https suffit pour Web Serial). Le CYD doit être branché en USB sur l'ordinateur qui ouvre la page.

### Premier démarrage

1. Sans Wi-Fi configuré, le CYD crée le réseau **CYD-Deck-Setup** (mot de passe `cyd12345`). Connecte-toi, ouvre `http://192.168.4.1`.
2. Identifiant `admin`, mot de passe `cyd1234` (à changer dans la page).
3. Section *Maintenance* → entre ton Wi-Fi → le CYD redémarre et rejoint ton réseau (ou utilise la console Web Serial : `wifi MonReseau|MonMotDePasse`).
4. Sur le PC : Bluetooth → ajouter un appareil → **CYD Deck**.
5. L'adresse s'affiche aussi sur le CYD (roue crantée) ; `http://cyd-deck.local` fonctionne en général.

### Mises à jour sans câble

- **Depuis la page web** : section *Maintenance* → choisis `.pio/build/<env>/firmware.bin` (ou `web/firmware/<env>-ota.bin`) → Envoyer. Ne pas envoyer le fichier `<env>.bin` complet (réservé au flash USB).
- **Depuis PlatformIO** : `pio run -e ota -t upload` (mot de passe à régler dans `platformio.ini` si tu l'as changé).

## Format de la config

```json
{"settings":{"invert":false,"brightness":200,"azerty":true},
 "pages":[{"name":"Media","buttons":[
   {"i":0,"label":"Play/Pause","color":"#2E8B57","action":"media","value":"playpause"},
   {"i":1,"label":"Mute Micro","color":"#C0392B","action":"key","value":"ctrl+shift+m"}]}]}
```

`i` = position 0..8 (ligne par ligne). `action` : `key`, `media` (`playpause`, `prev`, `next`, `stop`, `mute`, `volup`, `voldown`) ou `text`.

## Points à connaître

- **AZERTY** : le clavier BLE envoie des positions de touches, pas des lettres. Avec le réglage AZERTY, `ctrl+z` est bien « Ctrl + la touche Z de ton clavier belge ». Le mode « texte » en AZERTY gère les lettres, chiffres, espace et `, ; : = . ? / + - ! ( ) ' " &` ; les autres caractères (`@`, `#`, accents…) sont ignorés. Ces positions sont celles du clavier belge : dis-moi si un caractère sort faux.
- **Accents sur l'écran** : les polices de LVGL n'ont que l'ASCII, les accents sont remplacés par la lettre de base (`Précédent` s'affiche `Precedent`). La page web accepte les accents normalement.
- **Couleurs inversées / rouge-bleu échangés** : interrupteur *Inverser les couleurs* sur le CYD. Pour rouge/bleu inversés, décommente `TFT_RGB_ORDER` dans `platformio.ini`.
- **Tactile décalé ou inversé** : ajuste `TOUCH_*` dans `include/config.h`.
- **Sécurité** : toute personne sur ton Wi-Fi qui connaît le mot de passe web peut faire taper des touches sur ton PC. Change le mot de passe par défaut.
- Si le Wi-Fi est injoignable au démarrage (15 s), le CYD bascule en point d'accès de configuration ; redémarre-le pour qu'il réessaie ton réseau.

## Structure

```
src/keymap.*   raccourcis -> codes HID (logique pure)
src/keys.*     clavier BLE (NimBLE)
src/store.*    config JSON, LittleFS + SD, NVS
src/ui.*       LVGL, tactile XPT2046 (bit-bang), écran
src/net.*      Wi-Fi, serveur web, OTA, commandes série
src/webui.h    page de configuration embarquée
web/           flasheur Web Serial + console
tools/         script qui génère web/firmware/*.bin
```
