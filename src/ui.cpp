#include "ui.h"
#include "config.h"
#include "store.h"
#include "keys.h"
#include "net.h"
#include <lvgl.h>
#include <TFT_eSPI.h>

static TFT_eSPI tft;

// Si le pilote d'ecran est configure avec TFT_INVERSION_ON (ST7789), l'etat "normal" est deja inverse.
#ifdef TFT_INVERSION_ON
static const bool NATIVE_INVERT = true;
#else
static const bool NATIVE_INVERT = false;
#endif

// ------------------------------------------------------------------ tactile XPT2046 (bit-bang)
// Le tactile du CYD est sur d'autres broches que l'ecran, et la carte SD occupe le bus VSPI :
// un petit bit-bang evite tout conflit de bus.

static uint16_t xptXfer(uint8_t cmd) {
  digitalWrite(PIN_TOUCH_CS, LOW);
  for (int8_t i = 7; i >= 0; i--) {
    digitalWrite(PIN_TOUCH_MOSI, (cmd >> i) & 1);
    digitalWrite(PIN_TOUCH_CLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(PIN_TOUCH_CLK, LOW);
    delayMicroseconds(1);
  }
  uint16_t v = 0;
  for (uint8_t i = 0; i < 16; i++) {
    digitalWrite(PIN_TOUCH_CLK, HIGH);
    delayMicroseconds(1);
    v = (v << 1) | (digitalRead(PIN_TOUCH_MISO) & 1);
    digitalWrite(PIN_TOUCH_CLK, LOW);
    delayMicroseconds(1);
  }
  digitalWrite(PIN_TOUCH_CS, HIGH);
  return (v >> 3) & 0x0FFF;
}

static bool touchPoint(int16_t& sx, int16_t& sy) {
  int32_t z = (int32_t)xptXfer(0xB1) + 4095 - (int32_t)xptXfer(0xC1);   // pression
  if (z < 400) return false;
  int32_t rx = 0, ry = 0;
  for (int i = 0; i < 3; i++) { rx += xptXfer(0xD1); ry += xptXfer(0x91); }
  rx /= 3; ry /= 3;
#if TOUCH_SWAP_XY
  int32_t t = rx; rx = ry; ry = t;
#endif
  sx = (int16_t)constrain(map(rx, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W - 1), 0, SCREEN_W - 1);
  sy = (int16_t)constrain(map(ry, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H - 1), 0, SCREEN_H - 1);
#if TOUCH_INVERT_X
  sx = SCREEN_W - 1 - sx;
#endif
#if TOUCH_INVERT_Y
  sy = SCREEN_H - 1 - sy;
#endif
  return true;
}

// ------------------------------------------------------------------ LVGL <-> materiel

static lv_disp_draw_buf_t drawBuf;
static lv_color_t bufA[SCREEN_W * 20];
static lv_disp_drv_t dispDrv;
static lv_indev_drv_t indevDrv;

static void flushCb(lv_disp_drv_t* drv, const lv_area_t* a, lv_color_t* c) {
  uint32_t w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
  tft.startWrite();
  tft.setAddrWindow(a->x1, a->y1, w, h);
  tft.pushColors((uint16_t*)&c->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(drv);
}

static void touchCb(lv_indev_drv_t*, lv_indev_data_t* d) {
  int16_t x, y;
  if (touchPoint(x, y)) {
    d->state = LV_INDEV_STATE_PR;
    d->point.x = x;
    d->point.y = y;
  } else {
    d->state = LV_INDEV_STATE_REL;
  }
}

void uiApplyDisplay() {
  tft.invertDisplay(deck.invert != NATIVE_INVERT);
  ledcWrite(0, deck.brightness);
}

// ------------------------------------------------------------------ utilitaires

// Les polices integrees de LVGL n'ont que l'ASCII : on remplace les accents par la lettre de base.
static String fold(const char* s) {
  static const char MAP[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
  String o;
  for (const uint8_t* p = (const uint8_t*)s; *p; ++p) {
    if (*p < 0x80) { o += (char)*p; continue; }
    if (*p == 0xC3 && p[1] >= 0x80 && p[1] <= 0xBF) { o += MAP[p[1] - 0x80]; p++; continue; }
    o += '?';
    while ((p[1] & 0xC0) == 0x80) p++;
  }
  return o;
}

static lv_color_t textOn(uint32_t rgb) {
  uint8_t r = rgb >> 16, g = rgb >> 8, b = rgb;
  return ((r * 299 + g * 587 + b * 114) / 1000 > 150) ? lv_color_black() : lv_color_white();
}

// ------------------------------------------------------------------ toast

static lv_obj_t* toast = nullptr;
static lv_timer_t* toastTimer = nullptr;

static void toastDone(lv_timer_t*) {
  toastTimer = nullptr;               // LVGL supprime lui-meme ce timer (repeat_count = 1)
  if (toast) { lv_obj_del(toast); toast = nullptr; }
}

void uiToast(const char* msg, bool err) {
  if (toastTimer) { lv_timer_del(toastTimer); toastTimer = nullptr; }
  if (toast) { lv_obj_del(toast); toast = nullptr; }
  toast = lv_label_create(lv_layer_top());
  String m = fold(msg);
  lv_label_set_text(toast, m.c_str());
  lv_obj_set_style_bg_color(toast, err ? lv_color_hex(0xB03A2E) : lv_color_hex(0x239B56), 0);
  lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(toast, lv_color_white(), 0);
  lv_obj_set_style_pad_all(toast, 6, 0);
  lv_obj_set_style_radius(toast, 8, 0);
  lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -6);
  toastTimer = lv_timer_create(toastDone, 1400, nullptr);
  lv_timer_set_repeat_count(toastTimer, 1);
}

// ------------------------------------------------------------------ interface principale

static lv_obj_t *bar, *grid, *lblPage, *lblBle, *lblNet;
static lv_obj_t* panel = nullptr;
static int curPage = 0;

static void onButton(lv_event_t* e) {
  uintptr_t id = (uintptr_t)lv_event_get_user_data(e);
  int p = id >> 8, i = id & 0xFF;
  if (p >= deck.nPages) return;
  const Btn& b = deck.pages[p].btn[i];
  String msg;
  bool ok = keysRun(b.action, b.value, deck.azerty, msg);
  uiToast(ok ? fold(b.label).c_str() : msg.c_str(), !ok);
}

static void buildGrid() {
  lv_obj_clean(grid);
  if (curPage >= deck.nPages) curPage = 0;
  String name = fold(deck.pages[curPage].name);
  lv_label_set_text(lblPage, name.c_str());

  const int cw = SCREEN_W / GRID_COLS;
  const int ch = (SCREEN_H - BAR_H) / GRID_ROWS;
  for (int i = 0; i < GRID_SIZE; i++) {
    const Btn& b = deck.pages[curPage].btn[i];
    if (!b.used) continue;
    lv_obj_t* btn = lv_btn_create(grid);
    lv_obj_set_size(btn, cw - 6, ch - 6);
    lv_obj_set_pos(btn, (i % GRID_COLS) * cw + 3, (i / GRID_COLS) * ch + 3);
    lv_color_t c = lv_color_hex(b.color);
    lv_obj_set_style_bg_color(btn, c, 0);
    lv_obj_set_style_bg_color(btn, lv_color_darken(c, LV_OPA_40), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, onButton, LV_EVENT_CLICKED, (void*)(uintptr_t)((curPage << 8) | i));

    lv_obj_t* l = lv_label_create(btn);
    String t = fold(b.label);
    lv_label_set_text(l, t.c_str());
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, cw - 14);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(l, textOn(b.color), 0);
    lv_obj_center(l);
  }
}

static void setPage(int p) {
  if (deck.nPages == 0) return;
  curPage = (p + deck.nPages) % deck.nPages;
  buildGrid();
}

static void onPrev(lv_event_t*) { setPage(curPage - 1); }
static void onNext(lv_event_t*) { setPage(curPage + 1); }

static void onGesture(lv_event_t*) {
  lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (d == LV_DIR_LEFT) setPage(curPage + 1);
  else if (d == LV_DIR_RIGHT) setPage(curPage - 1);
}

static void statusTick(lv_timer_t*) {
  lv_obj_set_style_text_color(lblBle, lv_color_hex(keysConnected() ? 0x2ECC71 : 0x6E6E6E), 0);
  lv_obj_set_style_text_color(lblNet, lv_color_hex(netIsAp() ? 0xF39C12 : (netConnected() ? 0x2ECC71 : 0x6E6E6E)), 0);
}

// ------------------------------------------------------------------ panneau de reglages

static void closeSettings(lv_event_t*) {
  if (panel) { lv_obj_del(panel); panel = nullptr; }
}

static void onBright(lv_event_t* e) {
  lv_obj_t* s = lv_event_get_target(e);
  deck.brightness = (uint8_t)lv_slider_get_value(s);
  ledcWrite(0, deck.brightness);
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) storeSave();
}

static void onInvert(lv_event_t* e) {
  deck.invert = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  uiApplyDisplay();
  storeSave();
}

static void onAzerty(lv_event_t* e) {
  deck.azerty = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  storeSave();
}

static lv_obj_t* addRow(lv_obj_t* parent, const char* text) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_size(row, lv_pct(100), 32);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* l = lv_label_create(row);
  lv_label_set_text(l, text);
  return row;
}

static void openSettings(lv_event_t*) {
  if (panel) return;
  panel = lv_obj_create(lv_layer_top());
  lv_obj_set_size(panel, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(panel, 0, 0);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x101010), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(panel, 0, 0);
  lv_obj_set_style_border_width(panel, 0, 0);
  lv_obj_set_style_pad_all(panel, 6, 0);
  lv_obj_set_style_pad_row(panel, 2, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* title = lv_label_create(panel);
  lv_label_set_text(title, "Reglages");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

  lv_obj_t* r1 = addRow(panel, "Luminosite");
  lv_obj_t* sl = lv_slider_create(r1);
  lv_obj_set_width(sl, 150);
  lv_slider_set_range(sl, 10, 255);
  lv_slider_set_value(sl, deck.brightness, LV_ANIM_OFF);
  lv_obj_add_event_cb(sl, onBright, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(sl, onBright, LV_EVENT_RELEASED, nullptr);

  lv_obj_t* r2 = addRow(panel, "Inverser les couleurs");
  lv_obj_t* sw1 = lv_switch_create(r2);
  if (deck.invert) lv_obj_add_state(sw1, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw1, onInvert, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t* r3 = addRow(panel, "Clavier du PC en AZERTY");
  lv_obj_t* sw2 = lv_switch_create(r3);
  if (deck.azerty) lv_obj_add_state(sw2, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw2, onAzerty, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t* info = lv_label_create(panel);
  lv_obj_set_width(info, lv_pct(100));
  lv_obj_set_style_text_font(info, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(info, lv_color_hex(0xAAAAAA), 0);
  String s = netIsAp() ? String("Wi-Fi: ") + AP_SSID + " / " + AP_PASS + "  ->  " + netIp()
                       : String("Page web: http://") + netIp() + "  (" + DEVICE_HOSTNAME + ".local)";
  s += String("\nBluetooth: ") + BLE_NAME + (keysConnected() ? " (connecte)" : " (non connecte)");
  lv_label_set_text(info, s.c_str());

  lv_obj_t* close = lv_btn_create(panel);
  lv_obj_set_size(close, lv_pct(100), 30);
  lv_obj_add_event_cb(close, closeSettings, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* cl = lv_label_create(close);
  lv_label_set_text(cl, "Fermer");
  lv_obj_center(cl);
}

// ------------------------------------------------------------------ construction

static lv_obj_t* barButton(const char* symbol, lv_coord_t x, lv_event_cb_t cb) {
  lv_obj_t* b = lv_btn_create(bar);
  lv_obj_set_size(b, 36, 28);
  lv_obj_set_pos(b, x, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(0x333333), 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, symbol);
  lv_obj_center(l);
  return b;
}

static void buildUi() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(scr, onGesture, LV_EVENT_GESTURE, nullptr);

  bar = lv_obj_create(scr);
  lv_obj_set_size(bar, SCREEN_W, BAR_H);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x181818), 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 3, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  barButton(LV_SYMBOL_LEFT, 0, onPrev);
  lblPage = lv_label_create(bar);
  lv_obj_set_width(lblPage, 112);
  lv_label_set_long_mode(lblPage, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(lblPage, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(lblPage, 40, 5);
  barButton(LV_SYMBOL_RIGHT, 156, onNext);

  lblBle = lv_label_create(bar);
  lv_label_set_text(lblBle, LV_SYMBOL_BLUETOOTH);
  lv_obj_set_pos(lblBle, 208, 5);
  lblNet = lv_label_create(bar);
  lv_label_set_text(lblNet, LV_SYMBOL_WIFI);
  lv_obj_set_pos(lblNet, 236, 5);
  barButton(LV_SYMBOL_SETTINGS, 276, openSettings);

  grid = lv_obj_create(scr);
  lv_obj_set_size(grid, SCREEN_W, SCREEN_H - BAR_H);
  lv_obj_set_pos(grid, 0, BAR_H);
  lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

  lv_timer_create(statusTick, 500, nullptr);
  buildGrid();
}

void uiRebuild() {
  uiApplyDisplay();
  if (curPage >= deck.nPages) curPage = 0;
  buildGrid();
}

void uiBegin() {
  // Tactile
  pinMode(PIN_TOUCH_CS, OUTPUT);   digitalWrite(PIN_TOUCH_CS, HIGH);
  pinMode(PIN_TOUCH_CLK, OUTPUT);  digitalWrite(PIN_TOUCH_CLK, LOW);
  pinMode(PIN_TOUCH_MOSI, OUTPUT);
  pinMode(PIN_TOUCH_MISO, INPUT);

  // Ecran
  tft.init();
  tft.setRotation(1);               // paysage 320x240
  tft.fillScreen(TFT_BLACK);
  ledcSetup(0, 5000, 8);            // luminosite par PWM
  ledcAttachPin(PIN_BL, 0);
  uiApplyDisplay();

  // LVGL
  lv_init();
  lv_disp_draw_buf_init(&drawBuf, bufA, nullptr, SCREEN_W * 20);
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = SCREEN_W;
  dispDrv.ver_res = SCREEN_H;
  dispDrv.flush_cb = flushCb;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_t* disp = lv_disp_drv_register(&dispDrv);
  lv_theme_t* th = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE),
                                         lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
  lv_disp_set_theme(disp, th);

  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = touchCb;
  lv_indev_drv_register(&indevDrv);

  buildUi();
}

void uiRenderNow() { lv_timer_handler(); }

void uiLoop() {
  lv_timer_handler();
  delay(5);
}
