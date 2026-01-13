/**
 * @file st7789.c
 * @brief ST7789 TFT Display Driver for nRF52840
 * 
 * Uses shared agsys_spi driver for DMA-based SPI transfers.
 */

#include "st7789.h"
#include "agsys_spi.h"
#include "board_config.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"

/* SPI handle for display */
static agsys_spi_handle_t m_spi_handle = AGSYS_SPI_INVALID_HANDLE;

/* Current rotation */
static uint8_t m_rotation = 0;
static uint16_t m_width = ST7789_WIDTH;
static uint16_t m_height = ST7789_HEIGHT;

/* Send command to display (uses low-level access for DC pin control) */
static void st7789_write_cmd(uint8_t cmd)
{
    agsys_spi_acquire(100);
    
    nrf_gpio_pin_clear(DISPLAY_DC_PIN);  /* Command mode */
    agsys_spi_cs_assert(m_spi_handle);
    
    agsys_spi_xfer_t xfer = { .tx_buf = &cmd, .rx_buf = NULL, .length = 1 };
    agsys_spi_transfer_raw(m_spi_handle, &xfer);
    
    agsys_spi_cs_deassert(m_spi_handle);
    agsys_spi_release();
}

/* Send data to display */
static void st7789_write_data(const uint8_t *data, uint32_t len)
{
    if (len == 0) return;
    
    agsys_spi_acquire(100);
    
    nrf_gpio_pin_set(DISPLAY_DC_PIN);  /* Data mode */
    agsys_spi_cs_assert(m_spi_handle);
    
    /* SPI transfer in chunks (max 255 bytes per DMA transfer) */
    while (len > 0) {
        uint32_t chunk = (len > 255) ? 255 : len;
        agsys_spi_xfer_t xfer = { .tx_buf = data, .rx_buf = NULL, .length = chunk };
        agsys_spi_transfer_raw(m_spi_handle, &xfer);
        data += chunk;
        len -= chunk;
    }
    
    agsys_spi_cs_deassert(m_spi_handle);
    agsys_spi_release();
}

/* Send single byte data */
static void st7789_write_data_byte(uint8_t data)
{
    st7789_write_data(&data, 1);
}

bool st7789_init(void)
{
    /* Configure GPIO pins */
    nrf_gpio_cfg_output(DISPLAY_DC_PIN);
    nrf_gpio_cfg_output(DISPLAY_RESET_PIN);
    nrf_gpio_cfg_output(DISPLAY_BACKLIGHT_PIN);
    nrf_gpio_pin_clear(DISPLAY_BACKLIGHT_PIN);
    
    /* Register with SPI manager */
    agsys_spi_config_t spi_config = {
        .cs_pin = SPI_CS_DISPLAY_PIN,
        .cs_active_low = true,
        .frequency = NRF_SPIM_FREQ_8M,
        .mode = 0,
    };
    
    if (agsys_spi_register(&spi_config, &m_spi_handle) != AGSYS_OK) {
        return false;
    }
    
    /* Hardware reset */
    nrf_gpio_pin_set(DISPLAY_RESET_PIN);
    nrf_delay_ms(10);
    nrf_gpio_pin_clear(DISPLAY_RESET_PIN);
    nrf_delay_ms(10);
    nrf_gpio_pin_set(DISPLAY_RESET_PIN);
    nrf_delay_ms(120);
    
    /* Software reset */
    st7789_write_cmd(ST7789_SWRESET);
    nrf_delay_ms(150);
    
    /* Exit sleep mode */
    st7789_write_cmd(ST7789_SLPOUT);
    nrf_delay_ms(120);
    
    /* Set color mode to 16-bit RGB565 */
    st7789_write_cmd(ST7789_COLMOD);
    st7789_write_data_byte(0x55);
    nrf_delay_ms(10);
    
    /* Memory access control (rotation) */
    st7789_write_cmd(ST7789_MADCTL);
    st7789_write_data_byte(ST7789_MADCTL_RGB);
    
    /* Column address set */
    st7789_write_cmd(ST7789_CASET);
    uint8_t caset[] = {0x00, 0x00, 0x00, 0xEF};  /* 0-239 */
    st7789_write_data(caset, 4);
    
    /* Row address set */
    st7789_write_cmd(ST7789_RASET);
    uint8_t raset[] = {0x00, 0x00, 0x01, 0x3F};  /* 0-319 */
    st7789_write_data(raset, 4);
    
    /* Inversion on (required for some ST7789 panels) */
    st7789_write_cmd(ST7789_INVON);
    nrf_delay_ms(10);
    
    /* Normal display mode */
    st7789_write_cmd(ST7789_NORON);
    nrf_delay_ms(10);
    
    /* Display on */
    st7789_write_cmd(ST7789_DISPON);
    nrf_delay_ms(10);
    
    /* Turn on backlight */
    nrf_gpio_pin_set(DISPLAY_BACKLIGHT_PIN);
    
    return true;
}

void st7789_set_rotation(uint8_t rotation)
{
    m_rotation = rotation % 4;
    
    uint8_t madctl = ST7789_MADCTL_RGB;
    
    switch (m_rotation) {
        case 0:  /* Portrait */
            madctl |= 0;
            m_width = ST7789_WIDTH;
            m_height = ST7789_HEIGHT;
            break;
        case 1:  /* Landscape */
            madctl |= ST7789_MADCTL_MX | ST7789_MADCTL_MV;
            m_width = ST7789_HEIGHT;
            m_height = ST7789_WIDTH;
            break;
        case 2:  /* Portrait inverted */
            madctl |= ST7789_MADCTL_MX | ST7789_MADCTL_MY;
            m_width = ST7789_WIDTH;
            m_height = ST7789_HEIGHT;
            break;
        case 3:  /* Landscape inverted */
            madctl |= ST7789_MADCTL_MY | ST7789_MADCTL_MV;
            m_width = ST7789_HEIGHT;
            m_height = ST7789_WIDTH;
            break;
    }
    
    st7789_write_cmd(ST7789_MADCTL);
    st7789_write_data_byte(madctl);
}

void st7789_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    /* Column address set */
    st7789_write_cmd(ST7789_CASET);
    uint8_t caset[] = {
        (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
        (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)
    };
    st7789_write_data(caset, 4);
    
    /* Row address set */
    st7789_write_cmd(ST7789_RASET);
    uint8_t raset[] = {
        (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
        (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)
    };
    st7789_write_data(raset, 4);
    
    /* Write to RAM */
    st7789_write_cmd(ST7789_RAMWR);
}

void st7789_write_pixels(const uint16_t *data, uint32_t len)
{
    if (len == 0) return;
    
    agsys_spi_acquire(100);
    
    nrf_gpio_pin_set(DISPLAY_DC_PIN);  /* Data mode */
    agsys_spi_cs_assert(m_spi_handle);
    
    /* Send pixel data in chunks for better performance
     * LVGL provides RGB565 in native byte order, ST7789 expects big-endian
     * We need to byte-swap each pixel
     */
    #define PIXEL_CHUNK_SIZE 128
    static uint8_t swap_buf[PIXEL_CHUNK_SIZE * 2];
    
    while (len > 0) {
        uint32_t chunk = (len > PIXEL_CHUNK_SIZE) ? PIXEL_CHUNK_SIZE : len;
        
        /* Byte-swap pixels into buffer */
        for (uint32_t i = 0; i < chunk; i++) {
            swap_buf[i * 2] = (uint8_t)(data[i] >> 8);
            swap_buf[i * 2 + 1] = (uint8_t)(data[i] & 0xFF);
        }
        
        /* Send chunk via DMA */
        agsys_spi_xfer_t xfer = { .tx_buf = swap_buf, .rx_buf = NULL, .length = chunk * 2 };
        agsys_spi_transfer_raw(m_spi_handle, &xfer);
        
        data += chunk;
        len -= chunk;
    }
    
    agsys_spi_cs_deassert(m_spi_handle);
    agsys_spi_release();
}

void st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= m_width || y >= m_height) return;
    if (x + w > m_width) w = m_width - x;
    if (y + h > m_height) h = m_height - y;
    
    st7789_set_addr_window(x, y, x + w - 1, y + h - 1);
    
    /* Prepare fill buffer with repeated color (big-endian) */
    #define FILL_CHUNK_SIZE 128
    static uint8_t fill_buf[FILL_CHUNK_SIZE * 2];
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    
    for (int i = 0; i < FILL_CHUNK_SIZE; i++) {
        fill_buf[i * 2] = hi;
        fill_buf[i * 2 + 1] = lo;
    }
    
    agsys_spi_acquire(100);
    
    nrf_gpio_pin_set(DISPLAY_DC_PIN);  /* Data mode */
    agsys_spi_cs_assert(m_spi_handle);
    
    uint32_t total = (uint32_t)w * h;
    while (total > 0) {
        uint32_t chunk = (total > FILL_CHUNK_SIZE) ? FILL_CHUNK_SIZE : total;
        agsys_spi_xfer_t xfer = { .tx_buf = fill_buf, .rx_buf = NULL, .length = chunk * 2 };
        agsys_spi_transfer_raw(m_spi_handle, &xfer);
        total -= chunk;
    }
    
    agsys_spi_cs_deassert(m_spi_handle);
    agsys_spi_release();
}

void st7789_fill_screen(uint16_t color)
{
    st7789_fill_rect(0, 0, m_width, m_height, color);
}

void st7789_display_on(void)
{
    st7789_write_cmd(ST7789_DISPON);
}

void st7789_display_off(void)
{
    st7789_write_cmd(ST7789_DISPOFF);
}

void st7789_set_backlight(uint8_t percent)
{
    /* Simple on/off for now - PWM can be added later */
    if (percent > 0) {
        nrf_gpio_pin_set(DISPLAY_BACKLIGHT_PIN);
    } else {
        nrf_gpio_pin_clear(DISPLAY_BACKLIGHT_PIN);
    }
}

void st7789_sleep(void)
{
    st7789_write_cmd(ST7789_DISPOFF);
    nrf_delay_ms(10);
    st7789_write_cmd(ST7789_SLPIN);
    nrf_delay_ms(120);
}

void st7789_wake(void)
{
    st7789_write_cmd(ST7789_SLPOUT);
    nrf_delay_ms(120);
    st7789_write_cmd(ST7789_DISPON);
    nrf_delay_ms(10);
}
