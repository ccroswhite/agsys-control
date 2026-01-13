/**
 * @file agsys_spi.c
 * @brief SPI bus manager implementation with EasyDMA
 * 
 * Uses nrfx_spim driver for DMA-based transfers.
 * Supports both blocking and async operations.
 */

#include "agsys_spi.h"
#include "agsys_debug.h"
#include "nrfx_spim.h"
#include "nrf_gpio.h"

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static nrfx_spim_t m_spi = NRFX_SPIM_INSTANCE(0);
static SemaphoreHandle_t m_spi_mutex = NULL;
static SemaphoreHandle_t m_xfer_done_sem = NULL;
static bool m_initialized = false;

/* Async transfer state */
static volatile bool m_xfer_in_progress = false;
static agsys_spi_callback_t m_async_callback = NULL;
static void *m_async_user_data = NULL;
static agsys_spi_handle_t m_async_handle = AGSYS_SPI_INVALID_HANDLE;

typedef struct {
    uint8_t     cs_pin;
    bool        cs_active_low;
    uint32_t    frequency;
    uint8_t     mode;
    bool        in_use;
} spi_peripheral_t;

static spi_peripheral_t m_peripherals[AGSYS_SPI_MAX_PERIPHERALS];

/* ==========================================================================
 * DMA EVENT HANDLER
 * ========================================================================== */

static void spim_event_handler(nrfx_spim_evt_t const *p_event, void *p_context)
{
    (void)p_context;
    
    if (p_event->type == NRFX_SPIM_EVENT_DONE) {
        /* Deassert CS */
        if (m_async_handle != AGSYS_SPI_INVALID_HANDLE && 
            m_async_handle < AGSYS_SPI_MAX_PERIPHERALS) {
            spi_peripheral_t *periph = &m_peripherals[m_async_handle];
            nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 1 : 0);
        }
        
        m_xfer_in_progress = false;
        
        /* Call user callback if registered */
        if (m_async_callback != NULL) {
            agsys_spi_callback_t cb = m_async_callback;
            void *user_data = m_async_user_data;
            m_async_callback = NULL;
            m_async_user_data = NULL;
            m_async_handle = AGSYS_SPI_INVALID_HANDLE;
            
            /* Release mutex before callback */
            xSemaphoreGiveFromISR(m_spi_mutex, NULL);
            
            cb(AGSYS_OK, user_data);
        } else {
            /* Signal completion semaphore for blocking waits */
            BaseType_t higher_priority_woken = pdFALSE;
            xSemaphoreGiveFromISR(m_xfer_done_sem, &higher_priority_woken);
            portYIELD_FROM_ISR(higher_priority_woken);
        }
    }
}

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

    /* Create transfer completion semaphore */
    m_xfer_done_sem = xSemaphoreCreateBinary();
    if (m_xfer_done_sem == NULL) {
        AGSYS_LOG_ERROR("SPI: Failed to create semaphore");
        vSemaphoreDelete(m_spi_mutex);
        m_spi_mutex = NULL;
        return AGSYS_ERR_NO_MEMORY;
    }

    /* Configure SPIM with EasyDMA */
    nrfx_spim_config_t spi_config = NRFX_SPIM_DEFAULT_CONFIG;
    spi_config.sck_pin   = sck_pin;
    spi_config.mosi_pin  = mosi_pin;
    spi_config.miso_pin  = miso_pin;
    spi_config.ss_pin    = NRFX_SPIM_PIN_NOT_USED;  /* We manage CS ourselves */
    spi_config.frequency = NRF_SPIM_FREQ_8M;        /* Default 8MHz, can be changed per-peripheral */
    spi_config.mode      = NRF_SPIM_MODE_0;
    spi_config.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST;

    nrfx_err_t err = nrfx_spim_init(&m_spi, &spi_config, spim_event_handler, NULL);
    if (err != NRFX_SUCCESS) {
        AGSYS_LOG_ERROR("SPI: Init failed: %d", err);
        vSemaphoreDelete(m_spi_mutex);
        vSemaphoreDelete(m_xfer_done_sem);
        m_spi_mutex = NULL;
        m_xfer_done_sem = NULL;
        return AGSYS_ERR_SPI;
    }

    /* Initialize peripheral table */
    memset(m_peripherals, 0, sizeof(m_peripherals));

    m_initialized = true;
    m_xfer_in_progress = false;
    m_async_callback = NULL;
    m_async_user_data = NULL;
    m_async_handle = AGSYS_SPI_INVALID_HANDLE;

    AGSYS_LOG_INFO("SPI: Initialized with DMA (SCK=%d, MOSI=%d, MISO=%d)", 
                   sck_pin, mosi_pin, miso_pin);

    return AGSYS_OK;
}

void agsys_spi_deinit(void)
{
    if (!m_initialized) {
        return;
    }

    nrfx_spim_uninit(&m_spi);

    if (m_spi_mutex != NULL) {
        vSemaphoreDelete(m_spi_mutex);
        m_spi_mutex = NULL;
    }

    if (m_xfer_done_sem != NULL) {
        vSemaphoreDelete(m_xfer_done_sem);
        m_xfer_done_sem = NULL;
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

    /* Set up DMA transfer descriptor */
    nrfx_spim_xfer_desc_t xfer_desc = {
        .p_tx_buffer = xfer->tx_buf,
        .tx_length   = xfer->tx_buf ? xfer->length : 0,
        .p_rx_buffer = xfer->rx_buf,
        .rx_length   = xfer->rx_buf ? xfer->length : 0,
    };

    /* Clear completion semaphore */
    xSemaphoreTake(m_xfer_done_sem, 0);

    /* Track for ISR */
    m_async_handle = handle;
    m_async_callback = NULL;  /* Blocking mode */
    m_xfer_in_progress = true;

    /* Start DMA transfer */
    nrfx_err_t err = nrfx_spim_xfer(&m_spi, &xfer_desc, 0);
    if (err != NRFX_SUCCESS) {
        m_xfer_in_progress = false;
        nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 1 : 0);
        xSemaphoreGive(m_spi_mutex);
        AGSYS_LOG_ERROR("SPI: DMA transfer failed: %d", err);
        return AGSYS_ERR_SPI;
    }

    /* Wait for DMA completion */
    if (xSemaphoreTake(m_xfer_done_sem, pdMS_TO_TICKS(AGSYS_SPI_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        m_xfer_in_progress = false;
        nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 1 : 0);
        xSemaphoreGive(m_spi_mutex);
        AGSYS_LOG_ERROR("SPI: DMA timeout");
        return AGSYS_ERR_TIMEOUT;
    }

    /* CS deasserted by ISR, release mutex */
    m_async_handle = AGSYS_SPI_INVALID_HANDLE;
    xSemaphoreGive(m_spi_mutex);

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

    /* Perform all transfers with DMA */
    for (size_t i = 0; i < count; i++) {
        nrfx_spim_xfer_desc_t xfer_desc = {
            .p_tx_buffer = xfers[i].tx_buf,
            .tx_length   = xfers[i].tx_buf ? xfers[i].length : 0,
            .p_rx_buffer = xfers[i].rx_buf,
            .rx_length   = xfers[i].rx_buf ? xfers[i].length : 0,
        };

        /* Clear completion semaphore */
        xSemaphoreTake(m_xfer_done_sem, 0);
        
        m_async_handle = AGSYS_SPI_INVALID_HANDLE;  /* Don't deassert CS in ISR */
        m_async_callback = NULL;
        m_xfer_in_progress = true;

        nrfx_err_t err = nrfx_spim_xfer(&m_spi, &xfer_desc, 0);
        if (err != NRFX_SUCCESS) {
            AGSYS_LOG_ERROR("SPI: Multi-transfer %d failed: %d", i, err);
            result = AGSYS_ERR_SPI;
            break;
        }

        /* Wait for this transfer to complete */
        if (xSemaphoreTake(m_xfer_done_sem, pdMS_TO_TICKS(AGSYS_SPI_MUTEX_TIMEOUT_MS)) != pdTRUE) {
            AGSYS_LOG_ERROR("SPI: Multi-transfer %d timeout", i);
            result = AGSYS_ERR_TIMEOUT;
            break;
        }
    }

    m_xfer_in_progress = false;

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

    nrfx_spim_xfer_desc_t xfer_desc = {
        .p_tx_buffer = xfer->tx_buf,
        .tx_length   = xfer->tx_buf ? xfer->length : 0,
        .p_rx_buffer = xfer->rx_buf,
        .rx_length   = xfer->rx_buf ? xfer->length : 0,
    };

    /* Clear completion semaphore */
    xSemaphoreTake(m_xfer_done_sem, 0);
    
    m_async_handle = AGSYS_SPI_INVALID_HANDLE;  /* No CS management */
    m_async_callback = NULL;
    m_xfer_in_progress = true;

    nrfx_err_t err = nrfx_spim_xfer(&m_spi, &xfer_desc, 0);
    if (err != NRFX_SUCCESS) {
        m_xfer_in_progress = false;
        return AGSYS_ERR_SPI;
    }

    /* Wait for DMA completion */
    if (xSemaphoreTake(m_xfer_done_sem, pdMS_TO_TICKS(AGSYS_SPI_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        m_xfer_in_progress = false;
        return AGSYS_ERR_TIMEOUT;
    }

    return AGSYS_OK;
}

/* ==========================================================================
 * ASYNC DMA TRANSFERS
 * ========================================================================== */

agsys_err_t agsys_spi_transfer_async(agsys_spi_handle_t handle,
                                      const agsys_spi_xfer_t *xfer,
                                      agsys_spi_callback_t callback,
                                      void *user_data)
{
    if (handle >= AGSYS_SPI_MAX_PERIPHERALS || !m_peripherals[handle].in_use) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (xfer == NULL || xfer->length == 0) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    /* Acquire mutex (will be released in ISR callback) */
    if (xSemaphoreTake(m_spi_mutex, pdMS_TO_TICKS(AGSYS_SPI_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return AGSYS_ERR_TIMEOUT;
    }

    spi_peripheral_t *periph = &m_peripherals[handle];

    /* Assert CS */
    nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 0 : 1);

    /* Set up DMA transfer */
    nrfx_spim_xfer_desc_t xfer_desc = {
        .p_tx_buffer = xfer->tx_buf,
        .tx_length   = xfer->tx_buf ? xfer->length : 0,
        .p_rx_buffer = xfer->rx_buf,
        .rx_length   = xfer->rx_buf ? xfer->length : 0,
    };

    /* Store callback info for ISR */
    m_async_handle = handle;
    m_async_callback = callback;
    m_async_user_data = user_data;
    m_xfer_in_progress = true;

    /* Start DMA transfer */
    nrfx_err_t err = nrfx_spim_xfer(&m_spi, &xfer_desc, 0);
    if (err != NRFX_SUCCESS) {
        m_xfer_in_progress = false;
        m_async_callback = NULL;
        m_async_user_data = NULL;
        m_async_handle = AGSYS_SPI_INVALID_HANDLE;
        nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 1 : 0);
        xSemaphoreGive(m_spi_mutex);
        return AGSYS_ERR_SPI;
    }

    /* Transfer started - ISR will handle completion */
    return AGSYS_OK;
}

bool agsys_spi_is_busy(void)
{
    return m_xfer_in_progress;
}

agsys_err_t agsys_spi_wait_complete(uint32_t timeout_ms)
{
    if (!m_xfer_in_progress) {
        return AGSYS_OK;
    }

    TickType_t start = xTaskGetTickCount();
    while (m_xfer_in_progress) {
        if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(timeout_ms)) {
            return AGSYS_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }

    return AGSYS_OK;
}
