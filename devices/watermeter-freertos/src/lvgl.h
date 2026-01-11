/**
 * @file lvgl.h
 * @brief LVGL stub header for compilation
 * 
 * This is a placeholder. For actual builds, replace with real LVGL library.
 * Download LVGL from: https://github.com/lvgl/lvgl
 * 
 * To integrate LVGL:
 * 1. Clone LVGL v8.3.x to external/lvgl/
 * 2. Copy lv_conf.h to external/lvgl/
 * 3. Add LVGL source files to Makefile
 * 4. Remove this stub header
 */

#ifndef LVGL_H
#define LVGL_H

#ifdef LVGL_STUB_ONLY

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Basic types */
typedef uint32_t lv_color_t;
typedef int16_t lv_coord_t;
typedef uint8_t lv_opa_t;

#define LV_OPA_TRANSP 0

/* Object type */
typedef struct _lv_obj_t lv_obj_t;

/* Display types */
typedef struct {
    uint16_t x1, y1, x2, y2;
} lv_area_t;

typedef struct _lv_disp_draw_buf_t {
    void *buf1;
    void *buf2;
    uint32_t size;
} lv_disp_draw_buf_t;

typedef struct _lv_disp_drv_t {
    uint16_t hor_res;
    uint16_t ver_res;
    void (*flush_cb)(struct _lv_disp_drv_t *, const lv_area_t *, lv_color_t *);
    lv_disp_draw_buf_t *draw_buf;
} lv_disp_drv_t;

/* Stub functions */
static inline lv_color_t lv_color_hex(uint32_t c) { return c; }
static inline void lv_init(void) {}
static inline void lv_tick_inc(uint32_t ms) { (void)ms; }
static inline void lv_timer_handler(void) {}
static inline void lv_disp_draw_buf_init(lv_disp_draw_buf_t *buf, void *b1, void *b2, uint32_t size) { (void)buf; (void)b1; (void)b2; (void)size; }
static inline void lv_disp_drv_init(lv_disp_drv_t *drv) { (void)drv; }
static inline void lv_disp_drv_register(lv_disp_drv_t *drv) { (void)drv; }
static inline void lv_disp_flush_ready(lv_disp_drv_t *drv) { (void)drv; }
static inline lv_obj_t *lv_obj_create(lv_obj_t *parent) { (void)parent; return (lv_obj_t*)1; }
static inline lv_obj_t *lv_label_create(lv_obj_t *parent) { (void)parent; return (lv_obj_t*)1; }
static inline lv_obj_t *lv_bar_create(lv_obj_t *parent) { (void)parent; return (lv_obj_t*)1; }
static inline void lv_label_set_text(lv_obj_t *obj, const char *txt) { (void)obj; (void)txt; }
static inline void lv_bar_set_value(lv_obj_t *obj, int32_t val, int anim) { (void)obj; (void)val; (void)anim; }
static inline void lv_bar_set_range(lv_obj_t *obj, int32_t min, int32_t max) { (void)obj; (void)min; (void)max; }
static inline void lv_obj_set_size(lv_obj_t *obj, lv_coord_t w, lv_coord_t h) { (void)obj; (void)w; (void)h; }
static inline void lv_obj_align(lv_obj_t *obj, int align, lv_coord_t x, lv_coord_t y) { (void)obj; (void)align; (void)x; (void)y; }
static inline void lv_obj_align_to(lv_obj_t *obj, lv_obj_t *base, int align, lv_coord_t x, lv_coord_t y) { (void)obj; (void)base; (void)align; (void)x; (void)y; }
static inline void lv_obj_set_style_bg_color(lv_obj_t *obj, lv_color_t c, int sel) { (void)obj; (void)c; (void)sel; }
static inline void lv_obj_set_style_bg_opa(lv_obj_t *obj, lv_opa_t o, int sel) { (void)obj; (void)o; (void)sel; }
static inline void lv_obj_set_style_text_color(lv_obj_t *obj, lv_color_t c, int sel) { (void)obj; (void)c; (void)sel; }
static inline void lv_obj_set_style_text_font(lv_obj_t *obj, const void *f, int sel) { (void)obj; (void)f; (void)sel; }
static inline void lv_obj_set_style_border_width(lv_obj_t *obj, lv_coord_t w, int sel) { (void)obj; (void)w; (void)sel; }
static inline void lv_obj_set_style_border_color(lv_obj_t *obj, lv_color_t c, int sel) { (void)obj; (void)c; (void)sel; }
static inline void lv_obj_set_style_radius(lv_obj_t *obj, lv_coord_t r, int sel) { (void)obj; (void)r; (void)sel; }
static inline void lv_obj_set_style_pad_all(lv_obj_t *obj, lv_coord_t p, int sel) { (void)obj; (void)p; (void)sel; }
static inline void lv_obj_set_style_pad_left(lv_obj_t *obj, lv_coord_t p, int sel) { (void)obj; (void)p; (void)sel; }
static inline void lv_obj_set_style_pad_right(lv_obj_t *obj, lv_coord_t p, int sel) { (void)obj; (void)p; (void)sel; }
static inline void lv_obj_clear_flag(lv_obj_t *obj, uint32_t f) { (void)obj; (void)f; }
static inline void lv_obj_add_flag(lv_obj_t *obj, uint32_t f) { (void)obj; (void)f; }
static inline void lv_scr_load(lv_obj_t *scr) { (void)scr; }
static inline void lv_obj_clean(lv_obj_t *obj) { (void)obj; }
static inline void lv_obj_del(lv_obj_t *obj) { (void)obj; }
static inline void lv_obj_center(lv_obj_t *obj) { (void)obj; }
static inline lv_obj_t *lv_obj_get_parent(lv_obj_t *obj) { (void)obj; return (lv_obj_t*)1; }
static inline void lv_obj_set_flex_flow(lv_obj_t *obj, int flow) { (void)obj; (void)flow; }
static inline void lv_obj_set_flex_align(lv_obj_t *obj, int m, int c, int t) { (void)obj; (void)m; (void)c; (void)t; }

/* Flex flow constants */
#define LV_FLEX_FLOW_ROW 0
#define LV_FLEX_FLOW_COLUMN 1
#define LV_FLEX_ALIGN_START 0
#define LV_FLEX_ALIGN_CENTER 1

/* Alignment constants */
#define LV_ALIGN_CENTER 0
#define LV_ALIGN_TOP_MID 1
#define LV_ALIGN_TOP_LEFT 2
#define LV_ALIGN_TOP_RIGHT 3
#define LV_ALIGN_BOTTOM_MID 4
#define LV_ALIGN_LEFT_MID 5
#define LV_ALIGN_RIGHT_MID 6
#define LV_ALIGN_OUT_RIGHT_BOTTOM 7

/* Part constants */
#define LV_PART_MAIN 0
#define LV_PART_INDICATOR 1

/* Flag constants */
#define LV_OBJ_FLAG_SCROLLABLE 0x01
#define LV_OBJ_FLAG_HIDDEN 0x02

/* Animation constants */
#define LV_ANIM_OFF 0
#define LV_ANIM_ON 1

/* Symbols - single character to avoid string concat issues */
#define LV_SYMBOL_RIGHT ">"
#define LV_SYMBOL_LEFT "<"
#define LV_SYMBOL_UP "^"
#define LV_SYMBOL_DOWN "v"
#define LV_SYMBOL_WARNING "!"
#define LV_SYMBOL_REFRESH "R"
#define LV_SYMBOL_OK "OK"

/* Fonts (stubs) */
extern const void lv_font_montserrat_12;
extern const void lv_font_montserrat_14;
extern const void lv_font_montserrat_16;
extern const void lv_font_montserrat_20;
extern const void lv_font_montserrat_28;

#endif /* LVGL_STUB_ONLY */

#endif /* LVGL_H */
