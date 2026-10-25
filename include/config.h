#pragma once

#define FW_VERSION      "1.0.0"
#define DEVICE_HOSTNAME "cyd-deck"        // -> http://cyd-deck.local
#define BLE_NAME        "CYD Deck"
#define DEFAULT_PASS    "cyd1234"         // mot de passe web/OTA par defaut (user: admin)
#define AP_SSID         "CYD-Deck-Setup"  // reseau cree si le Wi-Fi n'est pas configure
#define AP_PASS         "cyd12345"

// Grille de boutons : 3 colonnes x 3 lignes = 9 boutons par page
#define GRID_COLS  3
#define GRID_ROWS  3
#define GRID_SIZE  (GRID_COLS * GRID_ROWS)
#define MAX_PAGES  6

// Ecran (paysage)
#define SCREEN_W   320
#define SCREEN_H   240
#define BAR_H      34

// Pins CYD ESP32-2432S028R
#define PIN_BL       21
#define PIN_SD_CS    5
#define PIN_SD_SCK   18
#define PIN_SD_MISO  19
#define PIN_SD_MOSI  23
#define PIN_TOUCH_CLK  25
#define PIN_TOUCH_MOSI 32
#define PIN_TOUCH_MISO 39
#define PIN_TOUCH_CS   33

// Calibration du tactile XPT2046 (valeurs brutes -> pixels, orientation paysage).
// Si le toucher est decale ou inverse, ajuste ces valeurs.
#define TOUCH_X_MIN  200
#define TOUCH_X_MAX  3700
#define TOUCH_Y_MIN  240
#define TOUCH_Y_MAX  3800
#define TOUCH_SWAP_XY   0   // 1 si X et Y sont echanges
#define TOUCH_INVERT_X  0
#define TOUCH_INVERT_Y  0
