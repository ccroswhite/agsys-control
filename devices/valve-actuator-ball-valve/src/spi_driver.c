/**
 * @file spi_driver.c
 * @brief Simple SPI driver implementation
 */

#include "sdk_config.h"
#include "spi_driver.h"
#include "board_config.h"

#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "SEGGER_RTT.h"

/* SPI Bus 0 - CAN (MCP2515) */
static const nrf_drv_spi_t m_spi_can = NRF_DRV_SPI_INSTANCE(0);
/* SPI Bus 1 - Memory (FRAM + Flash) - uses standard pins from agsys_pins.h */
static const nrf_drv_spi_t m_spi_mem = NRF_DRV_SPI_INSTANCE(1);
static bool m_initialized = false;

bool spi_init(void)
{
    if (m_initialized) {
        return true;
    }

    /* Initialize SPI Bus 0 - CAN */
    nrf_drv_spi_config_t spi_can_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_can_config.sck_pin   = SPI_CAN_SCK_PIN;
    spi_can_config.mosi_pin  = SPI_CAN_MOSI_PIN;
    spi_can_config.miso_pin  = SPI_CAN_MISO_PIN;
    spi_can_config.ss_pin    = NRF_DRV_SPI_PIN_NOT_USED;
    spi_can_config.frequency = NRF_DRV_SPI_FREQ_4M;
    spi_can_config.mode      = NRF_DRV_SPI_MODE_0;
    spi_can_config.bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST;

    ret_code_t err = nrf_drv_spi_init(&m_spi_can, &spi_can_config, NULL, NULL);
    if (err != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "SPI CAN init failed: %d\n", err);
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
    nrf_gpio_cfg_output(AGSYS_MEM_FRAM_CS);
    nrf_gpio_cfg_output(AGSYS_MEM_FLASH_CS);
    nrf_gpio_pin_set(SPI_CS_CAN_PIN);
    nrf_gpio_pin_set(AGSYS_MEM_FRAM_CS);
    nrf_gpio_pin_set(AGSYS_MEM_FLASH_CS);

    m_initialized = true;
    SEGGER_RTT_printf(0, "SPI initialized (2 buses)\n");
    return true;
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
    return &m_spi_can;  /* Default to CAN bus */
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
