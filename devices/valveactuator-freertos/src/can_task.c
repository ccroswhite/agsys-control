/**
 * @file can_task.c
 * @brief CAN bus task implementation
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include "nrf.h"
#include "nrfx_gpiote.h"
#include "SEGGER_RTT.h"

#include "can_task.h"
#include "valve_task.h"
#include "spi_driver.h"
#include "board_config.h"

#include <string.h>

/* ==========================================================================
 * MCP2515 DEFINITIONS
 * ========================================================================== */

/* MCP2515 SPI Commands */
#define MCP_RESET           0xC0
#define MCP_READ            0x03
#define MCP_WRITE           0x02
#define MCP_RTS_TX0         0x81
#define MCP_RTS_TX1         0x82
#define MCP_RTS_TX2         0x84
#define MCP_READ_STATUS     0xA0
#define MCP_RX_STATUS       0xB0
#define MCP_BIT_MODIFY      0x05
#define MCP_READ_RX0        0x90
#define MCP_READ_RX1        0x94

/* MCP2515 Registers */
#define MCP_CANSTAT         0x0E
#define MCP_CANCTRL         0x0F
#define MCP_CNF3            0x28
#define MCP_CNF2            0x29
#define MCP_CNF1            0x2A
#define MCP_CANINTE         0x2B
#define MCP_CANINTF         0x2C
#define MCP_TXB0CTRL        0x30
#define MCP_TXB0SIDH        0x31
#define MCP_TXB0D0          0x36
#define MCP_RXB0CTRL        0x60
#define MCP_RXB0SIDH        0x61
#define MCP_RXB0D0          0x66

/* MCP2515 Modes */
#define MCP_MODE_NORMAL     0x00
#define MCP_MODE_SLEEP      0x20
#define MCP_MODE_LOOPBACK   0x40
#define MCP_MODE_LISTEN     0x60
#define MCP_MODE_CONFIG     0x80

/* Interrupt flags */
#define MCP_RX0IF           0x01
#define MCP_RX1IF           0x02
#define MCP_TX0IF           0x04

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static TaskHandle_t m_can_task_handle = NULL;
static uint8_t m_device_address = 0;

/* CAN frame structure */
typedef struct {
    uint16_t id;
    uint8_t dlc;
    uint8_t data[8];
} can_frame_t;

/* ==========================================================================
 * MCP2515 LOW-LEVEL FUNCTIONS
 * ========================================================================== */

static void mcp_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx[3] = { MCP_WRITE, reg, value };
    spi_transfer(SPI_CS_CAN_PIN, tx, NULL, 3);
}

static uint8_t mcp_read_reg(uint8_t reg)
{
    uint8_t tx[3] = { MCP_READ, reg, 0x00 };
    uint8_t rx[3];
    spi_transfer(SPI_CS_CAN_PIN, tx, rx, 3);
    return rx[2];
}

static void mcp_bit_modify(uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t tx[4] = { MCP_BIT_MODIFY, reg, mask, value };
    spi_transfer(SPI_CS_CAN_PIN, tx, NULL, 4);
}

static void mcp_reset(void)
{
    uint8_t cmd = MCP_RESET;
    spi_transfer(SPI_CS_CAN_PIN, &cmd, NULL, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

static bool mcp_set_mode(uint8_t mode)
{
    mcp_bit_modify(MCP_CANCTRL, 0xE0, mode);
    
    /* Wait for mode change */
    for (int i = 0; i < 10; i++) {
        if ((mcp_read_reg(MCP_CANSTAT) & 0xE0) == mode) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return false;
}

static void mcp_init(void)
{
    mcp_reset();

    /* Enter config mode */
    mcp_set_mode(MCP_MODE_CONFIG);

    /* Configure bit timing for 1 Mbps with 16 MHz crystal
     * TQ = 2/Fosc = 125ns
     * Sync = 1 TQ
     * Prop = 1 TQ
     * PS1 = 3 TQ
     * PS2 = 3 TQ
     * Total = 8 TQ = 1 µs = 1 Mbps
     */
    mcp_write_reg(MCP_CNF1, 0x00);  /* BRP = 0, SJW = 1 */
    mcp_write_reg(MCP_CNF2, 0x90);  /* BTLMODE=1, SAM=0, PHSEG1=2, PRSEG=0 */
    mcp_write_reg(MCP_CNF3, 0x02);  /* PHSEG2=2 */

    /* Configure RX buffers - receive all messages */
    mcp_write_reg(MCP_RXB0CTRL, 0x60);  /* RXM=11 (any message), BUKT=0 */

    /* Enable RX interrupts */
    mcp_write_reg(MCP_CANINTE, MCP_RX0IF | MCP_RX1IF);

    /* Clear interrupt flags */
    mcp_write_reg(MCP_CANINTF, 0x00);

    /* Enter normal mode */
    mcp_set_mode(MCP_MODE_NORMAL);

    SEGGER_RTT_printf(0, "MCP2515 initialized (1 Mbps)\n");
}

static bool mcp_read_message(can_frame_t *frame)
{
    uint8_t status = mcp_read_reg(MCP_CANINTF);
    
    if (status & MCP_RX0IF) {
        /* Read from RX buffer 0 */
        uint8_t tx[14] = { MCP_READ_RX0 };
        uint8_t rx[14];
        spi_transfer(SPI_CS_CAN_PIN, tx, rx, 14);

        /* Parse ID (standard 11-bit) */
        frame->id = ((uint16_t)rx[1] << 3) | (rx[2] >> 5);
        frame->dlc = rx[5] & 0x0F;
        if (frame->dlc > 8) frame->dlc = 8;
        memcpy(frame->data, &rx[6], frame->dlc);

        /* Clear interrupt flag */
        mcp_bit_modify(MCP_CANINTF, MCP_RX0IF, 0x00);
        return true;
    }

    return false;
}

static bool mcp_send_message(const can_frame_t *frame)
{
    /* Wait for TX buffer to be free */
    for (int i = 0; i < 10; i++) {
        if ((mcp_read_reg(MCP_TXB0CTRL) & 0x08) == 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    /* Load TX buffer */
    uint8_t sidh = (frame->id >> 3) & 0xFF;
    uint8_t sidl = (frame->id << 5) & 0xE0;

    mcp_write_reg(MCP_TXB0SIDH, sidh);
    mcp_write_reg(MCP_TXB0SIDH + 1, sidl);
    mcp_write_reg(MCP_TXB0SIDH + 2, 0);  /* EID8 */
    mcp_write_reg(MCP_TXB0SIDH + 3, 0);  /* EID0 */
    mcp_write_reg(MCP_TXB0SIDH + 4, frame->dlc);

    for (int i = 0; i < frame->dlc; i++) {
        mcp_write_reg(MCP_TXB0D0 + i, frame->data[i]);
    }

    /* Request to send */
    uint8_t cmd = MCP_RTS_TX0;
    spi_transfer(SPI_CS_CAN_PIN, &cmd, NULL, 1);

    return true;
}

/* ==========================================================================
 * INTERRUPT HANDLER
 * ========================================================================== */

static void can_int_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (m_can_task_handle != NULL) {
        vTaskNotifyGiveFromISR(m_can_task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* ==========================================================================
 * CAN TASK
 * ========================================================================== */

void can_task(void *pvParameters)
{
    m_device_address = (uint8_t)(uintptr_t)pvParameters;
    m_can_task_handle = xTaskGetCurrentTaskHandle();

    SEGGER_RTT_printf(0, "CAN task started (addr=%d)\n", m_device_address);

    /* Initialize SPI (shared with FRAM) */
    spi_init();

    /* Initialize MCP2515 */
    mcp_init();

    /* Configure interrupt on CAN_INT pin (falling edge) */
    if (!nrfx_gpiote_is_init()) {
        nrfx_gpiote_init();
    }

    nrfx_gpiote_in_config_t int_config = NRFX_GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    int_config.pull = NRF_GPIO_PIN_PULLUP;
    nrfx_gpiote_in_init(CAN_INT_PIN, &int_config, can_int_handler);
    nrfx_gpiote_in_event_enable(CAN_INT_PIN, true);

    can_frame_t frame;

    for (;;) {
        /* Wait for interrupt notification or timeout */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));

        /* Process all pending messages */
        while (mcp_read_message(&frame)) {
            SEGGER_RTT_printf(0, "CAN RX: ID=0x%03X, DLC=%d\n", frame.id, frame.dlc);

            switch (frame.id) {
                case (CAN_BASE_ID + CAN_CMD_OPEN):
                    if (frame.dlc >= 1 && frame.data[0] == m_device_address) {
                        SEGGER_RTT_printf(0, "CMD: OPEN\n");
                        valve_request_open();
                        can_send_status();
                    }
                    break;

                case (CAN_BASE_ID + CAN_CMD_CLOSE):
                    if (frame.dlc >= 1 && frame.data[0] == m_device_address) {
                        SEGGER_RTT_printf(0, "CMD: CLOSE\n");
                        valve_request_close();
                        can_send_status();
                    }
                    break;

                case (CAN_BASE_ID + CAN_CMD_STOP):
                    if (frame.dlc >= 1 && frame.data[0] == m_device_address) {
                        SEGGER_RTT_printf(0, "CMD: STOP\n");
                        valve_request_stop();
                        can_send_status();
                    }
                    break;

                case (CAN_BASE_ID + CAN_CMD_STATUS):
                    if (frame.dlc >= 1 && frame.data[0] == m_device_address) {
                        SEGGER_RTT_printf(0, "CMD: QUERY\n");
                        can_send_status();
                    }
                    break;

                case (CAN_BASE_ID + CAN_CMD_EMERGENCY):
                    SEGGER_RTT_printf(0, "CMD: EMERGENCY CLOSE\n");
                    valve_request_emergency_close();
                    can_send_status();
                    break;

                case CAN_ID_DISCOVER:
                    /* Broadcast discovery - all actuators respond with staggered timing */
                    SEGGER_RTT_printf(0, "CMD: DISCOVER BROADCAST\n");
                    /* Delay based on address to avoid collisions */
                    vTaskDelay(pdMS_TO_TICKS(m_device_address * CAN_DISCOVERY_DELAY_MS));
                    can_send_discovery_response();
                    break;

                case CAN_ID_EMERGENCY:
                    /* Broadcast emergency close - no address check */
                    SEGGER_RTT_printf(0, "CMD: BROADCAST EMERGENCY CLOSE\n");
                    valve_request_emergency_close();
                    can_send_status();
                    break;
            }
        }
    }
}

/* ==========================================================================
 * PUBLIC FUNCTIONS
 * ========================================================================== */

void can_send_status(void)
{
    can_frame_t frame;
    frame.id = CAN_BASE_ID + 0x80 + m_device_address;  /* Status response */
    frame.dlc = 4;
    frame.data[0] = valve_get_status_flags();
    
    uint16_t current = valve_get_current_ma();
    frame.data[1] = (current >> 8) & 0xFF;
    frame.data[2] = current & 0xFF;
    frame.data[3] = 0;

    if (!mcp_send_message(&frame)) {
        SEGGER_RTT_printf(0, "Failed to send status\n");
    }
}

void can_send_uid(void)
{
    /* Get device UID from FICR */
    uint32_t uid0 = NRF_FICR->DEVICEID[0];
    uint32_t uid1 = NRF_FICR->DEVICEID[1];

    can_frame_t frame;
    frame.id = CAN_BASE_ID + 0x90 + m_device_address;  /* UID response */
    frame.dlc = 8;
    frame.data[0] = (uid0 >> 24) & 0xFF;
    frame.data[1] = (uid0 >> 16) & 0xFF;
    frame.data[2] = (uid0 >> 8) & 0xFF;
    frame.data[3] = uid0 & 0xFF;
    frame.data[4] = (uid1 >> 24) & 0xFF;
    frame.data[5] = (uid1 >> 16) & 0xFF;
    frame.data[6] = (uid1 >> 8) & 0xFF;
    frame.data[7] = uid1 & 0xFF;

    SEGGER_RTT_printf(0, "Sending UID: %08X%08X\n", uid0, uid1);

    if (!mcp_send_message(&frame)) {
        SEGGER_RTT_printf(0, "Failed to send UID\n");
    }
}

void can_send_discovery_response(void)
{
    /* Discovery response includes: address (1 byte) + UID (8 bytes) 
     * Response CAN ID: 0x1F0 + 1 = 0x1F1 (discovery response)
     * This allows controller to distinguish discovery responses from other messages
     */
    uint32_t uid0 = NRF_FICR->DEVICEID[0];
    uint32_t uid1 = NRF_FICR->DEVICEID[1];

    can_frame_t frame;
    frame.id = CAN_ID_DISCOVER + 1;  /* 0x1F1 = Discovery response */
    frame.dlc = 8;
    
    /* Byte 0: CAN bus address */
    frame.data[0] = m_device_address;
    
    /* Bytes 1-7: First 7 bytes of UID (enough to be unique) */
    frame.data[1] = (uid0 >> 0) & 0xFF;
    frame.data[2] = (uid0 >> 8) & 0xFF;
    frame.data[3] = (uid0 >> 16) & 0xFF;
    frame.data[4] = (uid0 >> 24) & 0xFF;
    frame.data[5] = (uid1 >> 0) & 0xFF;
    frame.data[6] = (uid1 >> 8) & 0xFF;
    frame.data[7] = (uid1 >> 16) & 0xFF;

    SEGGER_RTT_printf(0, "Discovery response: addr=%d UID=%02X%02X%02X%02X...\n", 
                      m_device_address, frame.data[1], frame.data[2], 
                      frame.data[3], frame.data[4]);

    if (!mcp_send_message(&frame)) {
        SEGGER_RTT_printf(0, "Failed to send discovery response\n");
    }
}

TaskHandle_t can_get_task_handle(void)
{
    return m_can_task_handle;
}
