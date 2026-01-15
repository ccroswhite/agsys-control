/**
 * @file agsys_spi.c
 * @brief SPI bus manager implementation with EasyDMA - Multi-bus support
 * 
 * Uses nrfx_spim driver for DMA-based transfers.
 * Supports multiple SPI buses and both blocking and async operations.
 */

#include "agsys_spi.h"
#include "agsys_debug.h"
#include "nrfx_spim.h"
#include "nrf_gpio.h"
#include <string.h>

/* ==========================================================================
 * PRIVATE DATA STRUCTURES
 * ========================================================================== */

/* SPIM instances - we support up to 4 on nRF52840 */
static nrfx_spim_t m_spim_instances[] = {
    NRFX_SPIM_INSTANCE(0),
#if NRFX_SPIM1_ENABLED
    NRFX_SPIM_INSTANCE(1),
#endif
#if NRFX_SPIM2_ENABLED
    NRFX_SPIM_INSTANCE(2),
#endif
#if NRFX_SPIM3_ENABLED
    NRFX_SPIM_INSTANCE(3),
#endif
};

#define NUM_SPIM_INSTANCES (sizeof(m_spim_instances) / sizeof(m_spim_instances[0]))

typedef struct {
    uint8_t     cs_pin;
    bool        cs_active_low;
    uint32_t    frequency;
    uint8_t     mode;
    uint8_t     bus;            /* Which bus this peripheral is on */
    bool        in_use;
} spi_peripheral_t;

typedef struct {
    bool                    initialized;
    uint8_t                 spim_idx;       /* Index into m_spim_instances */
    SemaphoreHandle_t       mutex;
    SemaphoreHandle_t       xfer_done_sem;
    volatile bool           xfer_in_progress;
    agsys_spi_callback_t    async_callback;
    void                   *async_user_data;
    agsys_spi_handle_t      async_handle;
} spi_bus_t;

static spi_bus_t m_buses[AGSYS_SPI_MAX_BUSES];
static spi_peripheral_t m_peripherals[AGSYS_SPI_MAX_PERIPHERALS];
static bool m_module_initialized = false;

/* ==========================================================================
 * DMA EVENT HANDLERS (one per bus)
 * ========================================================================== */

static void spim_event_handler_common(agsys_spi_bus_t bus, nrfx_spim_evt_t const *p_event)
{
    if (p_event->type != NRFX_SPIM_EVENT_DONE) {
        return;
    }
    
    spi_bus_t *b = &m_buses[bus];
    
    /* Deassert CS */
    if (b->async_handle != AGSYS_SPI_INVALID_HANDLE && 
        b->async_handle < AGSYS_SPI_MAX_PERIPHERALS) {
        spi_peripheral_t *periph = &m_peripherals[b->async_handle];
        nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 1 : 0);
    }
    
    b->xfer_in_progress = false;
    
    /* Call user callback if registered */
    if (b->async_callback != NULL) {
        agsys_spi_callback_t cb = b->async_callback;
        void *user_data = b->async_user_data;
        b->async_callback = NULL;
        b->async_user_data = NULL;
        b->async_handle = AGSYS_SPI_INVALID_HANDLE;
        
        /* Release mutex before callback */
        BaseType_t mutex_woken = pdFALSE;
        xSemaphoreGiveFromISR(b->mutex, &mutex_woken);
        
        cb(AGSYS_OK, user_data);
        
        portYIELD_FROM_ISR(mutex_woken);
    } else {
        /* Signal completion semaphore for blocking waits */
        BaseType_t higher_priority_woken = pdFALSE;
        xSemaphoreGiveFromISR(b->xfer_done_sem, &higher_priority_woken);
        portYIELD_FROM_ISR(higher_priority_woken);
    }
}

static void spim0_event_handler(nrfx_spim_evt_t const *p_event, void *p_context)
{
    (void)p_context;
    spim_event_handler_common(AGSYS_SPI_BUS_0, p_event);
}

#if NRFX_SPIM1_ENABLED
static void spim1_event_handler(nrfx_spim_evt_t const *p_event, void *p_context)
{
    (void)p_context;
    spim_event_handler_common(AGSYS_SPI_BUS_1, p_event);
}
#endif

#if NRFX_SPIM2_ENABLED
static void spim2_event_handler(nrfx_spim_evt_t const *p_event, void *p_context)
{
    (void)p_context;
    spim_event_handler_common(AGSYS_SPI_BUS_2, p_event);
}
#endif

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

agsys_err_t agsys_spi_bus_init(agsys_spi_bus_t bus, const agsys_spi_bus_config_t *config)
{
    if (bus >= AGSYS_SPI_MAX_BUSES || config == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    
    spi_bus_t *b = &m_buses[bus];
    
    if (b->initialized) {
        return AGSYS_OK;  /* Already initialized */
    }
    
    /* Validate SPIM instance */
    if (config->spim_instance >= NUM_SPIM_INSTANCES) {
        AGSYS_LOG_ERROR("SPI: Invalid SPIM instance %d", config->spim_instance);
        return AGSYS_ERR_INVALID_PARAM;
    }
    
    /* Create mutex */
    b->mutex = xSemaphoreCreateMutex();
    if (b->mutex == NULL) {
        AGSYS_LOG_ERROR("SPI: Failed to create mutex for bus %d", bus);
        return AGSYS_ERR_NO_MEMORY;
    }
    
    /* Create transfer completion semaphore */
    b->xfer_done_sem = xSemaphoreCreateBinary();
    if (b->xfer_done_sem == NULL) {
        AGSYS_LOG_ERROR("SPI: Failed to create semaphore for bus %d", bus);
        vSemaphoreDelete(b->mutex);
        b->mutex = NULL;
        return AGSYS_ERR_NO_MEMORY;
    }
    
    /* Select event handler based on SPIM instance */
    nrfx_spim_evt_handler_t handler = NULL;
    switch (config->spim_instance) {
        case 0: handler = spim0_event_handler; break;
#if NRFX_SPIM1_ENABLED
        case 1: handler = spim1_event_handler; break;
#endif
#if NRFX_SPIM2_ENABLED
        case 2: handler = spim2_event_handler; break;
#endif
        default:
            AGSYS_LOG_ERROR("SPI: No handler for SPIM%d", config->spim_instance);
            vSemaphoreDelete(b->mutex);
            vSemaphoreDelete(b->xfer_done_sem);
            return AGSYS_ERR_INVALID_PARAM;
    }
    
    /* Configure SPIM with EasyDMA */
    nrfx_spim_config_t spi_config = NRFX_SPIM_DEFAULT_CONFIG;
    spi_config.sck_pin   = config->sck_pin;
    spi_config.mosi_pin  = config->mosi_pin;
    spi_config.miso_pin  = config->miso_pin;
    spi_config.ss_pin    = NRFX_SPIM_PIN_NOT_USED;  /* We manage CS ourselves */
    spi_config.frequency = NRF_SPIM_FREQ_8M;        /* Default, can be changed per-peripheral */
    spi_config.mode      = NRF_SPIM_MODE_0;
    spi_config.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST;
    
    nrfx_err_t err = nrfx_spim_init(&m_spim_instances[config->spim_instance], 
                                     &spi_config, handler, NULL);
    if (err != NRFX_SUCCESS) {
        AGSYS_LOG_ERROR("SPI: Bus %d init failed: %d", bus, err);
        vSemaphoreDelete(b->mutex);
        vSemaphoreDelete(b->xfer_done_sem);
        b->mutex = NULL;
        b->xfer_done_sem = NULL;
        return AGSYS_ERR_SPI;
    }
    
    b->spim_idx = config->spim_instance;
    b->initialized = true;
    b->xfer_in_progress = false;
    b->async_callback = NULL;
    b->async_user_data = NULL;
    b->async_handle = AGSYS_SPI_INVALID_HANDLE;
    
    if (!m_module_initialized) {
        memset(m_peripherals, 0, sizeof(m_peripherals));
        m_module_initialized = true;
    }
    
    AGSYS_LOG_INFO("SPI: Bus %d initialized (SPIM%d, SCK=%d, MOSI=%d, MISO=%d)", 
                   bus, config->spim_instance, config->sck_pin, 
                   config->mosi_pin, config->miso_pin);
    
    return AGSYS_OK;
}

agsys_err_t agsys_spi_init(uint8_t sck_pin, uint8_t mosi_pin, uint8_t miso_pin)
{
    /* Convenience function - initialize bus 0 with SPIM0 */
    agsys_spi_bus_config_t config = {
        .sck_pin = sck_pin,
        .mosi_pin = mosi_pin,
        .miso_pin = miso_pin,
        .spim_instance = 0,
    };
    return agsys_spi_bus_init(AGSYS_SPI_BUS_0, &config);
}

void agsys_spi_deinit(void)
{
    for (uint8_t bus = 0; bus < AGSYS_SPI_MAX_BUSES; bus++) {
        spi_bus_t *b = &m_buses[bus];
        if (!b->initialized) continue;
        
        nrfx_spim_uninit(&m_spim_instances[b->spim_idx]);
        
        if (b->mutex != NULL) {
            vSemaphoreDelete(b->mutex);
            b->mutex = NULL;
        }
        if (b->xfer_done_sem != NULL) {
            vSemaphoreDelete(b->xfer_done_sem);
            b->xfer_done_sem = NULL;
        }
        b->initialized = false;
    }
    
    memset(m_peripherals, 0, sizeof(m_peripherals));
    m_module_initialized = false;
}

/* ==========================================================================
 * PERIPHERAL REGISTRATION
 * ========================================================================== */

agsys_err_t agsys_spi_register(const agsys_spi_config_t *config, 
                                agsys_spi_handle_t *handle)
{
    if (config == NULL || handle == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    
    /* Validate bus */
    agsys_spi_bus_t bus = config->bus;
    if (bus >= AGSYS_SPI_MAX_BUSES || !m_buses[bus].initialized) {
        AGSYS_LOG_ERROR("SPI: Bus %d not initialized", bus);
        return AGSYS_ERR_NOT_INITIALIZED;
    }
    
    /* Find free slot */
    for (uint8_t i = 0; i < AGSYS_SPI_MAX_PERIPHERALS; i++) {
        if (!m_peripherals[i].in_use) {
            m_peripherals[i].cs_pin = config->cs_pin;
            m_peripherals[i].cs_active_low = config->cs_active_low;
            m_peripherals[i].frequency = config->frequency;
            m_peripherals[i].mode = config->mode;
            m_peripherals[i].bus = bus;
            m_peripherals[i].in_use = true;
            
            /* Configure CS pin as output, deasserted */
            nrf_gpio_cfg_output(config->cs_pin);
            nrf_gpio_pin_write(config->cs_pin, config->cs_active_low ? 1 : 0);
            
            *handle = i;
            AGSYS_LOG_DEBUG("SPI: Registered peripheral %d on bus %d (CS=%d)", 
                           i, bus, config->cs_pin);
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
 * HELPER - Get bus for peripheral
 * ========================================================================== */

static spi_bus_t* get_bus_for_handle(agsys_spi_handle_t handle)
{
    if (handle >= AGSYS_SPI_MAX_PERIPHERALS || !m_peripherals[handle].in_use) {
        return NULL;
    }
    uint8_t bus_idx = m_peripherals[handle].bus;
    if (bus_idx >= AGSYS_SPI_MAX_BUSES || !m_buses[bus_idx].initialized) {
        return NULL;
    }
    return &m_buses[bus_idx];
}

static nrfx_spim_t* get_spim_for_handle(agsys_spi_handle_t handle)
{
    spi_bus_t *b = get_bus_for_handle(handle);
    if (b == NULL) return NULL;
    return &m_spim_instances[b->spim_idx];
}

/* ==========================================================================
 * DATA TRANSFER
 * ========================================================================== */

agsys_err_t agsys_spi_transfer(agsys_spi_handle_t handle, 
                                const agsys_spi_xfer_t *xfer)
{
    spi_bus_t *b = get_bus_for_handle(handle);
    nrfx_spim_t *spim = get_spim_for_handle(handle);
    
    if (b == NULL || spim == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (xfer == NULL || xfer->length == 0) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    
    /* Acquire mutex */
    if (xSemaphoreTake(b->mutex, pdMS_TO_TICKS(AGSYS_SPI_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        AGSYS_LOG_WARNING("SPI: Mutex timeout on bus %d", m_peripherals[handle].bus);
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
    xSemaphoreTake(b->xfer_done_sem, 0);
    
    /* Track for ISR */
    b->async_handle = handle;
    b->async_callback = NULL;  /* Blocking mode */
    b->xfer_in_progress = true;
    
    /* Start DMA transfer */
    nrfx_err_t err = nrfx_spim_xfer(spim, &xfer_desc, 0);
    if (err != NRFX_SUCCESS) {
        b->xfer_in_progress = false;
        nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 1 : 0);
        xSemaphoreGive(b->mutex);
        AGSYS_LOG_ERROR("SPI: DMA transfer failed: %d", err);
        return AGSYS_ERR_SPI;
    }
    
    /* Wait for DMA completion */
    if (xSemaphoreTake(b->xfer_done_sem, pdMS_TO_TICKS(AGSYS_SPI_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        b->xfer_in_progress = false;
        nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 1 : 0);
        xSemaphoreGive(b->mutex);
        AGSYS_LOG_ERROR("SPI: DMA timeout");
        return AGSYS_ERR_TIMEOUT;
    }
    
    /* CS deasserted by ISR, release mutex */
    b->async_handle = AGSYS_SPI_INVALID_HANDLE;
    xSemaphoreGive(b->mutex);
    
    return AGSYS_OK;
}

agsys_err_t agsys_spi_transfer_multi(agsys_spi_handle_t handle,
                                      const agsys_spi_xfer_t *xfers,
                                      size_t count)
{
    spi_bus_t *b = get_bus_for_handle(handle);
    nrfx_spim_t *spim = get_spim_for_handle(handle);
    
    if (b == NULL || spim == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (xfers == NULL || count == 0) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    
    /* Acquire mutex */
    if (xSemaphoreTake(b->mutex, pdMS_TO_TICKS(AGSYS_SPI_MUTEX_TIMEOUT_MS)) != pdTRUE) {
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
        xSemaphoreTake(b->xfer_done_sem, 0);
        
        b->async_handle = AGSYS_SPI_INVALID_HANDLE;  /* Don't deassert CS in ISR */
        b->async_callback = NULL;
        b->xfer_in_progress = true;
        
        nrfx_err_t err = nrfx_spim_xfer(spim, &xfer_desc, 0);
        if (err != NRFX_SUCCESS) {
            AGSYS_LOG_ERROR("SPI: Multi-transfer %d failed: %d", i, err);
            result = AGSYS_ERR_SPI;
            break;
        }
        
        /* Wait for this transfer to complete */
        if (xSemaphoreTake(b->xfer_done_sem, pdMS_TO_TICKS(AGSYS_SPI_MUTEX_TIMEOUT_MS)) != pdTRUE) {
            AGSYS_LOG_ERROR("SPI: Multi-transfer %d timeout", i);
            result = AGSYS_ERR_TIMEOUT;
            break;
        }
    }
    
    b->xfer_in_progress = false;
    
    /* Deassert CS */
    nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 1 : 0);
    
    /* Release mutex */
    xSemaphoreGive(b->mutex);
    
    return result;
}

/* ==========================================================================
 * LOW-LEVEL ACCESS
 * ========================================================================== */

agsys_err_t agsys_spi_acquire(uint32_t timeout_ms)
{
    /* For backward compatibility, acquire bus 0 mutex */
    if (!m_buses[0].initialized) {
        return AGSYS_ERR_NOT_INITIALIZED;
    }
    
    if (xSemaphoreTake(m_buses[0].mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return AGSYS_ERR_TIMEOUT;
    }
    
    return AGSYS_OK;
}

void agsys_spi_release(void)
{
    if (m_buses[0].mutex != NULL) {
        xSemaphoreGive(m_buses[0].mutex);
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
    spi_bus_t *b = get_bus_for_handle(handle);
    nrfx_spim_t *spim = get_spim_for_handle(handle);
    
    if (b == NULL || spim == NULL) {
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
    xSemaphoreTake(b->xfer_done_sem, 0);
    
    b->async_handle = AGSYS_SPI_INVALID_HANDLE;  /* No CS management */
    b->async_callback = NULL;
    b->xfer_in_progress = true;
    
    nrfx_err_t err = nrfx_spim_xfer(spim, &xfer_desc, 0);
    if (err != NRFX_SUCCESS) {
        b->xfer_in_progress = false;
        return AGSYS_ERR_SPI;
    }
    
    /* Wait for DMA completion */
    if (xSemaphoreTake(b->xfer_done_sem, pdMS_TO_TICKS(AGSYS_SPI_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        b->xfer_in_progress = false;
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
    spi_bus_t *b = get_bus_for_handle(handle);
    nrfx_spim_t *spim = get_spim_for_handle(handle);
    
    if (b == NULL || spim == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (xfer == NULL || xfer->length == 0) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    
    /* Acquire mutex (will be released in ISR callback) */
    if (xSemaphoreTake(b->mutex, pdMS_TO_TICKS(AGSYS_SPI_MUTEX_TIMEOUT_MS)) != pdTRUE) {
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
    b->async_handle = handle;
    b->async_callback = callback;
    b->async_user_data = user_data;
    b->xfer_in_progress = true;
    
    /* Start DMA transfer */
    nrfx_err_t err = nrfx_spim_xfer(spim, &xfer_desc, 0);
    if (err != NRFX_SUCCESS) {
        b->xfer_in_progress = false;
        b->async_callback = NULL;
        b->async_user_data = NULL;
        b->async_handle = AGSYS_SPI_INVALID_HANDLE;
        nrf_gpio_pin_write(periph->cs_pin, periph->cs_active_low ? 1 : 0);
        xSemaphoreGive(b->mutex);
        return AGSYS_ERR_SPI;
    }
    
    /* Transfer started - ISR will handle completion */
    return AGSYS_OK;
}

bool agsys_spi_is_busy(void)
{
    /* Check all buses */
    for (uint8_t i = 0; i < AGSYS_SPI_MAX_BUSES; i++) {
        if (m_buses[i].initialized && m_buses[i].xfer_in_progress) {
            return true;
        }
    }
    return false;
}

agsys_err_t agsys_spi_wait_complete(uint32_t timeout_ms)
{
    TickType_t start = xTaskGetTickCount();
    
    while (agsys_spi_is_busy()) {
        if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(timeout_ms)) {
            return AGSYS_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }
    
    return AGSYS_OK;
}
