/**
 * @file agsys_ble_commands.h
 * @brief BLE command definitions for AgSys devices
 * 
 * Defines command IDs and response formats for the BLE command characteristic.
 * Commands are device-type specific but share common response format.
 */

#ifndef AGSYS_BLE_COMMANDS_H
#define AGSYS_BLE_COMMANDS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * COMMON COMMANDS (0x00 - 0x0F)
 * Available on all device types
 * ========================================================================== */

#define AGSYS_BLE_CMD_PING              0x00    /* Ping/heartbeat */
#define AGSYS_BLE_CMD_GET_INFO          0x01    /* Get device info */
#define AGSYS_BLE_CMD_REBOOT            0x02    /* Reboot device */
#define AGSYS_BLE_CMD_FACTORY_RESET     0x03    /* Factory reset (requires auth) */

/* ==========================================================================
 * VALVE CONTROLLER COMMANDS (0x10 - 0x2F)
 * ========================================================================== */

#define AGSYS_BLE_CMD_VC_DISCOVER       0x10    /* Discover actuators on CAN bus */
#define AGSYS_BLE_CMD_VC_GET_ACTUATORS  0x11    /* Get list of discovered actuators */
#define AGSYS_BLE_CMD_VC_OPEN_VALVE     0x12    /* Open valve by address or UID */
#define AGSYS_BLE_CMD_VC_CLOSE_VALVE    0x13    /* Close valve by address or UID */
#define AGSYS_BLE_CMD_VC_STOP_VALVE     0x14    /* Stop valve movement */
#define AGSYS_BLE_CMD_VC_GET_STATUS     0x15    /* Get valve status */
#define AGSYS_BLE_CMD_VC_EMERGENCY_STOP 0x16    /* Emergency close all valves */
#define AGSYS_BLE_CMD_VC_GET_SCHEDULES  0x17    /* Get irrigation schedules */
#define AGSYS_BLE_CMD_VC_SET_SCHEDULE   0x18    /* Set/update a schedule */
#define AGSYS_BLE_CMD_VC_DELETE_SCHEDULE 0x19   /* Delete a schedule */
#define AGSYS_BLE_CMD_VC_GET_RTC        0x1A    /* Get RTC time */
#define AGSYS_BLE_CMD_VC_SET_RTC        0x1B    /* Set RTC time */

/* ==========================================================================
 * SOIL MOISTURE SENSOR COMMANDS (0x30 - 0x3F)
 * ========================================================================== */

#define AGSYS_BLE_CMD_SM_READ_NOW       0x30    /* Trigger immediate reading */
#define AGSYS_BLE_CMD_SM_CALIBRATE      0x31    /* Start calibration */
#define AGSYS_BLE_CMD_SM_GET_CAL        0x32    /* Get calibration data */
#define AGSYS_BLE_CMD_SM_SET_INTERVAL   0x33    /* Set reporting interval */

/* ==========================================================================
 * WATER METER COMMANDS (0x40 - 0x4F)
 * ========================================================================== */

#define AGSYS_BLE_CMD_WM_RESET_TOTAL    0x40    /* Reset totalizer */
#define AGSYS_BLE_CMD_WM_CALIBRATE_ZERO 0x41    /* Zero calibration */
#define AGSYS_BLE_CMD_WM_CALIBRATE_SPAN 0x42    /* Span calibration */
#define AGSYS_BLE_CMD_WM_GET_CAL        0x43    /* Get calibration data */
#define AGSYS_BLE_CMD_WM_SET_PIPE_SIZE  0x44    /* Set pipe size */
#define AGSYS_BLE_CMD_WM_UNLOCK_MENU    0x45    /* Remote menu unlock */

/* ==========================================================================
 * RESPONSE STATUS CODES
 * ========================================================================== */

#define AGSYS_BLE_RESP_OK               0x00    /* Success */
#define AGSYS_BLE_RESP_ERR_UNKNOWN_CMD  0x01    /* Unknown command */
#define AGSYS_BLE_RESP_ERR_INVALID_PARAM 0x02   /* Invalid parameters */
#define AGSYS_BLE_RESP_ERR_NOT_AUTH     0x03    /* Not authenticated */
#define AGSYS_BLE_RESP_ERR_BUSY         0x04    /* Device busy */
#define AGSYS_BLE_RESP_ERR_TIMEOUT      0x05    /* Operation timed out */
#define AGSYS_BLE_RESP_ERR_NOT_FOUND    0x06    /* Resource not found */
#define AGSYS_BLE_RESP_ERR_HARDWARE     0x07    /* Hardware error */

/* ==========================================================================
 * RESPONSE STRUCTURES
 * ========================================================================== */

/**
 * @brief Common response header (first 2 bytes of all responses)
 */
typedef struct __attribute__((packed)) {
    uint8_t cmd_id;         /* Echo of command ID */
    uint8_t status;         /* Response status code */
} agsys_ble_resp_header_t;

/**
 * @brief Actuator info in discovery response
 */
typedef struct __attribute__((packed)) {
    uint8_t address;        /* CAN bus address (1-64) */
    uint8_t uid[7];         /* Unique ID (truncated to 7 bytes for BLE) */
    uint8_t state;          /* Current valve state */
    uint8_t flags;          /* Status flags */
} agsys_ble_actuator_info_t;

/**
 * @brief Discovery response (AGSYS_BLE_CMD_VC_GET_ACTUATORS)
 * 
 * Response format:
 * [header (2)] [count (1)] [actuator_info (10)] * count
 * 
 * Max actuators per response limited by BLE MTU.
 * For >20 actuators, use pagination with offset parameter.
 */
typedef struct __attribute__((packed)) {
    agsys_ble_resp_header_t header;
    uint8_t count;          /* Number of actuators in this response */
    uint8_t total;          /* Total actuators discovered */
    uint8_t offset;         /* Offset for pagination */
    /* Followed by count * agsys_ble_actuator_info_t */
} agsys_ble_actuator_list_resp_t;

/**
 * @brief Valve status response (AGSYS_BLE_CMD_VC_GET_STATUS)
 */
typedef struct __attribute__((packed)) {
    agsys_ble_resp_header_t header;
    uint8_t address;        /* CAN bus address */
    uint8_t state;          /* Valve state (0=closed, 1=open, 2=moving, 0xFF=unknown) */
    uint16_t current_ma;    /* Motor current in mA */
    uint8_t flags;          /* Status flags */
} agsys_ble_valve_status_resp_t;

#ifdef __cplusplus
}
#endif

#endif /* AGSYS_BLE_COMMANDS_H */
