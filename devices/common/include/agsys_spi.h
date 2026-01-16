/**
 * @file agsys_spi.h
 * @brief SPI bus manager with FreeRTOS mutex protection
 * 
 * Provides thread-safe SPI access for multiple peripherals sharing the bus.
 * Each peripheral has its own CS pin, managed by this module.
 */

#ifndef AGSYS_SPI_H
#define AGSYS_SPI_H

#include "agsys_common.h"
#include "nrfx_spim.h"

/* ==========================================================================
 * CONFIGURATION
 * ========================================================================== */

/* Maximum number of SPI buses (SPIM instances) */
#ifndef AGSYS_SPI_MAX_BUSES
#define AGSYS_SPI_MAX_BUSES         3
#endif

/* Maximum number of SPI peripherals that can be registered per bus */
#ifndef AGSYS_SPI_MAX_PERIPHERALS
#define AGSYS_SPI_MAX_PERIPHERALS   6
#endif

/* Default timeout for acquiring SPI mutex (ms) */
#ifndef AGSYS_SPI_MUTEX_TIMEOUT_MS
#define AGSYS_SPI_MUTEX_TIMEOUT_MS  1000
#endif

/* ==========================================================================
 * TYPES
 * ========================================================================== */

/**
 * @brief SPI peripheral handle
 */
typedef uint8_t agsys_spi_handle_t;

#define AGSYS_SPI_INVALID_HANDLE    0xFF

/**
 * @brief SPI bus identifier
 */
typedef uint8_t agsys_spi_bus_t;

#define AGSYS_SPI_BUS_0     0
#define AGSYS_SPI_BUS_1     1
#define AGSYS_SPI_BUS_2     2

/**
 * @brief SPI peripheral configuration
 */
typedef struct {
    uint8_t     cs_pin;         /* Chip select GPIO pin */
    bool        cs_active_low;  /* true = active low (most common) */
    uint32_t    frequency;      /* SPI clock frequency (NRF_SPIM_FREQ_*) */
    uint8_t     mode;           /* SPI mode (0-3) */
    agsys_spi_bus_t bus;        /* Which SPI bus (default 0) */
} agsys_spi_config_t;

/**
 * @brief SPI transfer descriptor
 */
typedef struct {
    const uint8_t   *tx_buf;    /* TX buffer (NULL for RX-only) */
    uint8_t         *rx_buf;    /* RX buffer (NULL for TX-only) */
    size_t          length;     /* Transfer length */
} agsys_spi_xfer_t;

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

/**
 * @brief SPI bus configuration
 */
typedef struct {
    uint8_t     sck_pin;        /* SPI clock pin */
    uint8_t     mosi_pin;       /* SPI MOSI pin */
    uint8_t     miso_pin;       /* SPI MISO pin */
    uint8_t     spim_instance;  /* SPIM instance (0, 1, 2, or 3) */
} agsys_spi_bus_config_t;

/**
 * @brief Initialize a SPI bus
 * 
 * Must be called before registering peripherals on this bus.
 * Creates mutex and initializes the SPIM peripheral with DMA.
 * 
 * @param bus       Bus identifier (AGSYS_SPI_BUS_0, etc.)
 * @param config    Bus configuration (pins, instance)
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_spi_bus_init(agsys_spi_bus_t bus, const agsys_spi_bus_config_t *config);

/**
 * @brief Initialize the default SPI bus (bus 0)
 * 
 * Convenience function for single-bus devices.
 * 
 * @param sck_pin   SPI clock pin
 * @param mosi_pin  SPI MOSI pin
 * @param miso_pin  SPI MISO pin
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_spi_init(uint8_t sck_pin, uint8_t mosi_pin, uint8_t miso_pin);

/**
 * @brief Deinitialize the SPI bus manager
 */
void agsys_spi_deinit(void);

/* ==========================================================================
 * PERIPHERAL REGISTRATION
 * ========================================================================== */

/**
 * @brief Register a peripheral on the SPI bus
 * 
 * @param config    Peripheral configuration
 * @param handle    Output: handle for future operations
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_spi_register(const agsys_spi_config_t *config, 
                                agsys_spi_handle_t *handle);

/**
 * @brief Unregister a peripheral
 * 
 * @param handle    Peripheral handle
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_spi_unregister(agsys_spi_handle_t handle);

/* ==========================================================================
 * DATA TRANSFER
 * ========================================================================== */

/**
 * @brief Perform a SPI transfer (blocking, mutex-protected)
 * 
 * Acquires the SPI mutex, asserts CS, performs transfer, deasserts CS,
 * releases mutex.
 * 
 * @param handle    Peripheral handle
 * @param xfer      Transfer descriptor
 * @return AGSYS_OK on success, AGSYS_ERR_TIMEOUT if mutex not acquired
 */
agsys_err_t agsys_spi_transfer(agsys_spi_handle_t handle, 
                                const agsys_spi_xfer_t *xfer);

/**
 * @brief Perform multiple SPI transfers with CS held (blocking)
 * 
 * Useful for command + data sequences where CS must stay asserted.
 * 
 * @param handle    Peripheral handle
 * @param xfers     Array of transfer descriptors
 * @param count     Number of transfers
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_spi_transfer_multi(agsys_spi_handle_t handle,
                                      const agsys_spi_xfer_t *xfers,
                                      size_t count);

/* ==========================================================================
 * LOW-LEVEL ACCESS (use with caution)
 * ========================================================================== */

/**
 * @brief Acquire the SPI bus mutex
 * 
 * Use for multi-transfer sequences where you need to hold the bus.
 * Must call agsys_spi_release() when done.
 * 
 * @param timeout_ms    Timeout in milliseconds
 * @return AGSYS_OK on success, AGSYS_ERR_TIMEOUT if not acquired
 */
agsys_err_t agsys_spi_acquire(uint32_t timeout_ms);

/**
 * @brief Release the SPI bus mutex
 */
void agsys_spi_release(void);

/**
 * @brief Assert CS for a peripheral (must hold mutex)
 * @param handle    Peripheral handle
 */
void agsys_spi_cs_assert(agsys_spi_handle_t handle);

/**
 * @brief Deassert CS for a peripheral (must hold mutex)
 * @param handle    Peripheral handle
 */
void agsys_spi_cs_deassert(agsys_spi_handle_t handle);

/**
 * @brief Raw transfer without CS management (must hold mutex)
 * @param handle    Peripheral handle (for frequency/mode config)
 * @param xfer      Transfer descriptor
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_spi_transfer_raw(agsys_spi_handle_t handle,
                                    const agsys_spi_xfer_t *xfer);

/* ==========================================================================
 * ASYNC DMA TRANSFERS
 * ========================================================================== */

/**
 * @brief Completion callback for async transfers
 * @param result    Transfer result (AGSYS_OK on success)
 * @param user_data User-provided context
 */
typedef void (*agsys_spi_callback_t)(agsys_err_t result, void *user_data);

/**
 * @brief Start an async DMA transfer
 * 
 * Returns immediately. Callback is invoked when transfer completes.
 * CS is automatically managed. Mutex is held until callback.
 * 
 * @param handle    Peripheral handle
 * @param xfer      Transfer descriptor
 * @param callback  Completion callback (NULL for fire-and-forget)
 * @param user_data User context passed to callback
 * @return AGSYS_OK if transfer started, error otherwise
 */
agsys_err_t agsys_spi_transfer_async(agsys_spi_handle_t handle,
                                      const agsys_spi_xfer_t *xfer,
                                      agsys_spi_callback_t callback,
                                      void *user_data);

/**
 * @brief Check if an async transfer is in progress
 * @return true if transfer pending
 */
bool agsys_spi_is_busy(void);

/**
 * @brief Wait for async transfer to complete
 * @param timeout_ms Maximum wait time
 * @return AGSYS_OK if completed, AGSYS_ERR_TIMEOUT if still busy
 */
agsys_err_t agsys_spi_wait_complete(uint32_t timeout_ms);

#endif /* AGSYS_SPI_H */
