/**
 * @file st7789.h
 * @brief ST7789 TFT Display Driver for nRF52840
 * 
 * Hardware SPI driver for 240x320 ST7789 display.
 */

#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>
#include <stdbool.h>

/* Display dimensions */
#define ST7789_WIDTH    240
#define ST7789_HEIGHT   320

/* ST7789 Commands */
#define ST7789_NOP      0x00
#define ST7789_SWRESET  0x01
#define ST7789_SLPIN    0x10
#define ST7789_SLPOUT   0x11
#define ST7789_PTLON    0x12
#define ST7789_NORON    0x13
#define ST7789_INVOFF   0x20
#define ST7789_INVON    0x21
#define ST7789_DISPOFF  0x28
#define ST7789_DISPON   0x29
#define ST7789_CASET    0x2A
#define ST7789_RASET    0x2B
#define ST7789_RAMWR    0x2C
#define ST7789_RAMRD    0x2E
#define ST7789_PTLAR    0x30
#define ST7789_COLMOD   0x3A
#define ST7789_MADCTL   0x36
#define ST7789_FRMCTR1  0xB1
#define ST7789_FRMCTR2  0xB2
#define ST7789_FRMCTR3  0xB3
#define ST7789_INVCTR   0xB4
#define ST7789_PWCTR1   0xC0
#define ST7789_PWCTR2   0xC1
#define ST7789_PWCTR3   0xC2
#define ST7789_PWCTR4   0xC3
#define ST7789_PWCTR5   0xC4
#define ST7789_VMCTR1   0xC5
#define ST7789_GMCTRP1  0xE0
#define ST7789_GMCTRN1  0xE1

/* MADCTL bits */
#define ST7789_MADCTL_MY    0x80
#define ST7789_MADCTL_MX    0x40
#define ST7789_MADCTL_MV    0x20
#define ST7789_MADCTL_ML    0x10
#define ST7789_MADCTL_RGB   0x00
#define ST7789_MADCTL_BGR   0x08

/* Color definitions (RGB565) */
#define ST7789_BLACK    0x0000
#define ST7789_WHITE    0xFFFF
#define ST7789_RED      0xF800
#define ST7789_GREEN    0x07E0
#define ST7789_BLUE     0x001F
#define ST7789_YELLOW   0xFFE0
#define ST7789_CYAN     0x07FF
#define ST7789_MAGENTA  0xF81F

/**
 * @brief Initialize ST7789 display
 * @return true on success
 */
bool st7789_init(void);

/**
 * @brief Set display rotation
 * @param rotation 0-3 (0=portrait, 1=landscape, 2=portrait inverted, 3=landscape inverted)
 */
void st7789_set_rotation(uint8_t rotation);

/**
 * @brief Set address window for pixel writes
 * @param x0 Start X
 * @param y0 Start Y
 * @param x1 End X
 * @param y1 End Y
 */
void st7789_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief Write pixel data to display
 * @param data Pointer to RGB565 pixel data
 * @param len Number of pixels
 */
void st7789_write_pixels(const uint16_t *data, uint32_t len);

/**
 * @brief Fill rectangle with color
 * @param x Start X
 * @param y Start Y
 * @param w Width
 * @param h Height
 * @param color RGB565 color
 */
void st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief Fill entire screen with color
 * @param color RGB565 color
 */
void st7789_fill_screen(uint16_t color);

/**
 * @brief Turn display on
 */
void st7789_display_on(void);

/**
 * @brief Turn display off
 */
void st7789_display_off(void);

/**
 * @brief Set backlight brightness
 * @param percent 0-100
 */
void st7789_set_backlight(uint8_t percent);

/**
 * @brief Enter sleep mode
 */
void st7789_sleep(void);

/**
 * @brief Exit sleep mode
 */
void st7789_wake(void);

#endif /* ST7789_H */
