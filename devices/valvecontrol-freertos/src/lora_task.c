/**
 * @file lora_task.c
 * @brief LoRa task implementation for Valve Controller
 * 
 * Handles RFM95C communication with property controller using AgSys protocol.
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrfx_gpiote.h"
#include "SEGGER_RTT.h"

#include "lora_task.h"
#include "can_task.h"
#include "spi_driver.h"
#include "board_config.h"
#include "agsys_device.h"
#include "agsys_protocol.h"
#include "agsys_memory_layout.h"
#include "agsys_fram.h"

#include <string.h>

/* Firmware version - should match build */
#ifndef FW_VERSION_MAJOR
#define FW_VERSION_MAJOR    1
#endif
#ifndef FW_VERSION_MINOR
#define FW_VERSION_MINOR    0
#endif
#ifndef FW_VERSION_PATCH
#define FW_VERSION_PATCH    0
#endif

/* External device context for logging */
extern agsys_device_ctx_t m_device_ctx;
extern agsys_fram_ctx_t m_fram_ctx;

/* Boot reason - set during startup */
static uint8_t m_boot_reason = AGSYS_BOOT_REASON_NORMAL;

/* ==========================================================================
 * RFM95C REGISTER DEFINITIONS
 * ========================================================================== */

#define REG_FIFO                0x00
#define REG_OP_MODE             0x01
#define REG_FRF_MSB             0x06
#define REG_FRF_MID             0x07
#define REG_FRF_LSB             0x08
#define REG_PA_CONFIG           0x09
#define REG_LNA                 0x0C
#define REG_FIFO_ADDR_PTR       0x0D
#define REG_FIFO_TX_BASE        0x0E
#define REG_FIFO_RX_BASE        0x0F
#define REG_FIFO_RX_CURRENT     0x10
#define REG_IRQ_FLAGS           0x12
#define REG_RX_NB_BYTES         0x13
#define REG_PKT_SNR             0x19
#define REG_PKT_RSSI            0x1A
#define REG_MODEM_CONFIG_1      0x1D
#define REG_MODEM_CONFIG_2      0x1E
#define REG_PREAMBLE_MSB        0x20
#define REG_PREAMBLE_LSB        0x21
#define REG_PAYLOAD_LENGTH      0x22
#define REG_MODEM_CONFIG_3      0x26
#define REG_SYNC_WORD           0x39
#define REG_DIO_MAPPING_1       0x40
#define REG_VERSION             0x42
#define REG_PA_DAC              0x4D

/* Operating modes */
#define MODE_SLEEP              0x00
#define MODE_STDBY              0x01
#define MODE_TX                 0x03
#define MODE_RX_CONTINUOUS      0x05
#define MODE_RX_SINGLE          0x06
#define MODE_LORA               0x80

/* IRQ flags */
#define IRQ_TX_DONE             0x08
#define IRQ_RX_DONE             0x40
#define IRQ_PAYLOAD_CRC_ERROR   0x20

/* AgSys message types - using canonical definitions from agsys_protocol.h
 * Legacy aliases for backward compatibility during migration */
#define AGSYS_MSG_SCHEDULE_UPDATE   AGSYS_MSG_VALVE_SCHEDULE
#define AGSYS_MSG_SCHEDULE_REQUEST  AGSYS_MSG_VALVE_SCHEDULE_REQ
#define AGSYS_MSG_VALVE_DISCOVER    0x60  /* TODO: Add to canonical protocol */
#define AGSYS_MSG_VALVE_DISCOVERY_RESP 0x61  /* TODO: Add to canonical protocol */

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static TaskHandle_t m_task_handle = NULL;
static uint8_t m_device_uid[8];
static uint8_t m_sequence = 0;

/* External power state from main.c */
extern volatile bool g_on_battery_power;

/* ==========================================================================
 * RFM95C LOW-LEVEL FUNCTIONS
 * ========================================================================== */

static void rfm_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { reg | 0x80, value };
    spi_transfer(SPI_CS_LORA_PIN, tx, NULL, 2);
}

static uint8_t rfm_read_reg(uint8_t reg)
{
    uint8_t tx[2] = { reg & 0x7F, 0x00 };
    uint8_t rx[2];
    spi_transfer(SPI_CS_LORA_PIN, tx, rx, 2);
    return rx[1];
}

static void rfm_set_mode(uint8_t mode)
{
    rfm_write_reg(REG_OP_MODE, MODE_LORA | mode);
}

static void rfm_set_frequency(uint32_t freq)
{
    uint64_t frf = ((uint64_t)freq << 19) / 32000000;
    rfm_write_reg(REG_FRF_MSB, (frf >> 16) & 0xFF);
    rfm_write_reg(REG_FRF_MID, (frf >> 8) & 0xFF);
    rfm_write_reg(REG_FRF_LSB, frf & 0xFF);
}

static void rfm_init(void)
{
    /* Reset */
    nrf_gpio_cfg_output(LORA_RESET_PIN);
    nrf_gpio_pin_clear(LORA_RESET_PIN);
    vTaskDelay(pdMS_TO_TICKS(10));
    nrf_gpio_pin_set(LORA_RESET_PIN);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    /* Check version */
    uint8_t version = rfm_read_reg(REG_VERSION);
    SEGGER_RTT_printf(0, "RFM95 version: 0x%02X\n", version);
    
    /* Sleep mode for configuration */
    rfm_set_mode(MODE_SLEEP);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    /* Set frequency */
    rfm_set_frequency(LORA_FREQUENCY);
    
    /* Configure modem: BW=125kHz, CR=4/5, explicit header */
    rfm_write_reg(REG_MODEM_CONFIG_1, 0x72);
    
    /* SF=7, CRC on */
    rfm_write_reg(REG_MODEM_CONFIG_2, (LORA_SPREADING_FACTOR << 4) | 0x04);
    
    /* LNA gain auto, low data rate optimize off */
    rfm_write_reg(REG_MODEM_CONFIG_3, 0x04);
    
    /* TX power +20dBm */
    rfm_write_reg(REG_PA_CONFIG, 0x8F);
    rfm_write_reg(REG_PA_DAC, 0x87);
    
    /* Preamble length 8 */
    rfm_write_reg(REG_PREAMBLE_MSB, 0x00);
    rfm_write_reg(REG_PREAMBLE_LSB, 0x08);
    
    /* Sync word */
    rfm_write_reg(REG_SYNC_WORD, LORA_SYNC_WORD);
    
    /* FIFO pointers */
    rfm_write_reg(REG_FIFO_TX_BASE, 0x00);
    rfm_write_reg(REG_FIFO_RX_BASE, 0x00);
    
    /* DIO0 = RxDone/TxDone */
    rfm_write_reg(REG_DIO_MAPPING_1, 0x00);
    
    /* Standby mode */
    rfm_set_mode(MODE_STDBY);
    
    SEGGER_RTT_printf(0, "RFM95 initialized at %d MHz\n", LORA_FREQUENCY / 1000000);
}

static bool rfm_send(const uint8_t *data, uint8_t len)
{
    if (len > 255) return false;
    
    rfm_set_mode(MODE_STDBY);
    
    /* Set FIFO pointer */
    rfm_write_reg(REG_FIFO_ADDR_PTR, 0x00);
    
    /* Write data to FIFO */
    spi_cs_assert(SPI_CS_LORA_PIN);
    uint8_t cmd = REG_FIFO | 0x80;
    spi_transfer_raw(&cmd, NULL, 1);
    spi_transfer_raw(data, NULL, len);
    spi_cs_deassert(SPI_CS_LORA_PIN);
    
    /* Set payload length */
    rfm_write_reg(REG_PAYLOAD_LENGTH, len);
    
    /* Clear IRQ flags */
    rfm_write_reg(REG_IRQ_FLAGS, 0xFF);
    
    /* Start TX */
    rfm_set_mode(MODE_TX);
    
    /* Wait for TX done (with timeout) */
    for (int i = 0; i < 200; i++) {
        if (rfm_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE) {
            rfm_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE);
            rfm_set_mode(MODE_STDBY);
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    SEGGER_RTT_printf(0, "LoRa TX timeout\n");
    rfm_set_mode(MODE_STDBY);
    return false;
}

static int rfm_receive(uint8_t *data, uint8_t max_len, int16_t *rssi)
{
    uint8_t irq = rfm_read_reg(REG_IRQ_FLAGS);
    
    if (!(irq & IRQ_RX_DONE)) {
        return 0;
    }
    
    /* Clear IRQ */
    rfm_write_reg(REG_IRQ_FLAGS, IRQ_RX_DONE | IRQ_PAYLOAD_CRC_ERROR);
    
    /* Check CRC */
    if (irq & IRQ_PAYLOAD_CRC_ERROR) {
        SEGGER_RTT_printf(0, "LoRa CRC error\n");
        return -1;
    }
    
    /* Get packet length */
    uint8_t len = rfm_read_reg(REG_RX_NB_BYTES);
    if (len > max_len) len = max_len;
    
    /* Set FIFO pointer to packet start */
    rfm_write_reg(REG_FIFO_ADDR_PTR, rfm_read_reg(REG_FIFO_RX_CURRENT));
    
    /* Read data */
    spi_cs_assert(SPI_CS_LORA_PIN);
    uint8_t cmd = REG_FIFO & 0x7F;
    spi_transfer_raw(&cmd, NULL, 1);
    spi_transfer_raw(NULL, data, len);
    spi_cs_deassert(SPI_CS_LORA_PIN);
    
    /* Get RSSI */
    if (rssi) {
        *rssi = rfm_read_reg(REG_PKT_RSSI) - 137;
    }
    
    return len;
}

static void rfm_start_receive(void)
{
    rfm_set_mode(MODE_STDBY);
    rfm_write_reg(REG_FIFO_ADDR_PTR, 0x00);
    rfm_write_reg(REG_IRQ_FLAGS, 0xFF);
    rfm_set_mode(MODE_RX_CONTINUOUS);
}

/* ==========================================================================
 * AGSYS PROTOCOL (using canonical agsys_header_t from agsys_protocol.h)
 * ========================================================================== */

static void get_device_uid(void)
{
    uint32_t uid0 = NRF_FICR->DEVICEID[0];
    uint32_t uid1 = NRF_FICR->DEVICEID[1];
    
    m_device_uid[0] = (uid0 >> 0) & 0xFF;
    m_device_uid[1] = (uid0 >> 8) & 0xFF;
    m_device_uid[2] = (uid0 >> 16) & 0xFF;
    m_device_uid[3] = (uid0 >> 24) & 0xFF;
    m_device_uid[4] = (uid1 >> 0) & 0xFF;
    m_device_uid[5] = (uid1 >> 8) & 0xFF;
    m_device_uid[6] = (uid1 >> 16) & 0xFF;
    m_device_uid[7] = (uid1 >> 24) & 0xFF;
}

static void build_header(agsys_header_t *hdr, uint8_t msg_type)
{
    hdr->magic[0] = AGSYS_MAGIC_BYTE1;
    hdr->magic[1] = AGSYS_MAGIC_BYTE2;
    hdr->version = AGSYS_PROTOCOL_VERSION;
    hdr->msg_type = msg_type;
    hdr->device_type = AGSYS_DEVICE_TYPE_VALVE_CONTROLLER;
    memcpy(hdr->device_uid, m_device_uid, 8);
    hdr->sequence = m_sequence++;
}

static void process_valve_command(const uint8_t *payload, uint8_t len)
{
    /* Valve commands use UID (8 bytes), not CAN address */
    if (len < 11) return;  /* 8 byte UID + 1 cmd + 2 command_id */
    
    const uint8_t *actuator_uid = payload;
    uint8_t command = payload[8];
    uint16_t command_id = (payload[9] << 8) | payload[10];
    
    SEGGER_RTT_printf(0, "Valve cmd: UID=%02X%02X... cmd=%d id=%d\n", 
                      actuator_uid[0], actuator_uid[1], command, command_id);
    
    bool success = false;
    uint8_t result_state = 0xFF;
    
    switch (command) {
        case 0x01:  /* Open */
            success = can_open_valve_by_uid(actuator_uid);
            break;
        case 0x02:  /* Close */
            success = can_close_valve_by_uid(actuator_uid);
            break;
        case 0x03:  /* Stop */
            success = can_stop_valve_by_uid(actuator_uid);
            break;
        case 0x04:  /* Emergency close all */
            can_emergency_close_all();
            success = true;
            break;
    }
    
    if (success) {
        result_state = can_get_valve_state_by_uid(actuator_uid);
    }
    
    /* Send ACK with UID */
    lora_send_valve_ack_by_uid(actuator_uid, command_id, result_state, success, 0);
}

static void process_discovery_command(void)
{
    SEGGER_RTT_printf(0, "Discovery command received\n");
    can_discover_all();
    vTaskDelay(pdMS_TO_TICKS(500));  /* Wait for actuator responses */
    lora_send_discovery_response();
}

static void process_lora_message(const uint8_t *data, uint8_t len, int16_t rssi)
{
    if (len < sizeof(agsys_header_t)) return;
    
    agsys_header_t *hdr = (agsys_header_t *)data;
    
    /* Validate magic bytes */
    if (hdr->magic[0] != AGSYS_MAGIC_BYTE1 || hdr->magic[1] != AGSYS_MAGIC_BYTE2) {
        SEGGER_RTT_printf(0, "LoRa RX: Invalid magic bytes\n");
        return;
    }
    
    const uint8_t *payload = data + sizeof(agsys_header_t);
    uint8_t payload_len = len - sizeof(agsys_header_t);
    
    SEGGER_RTT_printf(0, "LoRa RX: type=0x%02X len=%d rssi=%d\n", 
                      hdr->msg_type, len, rssi);
    
    switch (hdr->msg_type) {
        case AGSYS_MSG_VALVE_COMMAND:
            process_valve_command(payload, payload_len);
            break;
            
        case AGSYS_MSG_VALVE_DISCOVER:
            process_discovery_command();
            break;
            
        case AGSYS_MSG_TIME_SYNC:
            if (payload_len >= 4) {
                uint32_t timestamp = (payload[0] << 24) | (payload[1] << 16) |
                                     (payload[2] << 8) | payload[3];
                SEGGER_RTT_printf(0, "Time sync: %u\n", timestamp);
                /* TODO: Set RTC */
            }
            break;
            
        case AGSYS_MSG_SCHEDULE_UPDATE:
            SEGGER_RTT_printf(0, "Schedule update received\n");
            /* TODO: Parse and store schedule */
            break;
    }
}

/* ==========================================================================
 * INTERRUPT HANDLER
 * ========================================================================== */

static void lora_int_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (m_task_handle != NULL) {
        vTaskNotifyGiveFromISR(m_task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* ==========================================================================
 * LORA TASK
 * ========================================================================== */

static void load_boot_reason_from_fram(void)
{
    agsys_ota_fram_state_t ota_state;
    
    if (agsys_fram_read(&m_fram_ctx, AGSYS_FRAM_OTA_STATE_ADDR, 
                        (uint8_t *)&ota_state, sizeof(ota_state)) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "LoRa: Failed to read OTA state from FRAM\n");
        return;
    }
    
    if (ota_state.magic != AGSYS_OTA_FRAM_MAGIC) {
        m_boot_reason = AGSYS_BOOT_REASON_NORMAL;
        return;
    }
    
    switch (ota_state.state) {
        case AGSYS_OTA_STATE_SUCCESS:
            m_boot_reason = AGSYS_BOOT_REASON_OTA_SUCCESS;
            SEGGER_RTT_printf(0, "LoRa: Boot after successful OTA to v%d.%d.%d\n",
                              ota_state.target_version[0],
                              ota_state.target_version[1],
                              ota_state.target_version[2]);
            break;
            
        case AGSYS_OTA_STATE_ROLLED_BACK:
        case AGSYS_OTA_STATE_FAILED:
            m_boot_reason = AGSYS_BOOT_REASON_OTA_ROLLBACK;
            SEGGER_RTT_printf(0, "LoRa: Boot after OTA rollback (error=%d)\n",
                              ota_state.error_code);
            break;
            
        default:
            m_boot_reason = AGSYS_BOOT_REASON_NORMAL;
            break;
    }
    
    /* Clear OTA state after reading */
    if (ota_state.state == AGSYS_OTA_STATE_SUCCESS || 
        ota_state.state == AGSYS_OTA_STATE_ROLLED_BACK ||
        ota_state.state == AGSYS_OTA_STATE_FAILED) {
        ota_state.state = AGSYS_OTA_STATE_NONE;
        ota_state.magic = 0;
        agsys_fram_write(&m_fram_ctx, AGSYS_FRAM_OTA_STATE_ADDR,
                         (uint8_t *)&ota_state, sizeof(ota_state));
    }
}

bool lora_task_init(void)
{
    get_device_uid();
    load_boot_reason_from_fram();
    return true;
}

void lora_task(void *pvParameters)
{
    (void)pvParameters;
    m_task_handle = xTaskGetCurrentTaskHandle();
    
    SEGGER_RTT_printf(0, "LoRa task started\n");
    
    /* Initialize RFM95 */
    if (spi_acquire(pdMS_TO_TICKS(1000))) {
        rfm_init();
        spi_release();
    }
    
    /* Configure DIO0 interrupt */
    nrfx_gpiote_in_config_t int_config = NRFX_GPIOTE_CONFIG_IN_SENSE_LOTOHI(true);
    int_config.pull = NRF_GPIO_PIN_NOPULL;
    nrfx_gpiote_in_init(LORA_DIO0_PIN, &int_config, lora_int_handler);
    nrfx_gpiote_in_event_enable(LORA_DIO0_PIN, true);
    
    /* Start receiving */
    if (spi_acquire(pdMS_TO_TICKS(100))) {
        rfm_start_receive();
        spi_release();
    }
    
    uint8_t rx_buf[128];
    int16_t rssi;
    TickType_t last_status_report = 0;
    TickType_t last_schedule_pull = 0;
    
    for (;;) {
        /* Wait for interrupt or timeout */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        
        /* Check for received data */
        if (spi_acquire(pdMS_TO_TICKS(50))) {
            int len = rfm_receive(rx_buf, sizeof(rx_buf), &rssi);
            if (len > 0) {
                process_lora_message(rx_buf, len, rssi);
            }
            
            /* Restart receive if needed */
            uint8_t mode = rfm_read_reg(REG_OP_MODE) & 0x07;
            if (mode != MODE_RX_CONTINUOUS) {
                rfm_start_receive();
            }
            
            spi_release();
        }
        
        TickType_t now = xTaskGetTickCount();
        
        /* Periodic status report (only on mains power) */
        if (!g_on_battery_power && 
            (now - last_status_report) >= pdMS_TO_TICKS(STATUS_REPORT_INTERVAL_MS)) {
            lora_send_status_report();
            last_status_report = now;
        }
        
        /* Periodic schedule pull (only on mains power) */
        if (!g_on_battery_power && 
            (now - last_schedule_pull) >= pdMS_TO_TICKS(SCHEDULE_PULL_INTERVAL_MS)) {
            lora_request_schedule();
            last_schedule_pull = now;
        }
    }
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

void lora_send_status_report(void)
{
    uint8_t buffer[128];
    agsys_header_t *hdr = (agsys_header_t *)buffer;
    build_header(hdr, AGSYS_MSG_VALVE_STATUS);
    
    uint8_t *payload = buffer + sizeof(agsys_header_t);
    
    /* Controller info first: fw_version (3 bytes) + boot_reason (1 byte) + actuator_count (1 byte) */
    payload[0] = FW_VERSION_MAJOR;
    payload[1] = FW_VERSION_MINOR;
    payload[2] = FW_VERSION_PATCH;
    payload[3] = m_boot_reason;
    
    uint8_t count = 0;
    uint8_t *actuator_data = payload + 5;  /* Skip header bytes */
    
    /* Add actuator statuses */
    for (uint8_t addr = ACTUATOR_ADDR_MIN; addr <= ACTUATOR_ADDR_MAX && count < 20; addr++) {
        if (can_is_actuator_online(addr)) {
            const actuator_status_t *act = can_get_actuator(addr);
            actuator_data[count * 4 + 0] = addr;
            actuator_data[count * 4 + 1] = act->status_flags;
            actuator_data[count * 4 + 2] = (act->current_ma >> 8) & 0xFF;
            actuator_data[count * 4 + 3] = act->current_ma & 0xFF;
            count++;
        }
    }
    
    payload[4] = count;  /* Actuator count */
    
    uint8_t total_len = sizeof(agsys_header_t) + 5 + (count * 4);
    
    if (spi_acquire(pdMS_TO_TICKS(1000))) {
        if (rfm_send(buffer, total_len)) {
            SEGGER_RTT_printf(0, "Status report sent: %d actuators\n", count);
            
            /* Clear boot reason after first successful report */
            if (m_boot_reason != AGSYS_BOOT_REASON_NORMAL) {
                m_boot_reason = AGSYS_BOOT_REASON_NORMAL;
            }
        }
        rfm_start_receive();
        spi_release();
    }
}

void lora_request_schedule(void)
{
    uint8_t buffer[sizeof(agsys_header_t)];
    agsys_header_t *hdr = (agsys_header_t *)buffer;
    build_header(hdr, AGSYS_MSG_SCHEDULE_REQUEST);
    
    if (spi_acquire(pdMS_TO_TICKS(1000))) {
        if (rfm_send(buffer, sizeof(buffer))) {
            SEGGER_RTT_printf(0, "Schedule request sent\n");
        }
        rfm_start_receive();
        spi_release();
    }
}

void lora_send_valve_ack_by_uid(const uint8_t actuator_uid[8], uint16_t command_id,
                                 uint8_t result_state, bool success, uint8_t error_code)
{
    uint8_t buffer[sizeof(agsys_header_t) + 13];  /* 8 UID + 2 cmd_id + 1 state + 1 success + 1 error */
    agsys_header_t *hdr = (agsys_header_t *)buffer;
    build_header(hdr, AGSYS_MSG_VALVE_ACK);
    
    uint8_t *payload = buffer + sizeof(agsys_header_t);
    memcpy(payload, actuator_uid, 8);
    payload[8] = (command_id >> 8) & 0xFF;
    payload[9] = command_id & 0xFF;
    payload[10] = result_state;
    payload[11] = success ? 1 : 0;
    payload[12] = error_code;
    
    bool tx_success = false;
    if (spi_acquire(pdMS_TO_TICKS(1000))) {
        if (rfm_send(buffer, sizeof(buffer))) {
            SEGGER_RTT_printf(0, "Valve ACK sent for UID %02X%02X...\n", 
                              actuator_uid[0], actuator_uid[1]);
            tx_success = true;
        }
        rfm_start_receive();
        spi_release();
    }
    
    /* Log valve event to flash (always log for audit trail) */
    agsys_device_log_valve(&m_device_ctx, actuator_uid[0], result_state, 
                           success ? 100 : 0);
    
    if (!tx_success) {
        SEGGER_RTT_printf(0, "Valve ACK TX failed, logged to flash\n");
    }
}

void lora_send_discovery_response(void)
{
    uint8_t buffer[200];
    agsys_header_t *hdr = (agsys_header_t *)buffer;
    build_header(hdr, AGSYS_MSG_VALVE_DISCOVERY_RESP);
    
    uint8_t *payload = buffer + sizeof(agsys_header_t);
    uint8_t count = 0;
    
    /* Add actuators with known UIDs */
    for (uint8_t addr = ACTUATOR_ADDR_MIN; addr <= ACTUATOR_ADDR_MAX && count < 15; addr++) {
        const actuator_status_t *act = can_get_actuator(addr);
        if (act && act->online && act->uid_known) {
            uint8_t *entry = payload + 1 + (count * 10);
            entry[0] = addr;
            memcpy(entry + 1, act->uid, 8);
            entry[9] = act->status_flags;
            count++;
        }
    }
    
    payload[0] = count;
    uint8_t total_len = sizeof(agsys_header_t) + 1 + (count * 10);
    
    if (spi_acquire(pdMS_TO_TICKS(1000))) {
        if (rfm_send(buffer, total_len)) {
            SEGGER_RTT_printf(0, "Discovery response sent: %d actuators\n", count);
        }
        rfm_start_receive();
        spi_release();
    }
}
