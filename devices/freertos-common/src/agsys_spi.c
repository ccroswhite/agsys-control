/**
 * @file agsys_spi.c
 * @brief SPI bus manager implementation
 * 
 * Uses nRF5 SDK legacy SPI driver for simpler configuration.
 */

#include "agsys_spi.h"
#include "agsys_debug.h"
#include "nrf_drv_spi.h"

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static const nrf_drv_spi_t m_spi = NRF_DRV_SPI_INSTANCE(0);
static SemaphoreHandle_t m_spi_mutex = NULL;
static bool m_initialized = false;

typedef struct {
    uint8_t     cs_pin;
    bool        cs_active_low;
    uint32_t    frequency;
    uint8_t     mode;
    bool        in_use;
} spi_peripheral_t;

static spi_peripheral_t m_peripherals[AGSYS_SPI_MAX_PERIPHERALS];

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

agsys_err_t agsys_spi_init(uint8_t sck_pin, uint8_t mosi_pin, uint8_t miso_pin)
{
    if (m_initialized) {
        return AGSYS_OK;
    }

    /* Create mutex */
    m_spi_mutex = xSemaphoreCreateMutex();
    if (m_spi_mutex == NULL) {
        AGSYS_LOG_ERROR("SPI: Failed to create mutex");
        return AGSYS_ERR_NO_MEMORY;
    }

    /* Configure SPI using legacy driver */
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.sck_pin   = sck_pin;
    spi_config.mosi_pin  = mosi_pin;
    spi_config.miso_pin  = miso_pin;
    spi_config.ss_pin    = NRF_DRV_SPI_PIN_NOT_USED;  /* We manage CS ourselves */
    spi_config.frequency = NRF_DRV_SPI_FREQ_4M;

    ret_code_t err = nrf_drv_spi_init(&m_spi, &spi_config, NULL, NULL);
    if (err != NRF_SUCCESS) {
        AGSYS_LOG_ERROR("SPI: Init failed: %d", err);
        vSemaphoreDelete(m_spi_mutex);
        m_spi_mutex = NULL;
        return AGSYS_ERR_SPI;
    }

    /* Initialize peripheral table */
    memset(m_peripherals, 0, sizeof(m_peripherals));

    m_initialized = true;
    AGSYS_LOG_INFO("SPI: Initialized (SCK=%d, MOSI=%d, MISO=%d)", 
                   sck_pin, mosi_pin, miso_pin);

    return AGSYS_OK;
}

void agsys_spi_deinit(void)
{
    if (!m_initialized) {
        return;
    }

    nrf_drv_spi_uninit(&m_spi);

    if (m_spi_mutex != NULL) {
        vSemaphoreDelete(m_spi_mutex);
        m_spi_mutex = NULL;
    }

    m_initialized = false;
}

/* ==========================================================================
 * PERIPHERAL REGISTRATION
 * ========================================================================== */

agsys_err_t agsys_spi_register(const agsys_spi_config_t *config, 
                                agsys_spi_handle_t *handle)
{
    if (!m_initialized || config == NULL || handle == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    /* Find free slot */
    for (uint8_t i = 0; i < AGSYS_SPI_MAX_PERIPHERALS; i++) {
        if (!m_peripherals[i].in_use) {
            m_peripherals[i].cs_pin = config->cs_pin;
            m_peripherals[i].cs_active_low = config->cs_active_low;
            m_peripherals[i].frequency = config->frequency;
            m_peripherals[i].mode = config->mode;
            m_peripherals[i].in_use = true;

            /* Configure CS pin as output, deasserted */
            nrf_gpio_cfg_output(config->cs_pin);
            nrf_gpio_pin_write(config->cs_pin, config->cs_active_low ? 1 : 0);

            *handle = i;
            AGSYS_LOG_DEBUG("SPI: Registered peripheral %d (CS=%d)", i, config->cs_pin);
            return AGSYS_OK;
        }
    }

    AGSYS_LOG_ERROR("SPI: No free peripheral slots");
    return AGSYS_ERR_NO_MEMORY;
}

agsys_err_t agsys_spi_unregister(agsys_spi_handle_t handle)
{
    if (handle >= AGSYS_SPI_MAX_PERIPHERALS || !m_peripherals[handle].in_use) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    m_peripherals[handle].in_use = false;
    return AGSYS_OK;
}

/* ==========================================================================
 * DATA TRANSFER
 * ========================================================================== */

agsys_err_t agsys_spi_transfer(agsys_spi_handle_t handle, 
                                const agsys_spi_xfer_t *xfer)
{
    if (handle >= AGSYS_SPI_MAX_PERIPHERALS || !m_peripherals[handle].in_use) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (xfer == NULL || xfer->length == 0) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    /* Acquire mutex */
    if (xSemaphoreTake(m_spi_mutex, pdMS_TO_TICKS(AGSYS_SPI_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        AGSYS_LOG_WARNING("SPI: Mutex timeout");
        return AGSYS_ERR_TIMEOUT;
    }

    spi_peripheral_t *periph = &m_peripherals[handle];

    /* Assert CS */
    nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 0 : 1);

    /* Perform transfer using legacy driver */
    ret_code_t err = nrf_drv_spi_transfer(&m_spi, 
                                           xfer->tx_buf, xfer->tx_buf ? xfer->length : 0,
                                           xfer->rx_buf, xfer->rx_buf ? xfer->length : 0);

    /* Deassert CS */
    nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 1 : 0);

    /* Release mutex */
    xSemaphoreGive(m_spi_mutex);

    if (err != NRF_SUCCESS) {
        AGSYS_LOG_ERROR("SPI: Transfer failed: %d", err);
        return AGSYS_ERR_SPI;
    }

    return AGSYS_OK;
}

agsys_err_t agsys_spi_transfer_multi(agsys_spi_handle_t handle,
                                      const agsys_spi_xfer_t *xfers,
                                      size_t count)
{
    if (handle >= AGSYS_SPI_MAX_PERIPHERALS || !m_peripherals[handle].in_use) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (xfers == NULL || count == 0) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    /* Acquire mutex */
    if (xSemaphoreTake(m_spi_mutex, pdMS_TO_TICKS(AGSYS_SPI_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return AGSYS_ERR_TIMEOUT;
    }

    spi_peripheral_t *periph = &m_peripherals[handle];
    agsys_err_t result = AGSYS_OK;

    /* Assert CS */
    nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 0 : 1);

    /* Perform all transfers */
    for (size_t i = 0; i < count; i++) {
        ret_code_t err = nrf_drv_spi_transfer(&m_spi,
                                               xfers[i].tx_buf, xfers[i].tx_buf ? xfers[i].length : 0,
                                               xfers[i].rx_buf, xfers[i].rx_buf ? xfers[i].length : 0);
        if (err != NRF_SUCCESS) {
            AGSYS_LOG_ERROR("SPI: Multi-transfer %d failed: %d", i, err);
            result = AGSYS_ERR_SPI;
            break;
        }
    }

    /* Deassert CS */
    nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 1 : 0);

    /* Release mutex */
    xSemaphoreGive(m_spi_mutex);

    return result;
}

/* ==========================================================================
 * LOW-LEVEL ACCESS
 * ========================================================================== */

agsys_err_t agsys_spi_acquire(uint32_t timeout_ms)
{
    if (!m_initialized) {
        return AGSYS_ERR_NOT_INITIALIZED;
    }

    if (xSemaphoreTake(m_spi_mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return AGSYS_ERR_TIMEOUT;
    }

    return AGSYS_OK;
}

void agsys_spi_release(void)
{
    if (m_spi_mutex != NULL) {
        xSemaphoreGive(m_spi_mutex);
    }
}

void agsys_spi_cs_assert(agsys_spi_handle_t handle)
{
    if (handle < AGSYS_SPI_MAX_PERIPHERALS && m_peripherals[handle].in_use) {
        spi_peripheral_t *periph = &m_peripherals[handle];
        nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 0 : 1);
    }
}

void agsys_spi_cs_deassert(agsys_spi_handle_t handle)
{
    if (handle < AGSYS_SPI_MAX_PERIPHERALS && m_peripherals[handle].in_use) {
        spi_peripheral_t *periph = &m_peripherals[handle];
        nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 1 : 0);
    }
}

agsys_err_t agsys_spi_transfer_raw(agsys_spi_handle_t handle,
                                    const agsys_spi_xfer_t *xfer)
{
    if (handle >= AGSYS_SPI_MAX_PERIPHERALS || !m_peripherals[handle].in_use) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (xfer == NULL || xfer->length == 0) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    ret_code_t err = nrf_drv_spi_transfer(&m_spi,
                                           xfer->tx_buf, xfer->tx_buf ? xfer->length : 0,
                                           xfer->rx_buf, xfer->rx_buf ? xfer->length : 0);
    if (err != NRF_SUCCESS) {
        return AGSYS_ERR_SPI;
    }

    return AGSYS_OK;
}
