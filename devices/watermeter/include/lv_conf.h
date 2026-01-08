/**
 * @file lv_conf.h
 * @brief LVGL configuration for Mag Meter
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1

/*====================
   MEMORY SETTINGS
 *====================*/
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (48U * 1024U)  // 48KB for LVGL

/*====================
   HAL SETTINGS
 *====================*/
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/*====================
   FEATURE CONFIGURATION
 *====================*/
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ 0

/*====================
   FONT USAGE
 *====================*/
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1

#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*====================
   WIDGET USAGE
 *====================*/
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BTN 1
#define LV_USE_BTNMATRIX 1
#define LV_USE_CANVAS 0
#define LV_USE_CHECKBOX 1
#define LV_USE_DROPDOWN 1
#define LV_USE_IMG 1
#define LV_USE_LABEL 1
#define LV_USE_LINE 1
#define LV_USE_ROLLER 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_TEXTAREA 1
#define LV_USE_TABLE 1

/*====================
   EXTRA COMPONENTS
 *====================*/
#define LV_USE_CHART 1
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LED 1
#define LV_USE_LIST 1
#define LV_USE_MENU 0
#define LV_USE_METER 1
#define LV_USE_MSGBOX 1
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 1
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

/*====================
   THEMES
 *====================*/
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

/*====================
   LAYOUTS
 *====================*/
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

#endif // LV_CONF_H
