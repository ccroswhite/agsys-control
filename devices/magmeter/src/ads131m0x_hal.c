/**
 * @file ads131m0x_hal.c
 * @brief HAL implementation for ADS131M0x driver on nRF52840 magmeter
 * 
 * Bridges the platform-agnostic ADS131M0x driver with the magmeter's
 * agsys_spi driver and nRF GPIO/delay functions.
 */

/* ADS131M0X_DEVICE_M02 must be defined in Makefile CFLAGS */
#include "ads131m0x.h"
#include "agsys_spi.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_gpiote.h"
#include "SEGGER_RTT.h"

/* ==========================================================================
 * STATIC VARIABLES
 * ========================================================================== */

static agsys_spi_handle_t s_spi_handle = AGSYS_SPI_INVALID_HANDLE;
static ads131m0x_ctx_t *s_active_ctx = NULL;

/* Callback for DRDY interrupt */
static void (*s_drdy_callback)(ads131m0x_sample_t *sample, void *user_data) = NULL;
static void *s_drdy_user_data = NULL;

/* ==========================================================================
 * HAL FUNCTION IMPLEMENTATIONS
 * ========================================================================== */

/**
 * @brief SPI transfer function for ADS131M0x driver
 */
static bool hal_spi_transfer(const uint8_t *tx_buf, uint8_t *rx_buf, 
                              size_t len, void *user_data)
{
    (void)user_data;
    
    if (s_spi_handle == AGSYS_SPI_INVALID_HANDLE) {
        return false;
    }
    
    agsys_spi_xfer_t xfer = {
        .tx_buf = (uint8_t *)tx_buf,
        .rx_buf = rx_buf,
        .length = len,
    };
    
    return (agsys_spi_transfer(s_spi_handle, &xfer) == AGSYS_OK);
}

/**
 * @brief GPIO read function for ADS131M0x driver
 */
static bool hal_gpio_read(uint8_t pin, void *user_data)
{
    (void)user_data;
    return (nrf_gpio_pin_read(pin) != 0);
}

/**
 * @brief GPIO write function for ADS131M0x driver
 */
static void hal_gpio_write(uint8_t pin, bool value, void *user_data)
{
    (void)user_data;
    if (value) {
        nrf_gpio_pin_set(pin);
    } else {
        nrf_gpio_pin_clear(pin);
    }
}

/**
 * @brief Delay function for ADS131M0x driver
 */
static void hal_delay_ms(uint32_t ms, void *user_data)
{
    (void)user_data;
    nrf_delay_ms(ms);
}

/* ==========================================================================
 * DRDY INTERRUPT HANDLER
 * ========================================================================== */

static void drdy_interrupt_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    (void)pin;
    (void)action;
    
    if (s_active_ctx == NULL || s_drdy_callback == NULL) {
        return;
    }
    
    ads131m0x_sample_t sample;
    if (ads131m0x_read_sample(s_active_ctx, &sample)) {
        s_drdy_callback(&sample, s_drdy_user_data);
    }
}

/* ==========================================================================
 * PUBLIC API - MAGMETER-SPECIFIC WRAPPER
 * ========================================================================== */

/**
 * @brief Initialize ADS131M02 for magmeter application
 * 
 * @param ctx Device context to initialize
 * @param cs_pin Chip select pin
 * @param drdy_pin Data ready pin
 * @param sync_pin Sync/reset pin
 * @param osr Oversampling ratio
 * @param gain_ch0 Gain for channel 0 (electrode signal)
 * @param gain_ch1 Gain for channel 1 (coil current sense)
 * @return true on success
 */
bool ads131m0x_hal_init(ads131m0x_ctx_t *ctx,
                        uint8_t cs_pin,
                        uint8_t drdy_pin,
                        uint8_t sync_pin,
                        ads131m0x_osr_t osr,
                        ads131m0x_gain_t gain_ch0,
                        ads131m0x_gain_t gain_ch1)
{
    if (ctx == NULL) {
        return false;
    }
    
    /* Register with SPI manager on bus 0 (ADC bus) */
    agsys_spi_config_t spi_config = {
        .cs_pin = cs_pin,
        .cs_active_low = true,
        .frequency = NRF_SPIM_FREQ_4M,
        .mode = 1,  /* CPOL=0, CPHA=1 for ADS131M0x */
        .bus = AGSYS_SPI_BUS_0,
    };
    
    if (agsys_spi_register(&spi_config, &s_spi_handle) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "ADS131M0x: Failed to register SPI\n");
        return false;
    }
    
    /* Configure SYNC/RST pin as output */
    nrf_gpio_cfg_output(sync_pin);
    nrf_gpio_pin_set(sync_pin);
    
    /* Configure DRDY pin as input */
    nrf_gpio_cfg_input(drdy_pin, NRF_GPIO_PIN_NOPULL);
    
    /* Set up HAL interface */
    ads131m0x_hal_t hal = {
        .spi_transfer = hal_spi_transfer,
        .gpio_read = hal_gpio_read,
        .gpio_write = hal_gpio_write,
        .delay_ms = hal_delay_ms,
        .user_data = NULL,
    };
    
    /* Configure device */
    ads131m0x_config_t config = ADS131M0X_CONFIG_DEFAULT();
    config.sync_reset_pin = sync_pin;
    config.drdy_pin = drdy_pin;
    config.osr = osr;
    config.power_mode = ADS131M0X_PWR_HIGH_RES;
    config.gain[0] = gain_ch0;
    config.gain[1] = gain_ch1;
    
    /* Initialize driver */
    if (!ads131m0x_init(ctx, &hal, &config)) {
        SEGGER_RTT_printf(0, "ADS131M0x: Init failed (device ID mismatch?)\n");
        return false;
    }
    
    s_active_ctx = ctx;
    
    SEGGER_RTT_printf(0, "ADS131M0x: %s initialized, OSR=%d, Gain CH0=%d, CH1=%d\n",
                      ads131m0x_get_device_name(),
                      (int)osr,
                      ads131m0x_get_gain_multiplier(gain_ch0),
                      ads131m0x_get_gain_multiplier(gain_ch1));
    
    return true;
}

/**
 * @brief Set DRDY callback for interrupt-driven sampling
 */
void ads131m0x_hal_set_drdy_callback(ads131m0x_ctx_t *ctx,
                                      void (*callback)(ads131m0x_sample_t*, void*),
                                      void *user_data)
{
    (void)ctx;
    s_drdy_callback = callback;
    s_drdy_user_data = user_data;
}

/**
 * @brief Enable DRDY interrupt
 */
void ads131m0x_hal_enable_drdy_interrupt(ads131m0x_ctx_t *ctx)
{
    if (ctx == NULL) return;
    
    if (!nrf_drv_gpiote_is_init()) {
        nrf_drv_gpiote_init();
    }
    
    nrf_drv_gpiote_in_config_t config = GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    config.pull = NRF_GPIO_PIN_NOPULL;
    
    nrf_drv_gpiote_in_init(ctx->drdy_pin, &config, drdy_interrupt_handler);
    nrf_drv_gpiote_in_event_enable(ctx->drdy_pin, true);
}

/**
 * @brief Disable DRDY interrupt
 */
void ads131m0x_hal_disable_drdy_interrupt(ads131m0x_ctx_t *ctx)
{
    if (ctx == NULL) return;
    nrf_drv_gpiote_in_event_disable(ctx->drdy_pin);
}
