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

/* Maximum number of SPI peripherals that can be registered */
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
 * @brief SPI peripheral configuration
 */
typedef struct {
    uint8_t     cs_pin;         /* Chip select GPIO pin */
    bool        cs_active_low;  /* true = active low (most common) */
    uint32_t    frequency;      /* SPI clock frequency (NRF_SPIM_FREQ_*) */
    uint8_t     mode;           /* SPI mode (0-3) */
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
 * @brief Initialize the SPI bus manager
 * 
 * Must be called before any other SPI functions.
 * Creates the SPI mutex and initializes the SPIM peripheral.
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

#endif /* AGSYS_SPI_H */
