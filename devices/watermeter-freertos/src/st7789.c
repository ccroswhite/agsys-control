/**
 * @file st7789.c
 * @brief ST7789 TFT Display Driver for nRF52840
 */

#include "st7789.h"
#include "board_config.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrfx_spim.h"

/* SPI instance for display */
static const nrfx_spim_t m_spi = NRFX_SPIM_INSTANCE(1);
static volatile bool m_spi_xfer_done = true;

/* Current rotation */
static uint8_t m_rotation = 0;
static uint16_t m_width = ST7789_WIDTH;
static uint16_t m_height = ST7789_HEIGHT;

/* SPI event handler */
static void spi_event_handler(nrfx_spim_evt_t const *p_event, void *p_context)
{
    m_spi_xfer_done = true;
}

/* Wait for SPI transfer to complete */
static void spi_wait(void)
{
    while (!m_spi_xfer_done) {
        __WFE();
    }
}

/* Send command to display */
static void st7789_write_cmd(uint8_t cmd)
{
    nrf_gpio_pin_clear(DISPLAY_DC_PIN);  /* Command mode */
    nrf_gpio_pin_clear(SPI_CS_DISPLAY_PIN);
    
    nrfx_spim_xfer_desc_t xfer = NRFX_SPIM_XFER_TX(&cmd, 1);
    m_spi_xfer_done = false;
    nrfx_spim_xfer(&m_spi, &xfer, 0);
    spi_wait();
    
    nrf_gpio_pin_set(SPI_CS_DISPLAY_PIN);
}

/* Send data to display */
static void st7789_write_data(const uint8_t *data, uint32_t len)
{
    if (len == 0) return;
    
    nrf_gpio_pin_set(DISPLAY_DC_PIN);  /* Data mode */
    nrf_gpio_pin_clear(SPI_CS_DISPLAY_PIN);
    
    /* SPI transfer in chunks (max 255 bytes per transfer) */
    while (len > 0) {
        uint32_t chunk = (len > 255) ? 255 : len;
        nrfx_spim_xfer_desc_t xfer = NRFX_SPIM_XFER_TX(data, chunk);
        m_spi_xfer_done = false;
        nrfx_spim_xfer(&m_spi, &xfer, 0);
        spi_wait();
        data += chunk;
        len -= chunk;
    }
    
    nrf_gpio_pin_set(SPI_CS_DISPLAY_PIN);
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
    nrf_gpio_cfg_output(SPI_CS_DISPLAY_PIN);
    
    nrf_gpio_pin_set(SPI_CS_DISPLAY_PIN);
    nrf_gpio_pin_clear(DISPLAY_BACKLIGHT_PIN);
    
    /* Configure SPI */
    nrfx_spim_config_t spi_config = NRFX_SPIM_DEFAULT_CONFIG;
    spi_config.sck_pin = SPI1_SCK_PIN;
    spi_config.mosi_pin = SPI1_MOSI_PIN;
    spi_config.miso_pin = NRFX_SPIM_PIN_NOT_USED;
    spi_config.frequency = NRF_SPIM_FREQ_8M;
    spi_config.mode = NRF_SPIM_MODE_0;
    spi_config.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST;
    
    nrfx_err_t err = nrfx_spim_init(&m_spi, &spi_config, spi_event_handler, NULL);
    if (err != NRFX_SUCCESS) {
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
    /* Convert to bytes and send */
    nrf_gpio_pin_set(DISPLAY_DC_PIN);
    nrf_gpio_pin_clear(SPI_CS_DISPLAY_PIN);
    
    /* Send pixel data (2 bytes per pixel, big-endian) */
    for (uint32_t i = 0; i < len; i++) {
        uint8_t buf[2] = {(uint8_t)(data[i] >> 8), (uint8_t)(data[i] & 0xFF)};
        nrfx_spim_xfer_desc_t xfer = NRFX_SPIM_XFER_TX(buf, 2);
        m_spi_xfer_done = false;
        nrfx_spim_xfer(&m_spi, &xfer, 0);
        spi_wait();
    }
    
    nrf_gpio_pin_set(SPI_CS_DISPLAY_PIN);
}

void st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= m_width || y >= m_height) return;
    if (x + w > m_width) w = m_width - x;
    if (y + h > m_height) h = m_height - y;
    
    st7789_set_addr_window(x, y, x + w - 1, y + h - 1);
    
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    uint8_t buf[2] = {hi, lo};
    
    nrf_gpio_pin_set(DISPLAY_DC_PIN);
    nrf_gpio_pin_clear(SPI_CS_DISPLAY_PIN);
    
    uint32_t total = (uint32_t)w * h;
    for (uint32_t i = 0; i < total; i++) {
        nrfx_spim_xfer_desc_t xfer = NRFX_SPIM_XFER_TX(buf, 2);
        m_spi_xfer_done = false;
        nrfx_spim_xfer(&m_spi, &xfer, 0);
        spi_wait();
    }
    
    nrf_gpio_pin_set(SPI_CS_DISPLAY_PIN);
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
