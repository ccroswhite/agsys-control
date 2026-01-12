/**
 * @file spi_driver.c
 * @brief SPI driver for Valve Controller
 * 
 * Manages shared SPI bus for MCP2515 CAN, RFM95C LoRa, and MB85RS1MT FRAM.
 * Uses FreeRTOS mutex for thread-safe access.
 */

#include "sdk_config.h"
#include "spi_driver.h"
#include "board_config.h"

#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "SEGGER_RTT.h"

#include "FreeRTOS.h"
#include "semphr.h"

/* SPI Bus 0 - Peripherals (CAN + LoRa) */
static const nrf_drv_spi_t m_spi_periph = NRF_DRV_SPI_INSTANCE(0);
/* SPI Bus 2 - Memory (FRAM + Flash) - uses standard pins from agsys_pins.h */
/* Note: SPI1 conflicts with TWI1 (RTC), so we use SPI2 */
static const nrf_drv_spi_t m_spi_mem = NRF_DRV_SPI_INSTANCE(2);
static bool m_initialized = false;

/* External mutex from main.c */
extern SemaphoreHandle_t g_spi_mutex;

bool spi_init(void)
{
    if (m_initialized) {
        return true;
    }

    /* Initialize SPI Bus 0 - Peripherals (CAN + LoRa) */
    nrf_drv_spi_config_t spi_periph_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_periph_config.sck_pin   = SPI_PERIPH_SCK_PIN;
    spi_periph_config.mosi_pin  = SPI_PERIPH_MOSI_PIN;
    spi_periph_config.miso_pin  = SPI_PERIPH_MISO_PIN;
    spi_periph_config.ss_pin    = NRF_DRV_SPI_PIN_NOT_USED;
    spi_periph_config.frequency = NRF_DRV_SPI_FREQ_4M;
    spi_periph_config.mode      = NRF_DRV_SPI_MODE_0;
    spi_periph_config.bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST;

    ret_code_t err = nrf_drv_spi_init(&m_spi_periph, &spi_periph_config, NULL, NULL);
    if (err != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "SPI Periph init failed: %d\n", err);
        return false;
    }

    /* Initialize SPI Bus 1 - Memory (standard pins) */
    nrf_drv_spi_config_t spi_mem_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_mem_config.sck_pin   = AGSYS_MEM_SPI_SCK;
    spi_mem_config.mosi_pin  = AGSYS_MEM_SPI_MOSI;
    spi_mem_config.miso_pin  = AGSYS_MEM_SPI_MISO;
    spi_mem_config.ss_pin    = NRF_DRV_SPI_PIN_NOT_USED;
    spi_mem_config.frequency = NRF_DRV_SPI_FREQ_8M;
    spi_mem_config.mode      = NRF_DRV_SPI_MODE_0;
    spi_mem_config.bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST;

    err = nrf_drv_spi_init(&m_spi_mem, &spi_mem_config, NULL, NULL);
    if (err != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "SPI Memory init failed: %d\n", err);
        return false;
    }

    /* Configure CS pins as outputs, deasserted (high) */
    nrf_gpio_cfg_output(SPI_CS_CAN_PIN);
    nrf_gpio_cfg_output(SPI_CS_LORA_PIN);
    nrf_gpio_cfg_output(AGSYS_MEM_FRAM_CS);
    nrf_gpio_cfg_output(AGSYS_MEM_FLASH_CS);
    nrf_gpio_pin_set(SPI_CS_CAN_PIN);
    nrf_gpio_pin_set(SPI_CS_LORA_PIN);
    nrf_gpio_pin_set(AGSYS_MEM_FRAM_CS);
    nrf_gpio_pin_set(AGSYS_MEM_FLASH_CS);

    m_initialized = true;
    SEGGER_RTT_printf(0, "SPI initialized (2 buses)\n");
    return true;
}

bool spi_acquire(TickType_t timeout)
{
    if (g_spi_mutex == NULL) {
        return true;  /* No mutex, assume single-threaded init */
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

/* Get the appropriate SPI instance for a given CS pin */
static const nrf_drv_spi_t* get_spi_for_cs(uint8_t cs_pin)
{
    if (cs_pin == AGSYS_MEM_FRAM_CS || cs_pin == AGSYS_MEM_FLASH_CS) {
        return &m_spi_mem;
    }
    return &m_spi_periph;  /* Default to peripheral bus (CAN, LoRa) */
}

/* Track current CS for raw transfers */
static uint8_t m_current_cs = 0;

bool spi_transfer_raw(const uint8_t *tx_buf, uint8_t *rx_buf, size_t len)
{
    const nrf_drv_spi_t *spi = get_spi_for_cs(m_current_cs);
    ret_code_t err = nrf_drv_spi_transfer(spi, tx_buf, len, rx_buf, len);
    return (err == NRF_SUCCESS);
}

bool spi_transfer(uint8_t cs_pin, const uint8_t *tx_buf, uint8_t *rx_buf, size_t len)
{
    m_current_cs = cs_pin;
    const nrf_drv_spi_t *spi = get_spi_for_cs(cs_pin);
    spi_cs_assert(cs_pin);
    ret_code_t err = nrf_drv_spi_transfer(spi, tx_buf, len, rx_buf, len);
    spi_cs_deassert(cs_pin);
    return (err == NRF_SUCCESS);
}
