/**
 * @file bl_hal_nrf52.c
 * @brief Bootloader HAL implementation for nRF52832
 *
 * Bare-metal implementation - no Nordic SDK, no FreeRTOS.
 * Direct register access for GPIO, SPI, and NVMC.
 */

#include "bl_hal.h"
#include <string.h>

/*******************************************************************************
 * nRF52832 Register Definitions
 ******************************************************************************/

/* Base addresses */
#define NRF_GPIO_BASE       0x50000000UL
#define NRF_SPIM0_BASE      0x40003000UL
#define NRF_NVMC_BASE       0x4001E000UL

/* GPIO registers */
typedef struct {
    volatile uint32_t RESERVED0[321];
    volatile uint32_t OUT;
    volatile uint32_t OUTSET;
    volatile uint32_t OUTCLR;
    volatile uint32_t IN;
    volatile uint32_t DIR;
    volatile uint32_t DIRSET;
    volatile uint32_t DIRCLR;
    volatile uint32_t LATCH;
    volatile uint32_t DETECTMODE;
    volatile uint32_t RESERVED1[118];
    volatile uint32_t PIN_CNF[32];
} NRF_GPIO_Type;

#define NRF_GPIO    ((NRF_GPIO_Type *)NRF_GPIO_BASE)

/* GPIO PIN_CNF bits */
#define GPIO_PIN_CNF_DIR_Output         (1UL << 0)
#define GPIO_PIN_CNF_DIR_Input          (0UL << 0)
#define GPIO_PIN_CNF_INPUT_Connect      (0UL << 1)
#define GPIO_PIN_CNF_INPUT_Disconnect   (1UL << 1)
#define GPIO_PIN_CNF_PULL_Disabled      (0UL << 2)
#define GPIO_PIN_CNF_PULL_Pulldown      (1UL << 2)
#define GPIO_PIN_CNF_PULL_Pullup        (3UL << 2)
#define GPIO_PIN_CNF_DRIVE_S0S1         (0UL << 8)
#define GPIO_PIN_CNF_DRIVE_H0S1         (1UL << 8)
#define GPIO_PIN_CNF_DRIVE_S0H1         (2UL << 8)
#define GPIO_PIN_CNF_DRIVE_H0H1         (3UL << 8)

/* SPIM registers */
typedef struct {
    volatile uint32_t RESERVED0[4];
    volatile uint32_t TASKS_START;
    volatile uint32_t TASKS_STOP;
    volatile uint32_t RESERVED1;
    volatile uint32_t TASKS_SUSPEND;
    volatile uint32_t TASKS_RESUME;
    volatile uint32_t RESERVED2[56];
    volatile uint32_t EVENTS_STOPPED;
    volatile uint32_t RESERVED3[2];
    volatile uint32_t EVENTS_ENDRX;
    volatile uint32_t RESERVED4;
    volatile uint32_t EVENTS_END;
    volatile uint32_t RESERVED5;
    volatile uint32_t EVENTS_ENDTX;
    volatile uint32_t RESERVED6[10];
    volatile uint32_t EVENTS_STARTED;
    volatile uint32_t RESERVED7[44];
    volatile uint32_t SHORTS;
    volatile uint32_t RESERVED8[64];
    volatile uint32_t INTENSET;
    volatile uint32_t INTENCLR;
    volatile uint32_t RESERVED9[125];
    volatile uint32_t ENABLE;
    volatile uint32_t RESERVED10;
    struct {
        volatile uint32_t SCK;
        volatile uint32_t MOSI;
        volatile uint32_t MISO;
    } PSEL;
    volatile uint32_t RESERVED11[4];
    volatile uint32_t FREQUENCY;
    volatile uint32_t RESERVED12[3];
    struct {
        volatile uint32_t PTR;
        volatile uint32_t MAXCNT;
        volatile uint32_t AMOUNT;
        volatile uint32_t LIST;
    } RXD;
    struct {
        volatile uint32_t PTR;
        volatile uint32_t MAXCNT;
        volatile uint32_t AMOUNT;
        volatile uint32_t LIST;
    } TXD;
    volatile uint32_t CONFIG;
    volatile uint32_t RESERVED13[26];
    volatile uint32_t ORC;
} NRF_SPIM_Type;

#define NRF_SPIM0   ((NRF_SPIM_Type *)NRF_SPIM0_BASE)

/* SPIM constants */
#define SPIM_ENABLE_Enabled         7
#define SPIM_ENABLE_Disabled        0
#define SPIM_FREQUENCY_M4           0x40000000UL  /* 4 MHz */
#define SPIM_CONFIG_ORDER_MsbFirst  (0UL << 0)
#define SPIM_CONFIG_CPHA_Leading    (0UL << 1)
#define SPIM_CONFIG_CPOL_ActiveHigh (0UL << 2)

/* NVMC registers */
typedef struct {
    volatile uint32_t RESERVED0[256];
    volatile uint32_t READY;
    volatile uint32_t RESERVED1[64];
    volatile uint32_t CONFIG;
    volatile uint32_t ERASEPAGE;
    volatile uint32_t ERASEALL;
    volatile uint32_t ERASEPCR0;
    volatile uint32_t ERASEUICR;
    volatile uint32_t RESERVED2[10];
    volatile uint32_t ICACHECNF;
    volatile uint32_t RESERVED3;
    volatile uint32_t IHIT;
    volatile uint32_t IMISS;
} NRF_NVMC_Type;

#define NRF_NVMC    ((NRF_NVMC_Type *)NRF_NVMC_BASE)

#define NVMC_CONFIG_WEN_Ren     0   /* Read enable */
#define NVMC_CONFIG_WEN_Wen     1   /* Write enable */
#define NVMC_CONFIG_WEN_Een     2   /* Erase enable */

/*******************************************************************************
 * Static Variables
 ******************************************************************************/

static uint8_t spi_tx_dummy[256];
static uint8_t spi_rx_dummy[256];

/*******************************************************************************
 * GPIO Helpers
 ******************************************************************************/

static void gpio_cfg_output(uint32_t pin)
{
    NRF_GPIO->PIN_CNF[pin] = GPIO_PIN_CNF_DIR_Output |
                             GPIO_PIN_CNF_INPUT_Disconnect |
                             GPIO_PIN_CNF_PULL_Disabled |
                             GPIO_PIN_CNF_DRIVE_S0S1;
}

static void gpio_cfg_input(uint32_t pin)
{
    NRF_GPIO->PIN_CNF[pin] = GPIO_PIN_CNF_DIR_Input |
                             GPIO_PIN_CNF_INPUT_Connect |
                             GPIO_PIN_CNF_PULL_Disabled;
}

static void gpio_set(uint32_t pin)
{
    NRF_GPIO->OUTSET = (1UL << pin);
}

static void gpio_clear(uint32_t pin)
{
    NRF_GPIO->OUTCLR = (1UL << pin);
}

/*******************************************************************************
 * Initialization
 ******************************************************************************/

void bl_hal_init(void)
{
    /* Initialize dummy buffers */
    memset(spi_tx_dummy, 0xFF, sizeof(spi_tx_dummy));
    
    /* Configure LED */
    gpio_cfg_output(BL_PIN_LED);
    gpio_clear(BL_PIN_LED);
    
    /* Configure SPI pins */
    gpio_cfg_output(BL_PIN_SPI_SCK);
    gpio_cfg_output(BL_PIN_SPI_MOSI);
    gpio_cfg_input(BL_PIN_SPI_MISO);
    
    /* Configure chip selects (active low, start deselected) */
    gpio_cfg_output(BL_PIN_FRAM_CS);
    gpio_set(BL_PIN_FRAM_CS);
    gpio_cfg_output(BL_PIN_FLASH_CS);
    gpio_set(BL_PIN_FLASH_CS);
    
    /* Configure SPIM0 */
    NRF_SPIM0->ENABLE = SPIM_ENABLE_Disabled;
    
    NRF_SPIM0->PSEL.SCK = BL_PIN_SPI_SCK;
    NRF_SPIM0->PSEL.MOSI = BL_PIN_SPI_MOSI;
    NRF_SPIM0->PSEL.MISO = BL_PIN_SPI_MISO;
    
    NRF_SPIM0->FREQUENCY = SPIM_FREQUENCY_M4;
    NRF_SPIM0->CONFIG = SPIM_CONFIG_ORDER_MsbFirst |
                        SPIM_CONFIG_CPHA_Leading |
                        SPIM_CONFIG_CPOL_ActiveHigh;
    NRF_SPIM0->ORC = 0xFF;  /* Output 0xFF when no TX data */
    
    NRF_SPIM0->ENABLE = SPIM_ENABLE_Enabled;
}

/*******************************************************************************
 * LED Functions
 ******************************************************************************/

void bl_led_set(bool on)
{
    if (on) {
        gpio_set(BL_PIN_LED);
    } else {
        gpio_clear(BL_PIN_LED);
    }
}

void bl_led_toggle(void)
{
    if (NRF_GPIO->OUT & (1UL << BL_PIN_LED)) {
        gpio_clear(BL_PIN_LED);
    } else {
        gpio_set(BL_PIN_LED);
    }
}

/*******************************************************************************
 * Delay Functions
 ******************************************************************************/

void bl_delay_ms(uint32_t ms)
{
    /* nRF52832 runs at 64 MHz
     * Approximate delay using busy loop
     * Tuned empirically - adjust if needed
     */
    volatile uint32_t count = ms * 8000;
    while (count--) {
        __asm__ volatile ("nop");
    }
}

/*******************************************************************************
 * SPI Functions
 ******************************************************************************/

void bl_fram_select(void)
{
    gpio_clear(BL_PIN_FRAM_CS);
}

void bl_fram_deselect(void)
{
    gpio_set(BL_PIN_FRAM_CS);
}

void bl_flash_select(void)
{
    gpio_clear(BL_PIN_FLASH_CS);
}

void bl_flash_deselect(void)
{
    gpio_set(BL_PIN_FLASH_CS);
}

void bl_spi_transfer(const uint8_t *tx_buf, uint8_t *rx_buf, size_t len)
{
    if (len == 0) return;
    
    /* Use dummy buffers if NULL */
    if (tx_buf == NULL) {
        tx_buf = spi_tx_dummy;
        if (len > sizeof(spi_tx_dummy)) {
            len = sizeof(spi_tx_dummy);
        }
    }
    if (rx_buf == NULL) {
        rx_buf = spi_rx_dummy;
        if (len > sizeof(spi_rx_dummy)) {
            len = sizeof(spi_rx_dummy);
        }
    }
    
    /* Configure DMA */
    NRF_SPIM0->TXD.PTR = (uint32_t)tx_buf;
    NRF_SPIM0->TXD.MAXCNT = len;
    NRF_SPIM0->RXD.PTR = (uint32_t)rx_buf;
    NRF_SPIM0->RXD.MAXCNT = len;
    
    /* Clear events */
    NRF_SPIM0->EVENTS_END = 0;
    
    /* Start transfer */
    NRF_SPIM0->TASKS_START = 1;
    
    /* Wait for completion */
    while (NRF_SPIM0->EVENTS_END == 0) {
        /* Busy wait */
    }
    
    NRF_SPIM0->EVENTS_END = 0;
}

/*******************************************************************************
 * NVMC (Internal Flash) Functions
 ******************************************************************************/

void bl_nvmc_erase_page(uint32_t page_addr)
{
    /* Enable erase */
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een;
    while (NRF_NVMC->READY == 0);
    
    /* Erase page */
    NRF_NVMC->ERASEPAGE = page_addr;
    while (NRF_NVMC->READY == 0);
    
    /* Return to read mode */
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
    while (NRF_NVMC->READY == 0);
}

void bl_nvmc_write(uint32_t addr, const uint8_t *data, size_t len)
{
    /* Enable write */
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
    while (NRF_NVMC->READY == 0);
    
    /* Write word by word */
    const uint32_t *src = (const uint32_t *)data;
    volatile uint32_t *dst = (volatile uint32_t *)addr;
    size_t words = len / 4;
    
    for (size_t i = 0; i < words; i++) {
        *dst++ = *src++;
        while (NRF_NVMC->READY == 0);
    }
    
    /* Return to read mode */
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
    while (NRF_NVMC->READY == 0);
}

/*******************************************************************************
 * Boot Functions
 ******************************************************************************/

void bl_jump_to_app(void)
{
    /* Disable SPIM */
    NRF_SPIM0->ENABLE = SPIM_ENABLE_Disabled;
    
    /* Get application's vector table */
    uint32_t *app_vector = (uint32_t *)BL_FLASH_APP_ADDR;
    
    /* Get initial stack pointer and reset handler */
    uint32_t app_sp = app_vector[0];
    uint32_t app_reset = app_vector[1];
    
    /* Set stack pointer */
    __asm__ volatile ("MSR MSP, %0" : : "r" (app_sp));
    
    /* Jump to application reset handler */
    void (*app_entry)(void) = (void (*)(void))app_reset;
    app_entry();
    
    /* Should never reach here */
    while (1);
}

void bl_panic(void)
{
    /* SOS pattern: ... --- ... */
    while (1) {
        /* S: three short */
        for (int i = 0; i < 3; i++) {
            bl_led_set(true);
            bl_delay_ms(100);
            bl_led_set(false);
            bl_delay_ms(100);
        }
        bl_delay_ms(200);
        
        /* O: three long */
        for (int i = 0; i < 3; i++) {
            bl_led_set(true);
            bl_delay_ms(300);
            bl_led_set(false);
            bl_delay_ms(100);
        }
        bl_delay_ms(200);
        
        /* S: three short */
        for (int i = 0; i < 3; i++) {
            bl_led_set(true);
            bl_delay_ms(100);
            bl_led_set(false);
            bl_delay_ms(100);
        }
        
        /* Long pause */
        bl_delay_ms(1000);
    }
}
