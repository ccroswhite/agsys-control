/**
 * @file spi_driver.c
 * @brief SPI driver for Soil Moisture Sensor
 */

#include "sdk_config.h"
#include "spi_driver.h"
#include "board_config.h"

#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "SEGGER_RTT.h"

#include "FreeRTOS.h"
#include "semphr.h"

static const nrf_drv_spi_t m_spi = NRF_DRV_SPI_INSTANCE(0);
static bool m_initialized = false;

extern SemaphoreHandle_t g_spi_mutex;

bool spi_init(void)
{
    if (m_initialized) {
        return true;
    }

    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.sck_pin   = SPI_SCK_PIN;
    spi_config.mosi_pin  = SPI_MOSI_PIN;
    spi_config.miso_pin  = SPI_MISO_PIN;
    spi_config.ss_pin    = NRF_DRV_SPI_PIN_NOT_USED;
    spi_config.frequency = NRF_DRV_SPI_FREQ_4M;
    spi_config.mode      = NRF_DRV_SPI_MODE_0;
    spi_config.bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST;

    ret_code_t err = nrf_drv_spi_init(&m_spi, &spi_config, NULL, NULL);
    if (err != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "SPI init failed: %d\n", err);
        return false;
    }

    /* Configure CS pins */
    nrf_gpio_cfg_output(SPI_CS_LORA_PIN);
    nrf_gpio_cfg_output(SPI_CS_FRAM_PIN);
    nrf_gpio_cfg_output(SPI_CS_FLASH_PIN);
    nrf_gpio_pin_set(SPI_CS_LORA_PIN);
    nrf_gpio_pin_set(SPI_CS_FRAM_PIN);
    nrf_gpio_pin_set(SPI_CS_FLASH_PIN);

    m_initialized = true;
    SEGGER_RTT_printf(0, "SPI initialized\n");
    return true;
}

bool spi_acquire(uint32_t timeout)
{
    if (g_spi_mutex == NULL) {
        return true;
    }
    return xSemaphoreTake(g_spi_mutex, timeout) == pdTRUE;
}

void spi_release(void)
{
    if (g_spi_mutex != NULL) {
        xSemaphoreGive(g_spi_mutex);
    }
}

void spi_cs_assert(uint8_t cs_pin)
{
    nrf_gpio_pin_clear(cs_pin);
}

void spi_cs_deassert(uint8_t cs_pin)
{
    nrf_gpio_pin_set(cs_pin);
}

bool spi_transfer_raw(const uint8_t *tx_buf, uint8_t *rx_buf, size_t len)
{
    ret_code_t err = nrf_drv_spi_transfer(&m_spi, tx_buf, len, rx_buf, len);
    return (err == NRF_SUCCESS);
}

bool spi_transfer(uint8_t cs_pin, const uint8_t *tx_buf, uint8_t *rx_buf, size_t len)
{
    spi_cs_assert(cs_pin);
    bool result = spi_transfer_raw(tx_buf, rx_buf, len);
    spi_cs_deassert(cs_pin);
    return result;
}
