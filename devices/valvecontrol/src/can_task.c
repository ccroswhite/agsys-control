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
#include "agsys_can.h"
#include "agsys_spi.h"
#include "board_config.h"

#include <string.h>

/* Constants from shared agsys_can.h via can_task.h */

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static actuator_status_t m_actuators[ACTUATOR_ADDR_MAX + 1];
static QueueHandle_t m_cmd_queue = NULL;
static TaskHandle_t m_task_handle = NULL;

/* CAN controller context */
static agsys_can_ctx_t m_can_ctx;

/* CAN frame type alias for local use */
typedef agsys_can_frame_t can_frame_t;

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
    if (frame->id == AGSYS_CAN_ID_DISCOVER_RESP) {
        process_discovery_response(frame->data, frame->dlc);
    }
    /* Status response: 0x180 + address */
    else if (frame->id >= AGSYS_CAN_ID_STATUS_BASE && frame->id < AGSYS_CAN_ID_STATUS_BASE + 0x40) {
        uint8_t addr = frame->id - AGSYS_CAN_ID_STATUS_BASE;
        process_status_response(addr, frame->data, frame->dlc);
    }
    /* UID response: 0x190 + address */
    else if (frame->id >= AGSYS_CAN_ID_UID_RESP_BASE && frame->id < AGSYS_CAN_ID_UID_RESP_BASE + 0x40) {
        uint8_t addr = frame->id - AGSYS_CAN_ID_UID_RESP_BASE;
        process_uid_response(addr, frame->data, frame->dlc);
    }
}

static void send_valve_command(uint8_t address, uint8_t cmd)
{
    can_frame_t frame;
    frame.id = AGSYS_CAN_ID_CMD_BASE + cmd;
    frame.dlc = 1;
    frame.data[0] = address;
    
    agsys_can_send(&m_can_ctx, &frame);
}

static void send_discover_broadcast(void)
{
    can_frame_t frame;
    frame.id = AGSYS_CAN_ID_DISCOVER;
    frame.dlc = 0;
    
    agsys_can_send(&m_can_ctx, &frame);
}

static void send_emergency_close(void)
{
    can_frame_t frame;
    frame.id = AGSYS_CAN_ID_EMERGENCY;
    frame.dlc = 0;
    
    agsys_can_send(&m_can_ctx, &frame);
    
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
    
    /* Register with SPI manager on bus 0 (Peripherals bus) */
    agsys_spi_config_t spi_config = {
        .cs_pin = SPI_CS_CAN_PIN,
        .cs_active_low = true,
        .frequency = NRF_SPIM_FREQ_4M,
        .mode = 0,
        .bus = AGSYS_SPI_BUS_0,
    };
    
    agsys_spi_handle_t spi_handle;
    if (agsys_spi_register(&spi_config, &spi_handle) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "CAN: Failed to register SPI\n");
        vTaskDelete(NULL);
        return;
    }
    
    /* Initialize MCP2515 using shared CAN driver */
    if (!agsys_can_init(&m_can_ctx, spi_handle)) {
        SEGGER_RTT_printf(0, "CAN: Failed to initialize MCP2515\n");
        vTaskDelete(NULL);
        return;
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
        while (agsys_can_read(&m_can_ctx, &frame)) {
            process_can_message(&frame);
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
                    send_valve_command(cmd.address, AGSYS_CAN_WIRE_CMD_STATUS);
                    break;
                    
                case CAN_CMD_DISCOVER_ALL:
                    send_discover_broadcast();
                    break;
            }
        }
        
        /* Periodic heartbeat/discovery */
        TickType_t now = xTaskGetTickCount();
        if (now - last_heartbeat >= pdMS_TO_TICKS(AGSYS_CAN_HEARTBEAT_INTERVAL_MS)) {
            send_discover_broadcast();
            last_heartbeat = now;
            
            /* Mark stale actuators as offline */
            for (int i = ACTUATOR_ADDR_MIN; i <= ACTUATOR_ADDR_MAX; i++) {
                if (m_actuators[i].online && 
                    (now - m_actuators[i].last_seen) > pdMS_TO_TICKS(AGSYS_CAN_HEARTBEAT_INTERVAL_MS * 3)) {
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
