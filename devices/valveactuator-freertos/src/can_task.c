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
#include "agsys_can.h"
#include "agsys_spi.h"
#include "board_config.h"

#include <string.h>

/* CAN protocol constants from shared agsys_can.h */

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static TaskHandle_t m_can_task_handle = NULL;
static uint8_t m_device_address = 0;

/* CAN controller context */
static agsys_can_ctx_t m_can_ctx;

/* CAN frame type alias for local use */
typedef agsys_can_frame_t can_frame_t;


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

    /* Register with SPI manager */
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
        while (agsys_can_read(&m_can_ctx, &frame)) {
            SEGGER_RTT_printf(0, "CAN RX: ID=0x%03X, DLC=%d\n", frame.id, frame.dlc);

            switch (frame.id) {
                case (AGSYS_CAN_ID_CMD_BASE + AGSYS_CAN_WIRE_CMD_OPEN):
                    if (frame.dlc >= 1 && frame.data[0] == m_device_address) {
                        SEGGER_RTT_printf(0, "CMD: OPEN\n");
                        valve_request_open();
                        can_send_status();
                    }
                    break;

                case (AGSYS_CAN_ID_CMD_BASE + AGSYS_CAN_WIRE_CMD_CLOSE):
                    if (frame.dlc >= 1 && frame.data[0] == m_device_address) {
                        SEGGER_RTT_printf(0, "CMD: CLOSE\n");
                        valve_request_close();
                        can_send_status();
                    }
                    break;

                case (AGSYS_CAN_ID_CMD_BASE + AGSYS_CAN_WIRE_CMD_STOP):
                    if (frame.dlc >= 1 && frame.data[0] == m_device_address) {
                        SEGGER_RTT_printf(0, "CMD: STOP\n");
                        valve_request_stop();
                        can_send_status();
                    }
                    break;

                case (AGSYS_CAN_ID_CMD_BASE + AGSYS_CAN_WIRE_CMD_STATUS):
                    if (frame.dlc >= 1 && frame.data[0] == m_device_address) {
                        SEGGER_RTT_printf(0, "CMD: QUERY\n");
                        can_send_status();
                    }
                    break;

                case (AGSYS_CAN_ID_CMD_BASE + AGSYS_CAN_WIRE_CMD_EMERGENCY):
                    SEGGER_RTT_printf(0, "CMD: EMERGENCY CLOSE\n");
                    valve_request_emergency_close();
                    can_send_status();
                    break;

                case AGSYS_CAN_ID_DISCOVER:
                    /* Broadcast discovery - all actuators respond with staggered timing */
                    SEGGER_RTT_printf(0, "CMD: DISCOVER BROADCAST\n");
                    /* Delay based on address to avoid collisions */
                    vTaskDelay(pdMS_TO_TICKS(m_device_address * AGSYS_CAN_DISCOVERY_DELAY_MS));
                    can_send_discovery_response();
                    break;

                case AGSYS_CAN_ID_EMERGENCY:
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
    frame.id = AGSYS_CAN_ID_STATUS_BASE + m_device_address;  /* Status response */
    frame.dlc = 4;
    frame.data[0] = valve_get_status_flags();
    
    uint16_t current = valve_get_current_ma();
    frame.data[1] = (current >> 8) & 0xFF;
    frame.data[2] = current & 0xFF;
    frame.data[3] = 0;

    if (!agsys_can_send(&m_can_ctx, &frame)) {
        SEGGER_RTT_printf(0, "Failed to send status\n");
    }
}

void can_send_uid(void)
{
    /* Get device UID from FICR */
    uint32_t uid0 = NRF_FICR->DEVICEID[0];
    uint32_t uid1 = NRF_FICR->DEVICEID[1];

    can_frame_t frame;
    frame.id = AGSYS_CAN_ID_UID_RESP_BASE + m_device_address;  /* UID response */
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

    if (!agsys_can_send(&m_can_ctx, &frame)) {
        SEGGER_RTT_printf(0, "Failed to send UID\n");
    }
}

void can_send_discovery_response(void)
{
    /* Discovery response includes: address (1 byte) + UID (7 bytes) 
     * Response CAN ID: 0x1F1 (discovery response)
     * This allows controller to distinguish discovery responses from other messages
     */
    uint32_t uid0 = NRF_FICR->DEVICEID[0];
    uint32_t uid1 = NRF_FICR->DEVICEID[1];

    can_frame_t frame;
    frame.id = AGSYS_CAN_ID_DISCOVER_RESP;  /* 0x1F1 = Discovery response */
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

    if (!agsys_can_send(&m_can_ctx, &frame)) {
        SEGGER_RTT_printf(0, "Failed to send discovery response\n");
    }
}

TaskHandle_t can_get_task_handle(void)
{
    return m_can_task_handle;
}
