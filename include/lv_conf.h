/* Configuration LVGL 8.3 minimale : tout ce qui n'est pas defini ici prend la valeur par defaut. */
#if 1
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0          /* l'echange d'octets est fait par TFT_eSPI::pushColors(..., true) */

#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (40U * 1024U)

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_DISP_DEF_REFR_PERIOD 20
#define LV_INDEV_DEF_READ_PERIOD 20

#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 0
#define LV_USE_ASSERT_MALLOC 0

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_FLEX 1
#define LV_USE_GRID 0

#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_BTN 1
#define LV_USE_LABEL 1

#endif /*LV_CONF_H*/
#endif
