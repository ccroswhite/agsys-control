/**
 * @file User_Setup.h
 * @brief TFT_eSPI configuration for Mag Meter ST7789 display
 * 
 * This file configures the TFT_eSPI library for the E28GA-T-CW250-N
 * 2.8" transflective TFT display with ST7789 controller.
 */

#ifndef USER_SETUP_H
#define USER_SETUP_H

// Display driver
#define ST7789_DRIVER

// Display resolution
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// Color order (RGB or BGR)
#define TFT_RGB_ORDER TFT_RGB

// SPI pins for nRF52840
#define TFT_MOSI 18   // P0.18
#define TFT_SCLK 19   // P0.19
#define TFT_CS   17   // P0.17
#define TFT_DC   16   // P0.16
#define TFT_RST  15   // P0.15

// No MISO needed for display-only operation
#define TFT_MISO -1

// SPI frequency (ST7789 supports up to 80MHz, but nRF52840 SPI is limited)
#define SPI_FREQUENCY  32000000  // 32 MHz

// Use hardware SPI
#define USE_HSPI_PORT

// Font selection (using LVGL fonts instead)
#define LOAD_GLCD   0
#define LOAD_FONT2  0
#define LOAD_FONT4  0
#define LOAD_FONT6  0
#define LOAD_FONT7  0
#define LOAD_FONT8  0
#define LOAD_GFXFF  0

// Smooth fonts (disabled - using LVGL)
#define SMOOTH_FONT 0

#endif // USER_SETUP_H
