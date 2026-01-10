/**
 * @file can_task.c
 * @brief CAN bus task implementation for Valve Controller
 * 
 * Manages MCP2515 CAN controller to communicate with up to 64 valve actuators.
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrfx_gpiote.h"
#include "SEGGER_RTT.h"

#include "can_task.h"
#include "spi_driver.h"
#include "board_config.h"

#include <string.h>

/* ==========================================================================
 * MCP2515 DEFINITIONS
 * ========================================================================== */

/* SPI Commands */
#define MCP_RESET           0xC0
#define MCP_READ            0x03
#define MCP_WRITE           0x02
#define MCP_RTS_TX0         0x81
#define MCP_READ_STATUS     0xA0
#define MCP_RX_STATUS       0xB0
#define MCP_BIT_MODIFY      0x05
#define MCP_READ_RX0        0x90

/* Registers */
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

/* Modes */
#define MCP_MODE_NORMAL     0x00
#define MCP_MODE_CONFIG     0x80

/* Interrupt flags */
#define MCP_RX0IF           0x01
#define MCP_RX1IF           0x02

/* CAN IDs for actuator communication */
#define CAN_ID_CMD_BASE         0x100
#define CAN_ID_STATUS_BASE      0x180
#define CAN_ID_UID_RESP_BASE    0x190
#define CAN_ID_DISCOVER         0x1F0
#define CAN_ID_DISCOVER_RESP    0x1F1   /* Discovery response from actuators */
#define CAN_ID_EMERGENCY        0x1FF

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static actuator_status_t m_actuators[ACTUATOR_ADDR_MAX + 1];
static QueueHandle_t m_cmd_queue = NULL;
static TaskHandle_t m_task_handle = NULL;

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
    mcp_set_mode(MCP_MODE_CONFIG);

    /* Configure for 1 Mbps with 16 MHz crystal */
    mcp_write_reg(MCP_CNF1, 0x00);
    mcp_write_reg(MCP_CNF2, 0x90);
    mcp_write_reg(MCP_CNF3, 0x02);

    /* Receive all messages */
    mcp_write_reg(MCP_RXB0CTRL, 0x60);

    /* Enable RX interrupts */
    mcp_write_reg(MCP_CANINTE, MCP_RX0IF | MCP_RX1IF);
    mcp_write_reg(MCP_CANINTF, 0x00);

    mcp_set_mode(MCP_MODE_NORMAL);
    SEGGER_RTT_printf(0, "MCP2515 initialized\n");
}

static bool mcp_read_message(can_frame_t *frame)
{
    uint8_t status = mcp_read_reg(MCP_CANINTF);
    
    if (status & MCP_RX0IF) {
        uint8_t tx[14] = { MCP_READ_RX0 };
        uint8_t rx[14];
        spi_transfer(SPI_CS_CAN_PIN, tx, rx, 14);

        frame->id = ((uint16_t)rx[1] << 3) | (rx[2] >> 5);
        frame->dlc = rx[5] & 0x0F;
        if (frame->dlc > 8) frame->dlc = 8;
        memcpy(frame->data, &rx[6], frame->dlc);

        mcp_bit_modify(MCP_CANINTF, MCP_RX0IF, 0x00);
        return true;
    }
    return false;
}

static bool mcp_send_message(const can_frame_t *frame)
{
    /* Wait for TX buffer */
    for (int i = 0; i < 10; i++) {
        if ((mcp_read_reg(MCP_TXB0CTRL) & 0x08) == 0) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    uint8_t sidh = (frame->id >> 3) & 0xFF;
    uint8_t sidl = (frame->id << 5) & 0xE0;

    mcp_write_reg(MCP_TXB0SIDH, sidh);
    mcp_write_reg(MCP_TXB0SIDH + 1, sidl);
    mcp_write_reg(MCP_TXB0SIDH + 2, 0);
    mcp_write_reg(MCP_TXB0SIDH + 3, 0);
    mcp_write_reg(MCP_TXB0SIDH + 4, frame->dlc);

    for (int i = 0; i < frame->dlc; i++) {
        mcp_write_reg(MCP_TXB0D0 + i, frame->data[i]);
    }

    uint8_t cmd = MCP_RTS_TX0;
    spi_transfer(SPI_CS_CAN_PIN, &cmd, NULL, 1);
    return true;
}

/* ==========================================================================
 * ACTUATOR MANAGEMENT
 * ========================================================================== */

static void process_status_response(uint8_t address, const uint8_t *data, uint8_t len)
{
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) return;
    
    actuator_status_t *act = &m_actuators[address];
    act->online = true;
    act->last_seen = xTaskGetTickCount();
    
    if (len >= 1) act->status_flags = data[0];
    if (len >= 3) act->current_ma = ((uint16_t)data[1] << 8) | data[2];
}

static void process_uid_response(uint8_t address, const uint8_t *data, uint8_t len)
{
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) return;
    if (len < 8) return;
    
    actuator_status_t *act = &m_actuators[address];
    act->online = true;
    act->uid_known = true;
    act->last_seen = xTaskGetTickCount();
    memcpy(act->uid, data, 8);
    
    SEGGER_RTT_printf(0, "Actuator %d UID: %02X%02X%02X%02X%02X%02X%02X%02X\n",
                      address, data[0], data[1], data[2], data[3],
                      data[4], data[5], data[6], data[7]);
}

static void process_discovery_response(const uint8_t *data, uint8_t len)
{
    /* Discovery response format: [addr][uid0-6] */
    if (len < 8) return;
    
    uint8_t address = data[0];
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) return;
    
    actuator_status_t *act = &m_actuators[address];
    act->online = true;
    act->uid_known = true;
    act->last_seen = xTaskGetTickCount();
    
    /* Copy 7 bytes of UID, pad last byte with 0 */
    memcpy(act->uid, &data[1], 7);
    act->uid[7] = 0;
    
    SEGGER_RTT_printf(0, "Discovery: addr=%d UID=%02X%02X%02X%02X%02X%02X%02X\n",
                      address, act->uid[0], act->uid[1], act->uid[2], act->uid[3],
                      act->uid[4], act->uid[5], act->uid[6]);
}

static void process_can_message(const can_frame_t *frame)
{
    /* Discovery response: 0x1F1 */
    if (frame->id == CAN_ID_DISCOVER_RESP) {
        process_discovery_response(frame->data, frame->dlc);
    }
    /* Status response: 0x180 + address */
    else if (frame->id >= CAN_ID_STATUS_BASE && frame->id < CAN_ID_STATUS_BASE + 0x40) {
        uint8_t addr = frame->id - CAN_ID_STATUS_BASE;
        process_status_response(addr, frame->data, frame->dlc);
    }
    /* UID response: 0x190 + address */
    else if (frame->id >= CAN_ID_UID_RESP_BASE && frame->id < CAN_ID_UID_RESP_BASE + 0x40) {
        uint8_t addr = frame->id - CAN_ID_UID_RESP_BASE;
        process_uid_response(addr, frame->data, frame->dlc);
    }
}

static void send_valve_command(uint8_t address, uint8_t cmd)
{
    can_frame_t frame;
    frame.id = CAN_ID_CMD_BASE + cmd;
    frame.dlc = 1;
    frame.data[0] = address;
    
    if (spi_acquire(pdMS_TO_TICKS(100))) {
        mcp_send_message(&frame);
        spi_release();
    }
}

static void send_discover_broadcast(void)
{
    can_frame_t frame;
    frame.id = CAN_ID_DISCOVER;
    frame.dlc = 0;
    
    if (spi_acquire(pdMS_TO_TICKS(100))) {
        mcp_send_message(&frame);
        spi_release();
    }
}

static void send_emergency_close(void)
{
    can_frame_t frame;
    frame.id = CAN_ID_EMERGENCY;
    frame.dlc = 0;
    
    if (spi_acquire(pdMS_TO_TICKS(100))) {
        mcp_send_message(&frame);
        spi_release();
    }
    
    SEGGER_RTT_printf(0, "EMERGENCY CLOSE broadcast sent\n");
}

/* ==========================================================================
 * INTERRUPT HANDLER
 * ========================================================================== */

static void can_int_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (m_task_handle != NULL) {
        vTaskNotifyGiveFromISR(m_task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* ==========================================================================
 * CAN TASK
 * ========================================================================== */

bool can_task_init(void)
{
    m_cmd_queue = xQueueCreate(16, sizeof(can_command_t));
    if (m_cmd_queue == NULL) {
        SEGGER_RTT_printf(0, "Failed to create CAN command queue\n");
        return false;
    }
    
    /* Initialize actuator array */
    memset(m_actuators, 0, sizeof(m_actuators));
    
    return true;
}

void can_task(void *pvParameters)
{
    (void)pvParameters;
    m_task_handle = xTaskGetCurrentTaskHandle();
    
    SEGGER_RTT_printf(0, "CAN task started\n");
    
    /* Initialize SPI */
    spi_init();
    
    /* Initialize MCP2515 */
    if (spi_acquire(pdMS_TO_TICKS(1000))) {
        mcp_init();
        spi_release();
    }
    
    /* Configure CAN interrupt pin */
    if (!nrfx_gpiote_is_init()) {
        nrfx_gpiote_init();
    }
    
    nrfx_gpiote_in_config_t int_config = NRFX_GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    int_config.pull = NRF_GPIO_PIN_PULLUP;
    nrfx_gpiote_in_init(CAN_INT_PIN, &int_config, can_int_handler);
    nrfx_gpiote_in_event_enable(CAN_INT_PIN, true);
    
    /* Initial discovery */
    SEGGER_RTT_printf(0, "Discovering actuators...\n");
    send_discover_broadcast();
    
    can_frame_t frame;
    can_command_t cmd;
    TickType_t last_heartbeat = 0;
    
    for (;;) {
        /* Wait for CAN interrupt or timeout */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
        
        /* Process incoming CAN messages */
        if (spi_acquire(pdMS_TO_TICKS(50))) {
            while (mcp_read_message(&frame)) {
                process_can_message(&frame);
            }
            spi_release();
        }
        
        /* Process command queue */
        while (xQueueReceive(m_cmd_queue, &cmd, 0) == pdTRUE) {
            switch (cmd.type) {
                case CAN_CMD_OPEN:
                    send_valve_command(cmd.address, CAN_CMD_OPEN);
                    SEGGER_RTT_printf(0, "CAN: OPEN addr=%d\n", cmd.address);
                    break;
                    
                case CAN_CMD_CLOSE:
                    send_valve_command(cmd.address, CAN_CMD_CLOSE);
                    SEGGER_RTT_printf(0, "CAN: CLOSE addr=%d\n", cmd.address);
                    break;
                    
                case CAN_CMD_STOP:
                    send_valve_command(cmd.address, CAN_CMD_STOP);
                    break;
                    
                case CAN_CMD_EMERGENCY_CLOSE_ALL:
                    send_emergency_close();
                    break;
                    
                case CAN_CMD_QUERY:
                    send_valve_command(cmd.address, CAN_WIRE_CMD_STATUS);
                    break;
                    
                case CAN_CMD_DISCOVER_ALL:
                    send_discover_broadcast();
                    break;
            }
        }
        
        /* Periodic heartbeat/discovery */
        TickType_t now = xTaskGetTickCount();
        if (now - last_heartbeat >= pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS)) {
            send_discover_broadcast();
            last_heartbeat = now;
            
            /* Mark stale actuators as offline */
            for (int i = ACTUATOR_ADDR_MIN; i <= ACTUATOR_ADDR_MAX; i++) {
                if (m_actuators[i].online && 
                    (now - m_actuators[i].last_seen) > pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS * 3)) {
                    m_actuators[i].online = false;
                    SEGGER_RTT_printf(0, "Actuator %d offline\n", i);
                }
            }
        }
    }
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

bool can_send_command(can_cmd_type_t type, uint8_t address, uint16_t command_id)
{
    can_command_t cmd = {
        .type = type,
        .address = address,
        .command_id = command_id,
    };
    return xQueueSend(m_cmd_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE;
}

bool can_open_valve(uint8_t address)
{
    return can_send_command(CAN_CMD_OPEN, address, 0);
}

bool can_close_valve(uint8_t address)
{
    return can_send_command(CAN_CMD_CLOSE, address, 0);
}

bool can_stop_valve(uint8_t address)
{
    return can_send_command(CAN_CMD_STOP, address, 0);
}

void can_emergency_close_all(void)
{
    can_send_command(CAN_CMD_EMERGENCY_CLOSE_ALL, 0xFF, 0);
}

void can_query_all(void)
{
    for (int i = ACTUATOR_ADDR_MIN; i <= ACTUATOR_ADDR_MAX; i++) {
        if (m_actuators[i].online) {
            can_send_command(CAN_CMD_QUERY, i, 0);
        }
    }
}

void can_discover_all(void)
{
    can_send_command(CAN_CMD_DISCOVER_ALL, 0, 0);
}

bool can_is_actuator_online(uint8_t address)
{
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) return false;
    return m_actuators[address].online;
}

const actuator_status_t* can_get_actuator(uint8_t address)
{
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) return NULL;
    return &m_actuators[address];
}

uint8_t can_get_valve_state(uint8_t address)
{
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) return 0xFF;
    if (!m_actuators[address].online) return 0xFF;
    return m_actuators[address].status_flags;
}

uint16_t can_get_motor_current(uint8_t address)
{
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) return 0;
    return m_actuators[address].current_ma;
}

uint8_t can_get_online_count(void)
{
    uint8_t count = 0;
    for (int i = ACTUATOR_ADDR_MIN; i <= ACTUATOR_ADDR_MAX; i++) {
        if (m_actuators[i].online) count++;
    }
    return count;
}

/* ==========================================================================
 * UID-BASED OPERATIONS
 * All external interfaces (LoRa, BLE, schedules) use UID, not CAN address
 * ========================================================================== */

bool can_uid_equals(const uint8_t a[8], const uint8_t b[8])
{
    return memcmp(a, b, 8) == 0;
}

uint8_t can_lookup_address_by_uid(const uint8_t uid[8])
{
    for (int i = ACTUATOR_ADDR_MIN; i <= ACTUATOR_ADDR_MAX; i++) {
        if (m_actuators[i].uid_known && can_uid_equals(m_actuators[i].uid, uid)) {
            return i;
        }
    }
    return 0;  /* Not found */
}

const actuator_status_t* can_get_actuator_by_uid(const uint8_t uid[8])
{
    uint8_t addr = can_lookup_address_by_uid(uid);
    if (addr == 0) return NULL;
    return &m_actuators[addr];
}

bool can_open_valve_by_uid(const uint8_t uid[8])
{
    uint8_t addr = can_lookup_address_by_uid(uid);
    if (addr == 0) {
        SEGGER_RTT_printf(0, "CAN: UID not found for open\n");
        return false;
    }
    return can_open_valve(addr);
}

bool can_close_valve_by_uid(const uint8_t uid[8])
{
    uint8_t addr = can_lookup_address_by_uid(uid);
    if (addr == 0) {
        SEGGER_RTT_printf(0, "CAN: UID not found for close\n");
        return false;
    }
    return can_close_valve(addr);
}

bool can_stop_valve_by_uid(const uint8_t uid[8])
{
    uint8_t addr = can_lookup_address_by_uid(uid);
    if (addr == 0) {
        SEGGER_RTT_printf(0, "CAN: UID not found for stop\n");
        return false;
    }
    return can_stop_valve(addr);
}

uint8_t can_get_valve_state_by_uid(const uint8_t uid[8])
{
    uint8_t addr = can_lookup_address_by_uid(uid);
    if (addr == 0) return 0xFF;
    return m_actuators[addr].status_flags;
}
