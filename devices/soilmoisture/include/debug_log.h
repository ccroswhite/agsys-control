/**
 * @file debug_log.h
 * @brief Persistent debug logging to FRAM
 * 
 * Stores boot information, errors, and diagnostic data in FRAM.
 * Survives resets and power cycles. Readable via BLE diagnostics service.
 * 
 * Uses the reserved protected region (0x00C0 - 0x00FF, 64 bytes)
 */

#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <stdint.h>

// Firmware version (set in platformio.ini or here)
#ifndef FW_VERSION_MAJOR
#define FW_VERSION_MAJOR 1
#endif
#ifndef FW_VERSION_MINOR
#define FW_VERSION_MINOR 1
#endif
#ifndef FW_VERSION_PATCH
#define FW_VERSION_PATCH 0
#endif

// Build type identifiers
#define BUILD_TYPE_RELEASE          0x00
#define BUILD_TYPE_DEBUG            0x01
#define BUILD_TYPE_TEST_CYCLE       0x10
#define BUILD_TYPE_TEST_POWER       0x11
#define BUILD_TYPE_TEST_FAILBACK_GOOD 0x20
#define BUILD_TYPE_TEST_FAILBACK_BAD  0x21

// Error codes
#define ERR_NONE                    0x00
#define ERR_LORA_INIT_FAIL          0x01
#define ERR_NVRAM_INIT_FAIL         0x02
#define ERR_FLASH_INIT_FAIL         0x03
#define ERR_SENSOR_FAIL             0x04
#define ERR_CRYPTO_FAIL             0x05
#define ERR_OTA_FAIL                0x06
#define ERR_WATCHDOG_RESET          0x07
#define ERR_HARDFAULT               0x08
#define ERR_STACK_OVERFLOW          0x09
#define ERR_VALIDATION_TIMEOUT      0x0A
#define ERR_ROLLBACK_TRIGGERED      0x0B

// Reset reason codes (from nRF52 RESETREAS register)
#define RESET_REASON_POWER_ON       0x01
#define RESET_REASON_PIN_RESET      0x02
#define RESET_REASON_WATCHDOG       0x04
#define RESET_REASON_SOFT_RESET     0x08
#define RESET_REASON_LOCKUP         0x10
#define RESET_REASON_GPIO_WAKE      0x20
#define RESET_REASON_DIF            0x40

// Debug log structure (stored in FRAM at 0x00C0, 64 bytes)
struct DebugLogData {
    uint32_t magic;             // 0x44424C47 ("DBLG")
    uint8_t  version;           // Log format version
    uint8_t  buildType;         // Build type identifier
    uint8_t  fwVersionMajor;    // Firmware version
    uint8_t  fwVersionMinor;
    uint8_t  fwVersionPatch;
    uint8_t  lastResetReason;   // Last reset reason
    uint8_t  lastError;         // Last error code
    uint8_t  reserved1;
    uint32_t bootCount;         // Total boot count
    uint32_t errorCount;        // Total error count
    uint32_t lastBootTime;      // Timestamp of last boot (uptime seconds)
    uint32_t lastErrorTime;     // Timestamp of last error
    uint32_t uptimeTotal;       // Total uptime in seconds (accumulated)
    uint8_t  errorHistory[8];   // Last 8 error codes (circular)
    uint8_t  errorHistoryIdx;   // Current index in error history
    uint8_t  validationPending; // 1 if firmware validation pending
    uint8_t  rollbackCount;     // Number of rollbacks performed
    uint8_t  reserved2[5];
    uint32_t crc;               // CRC32 of this structure
} __attribute__((packed));

// Static assert to ensure structure fits in 64 bytes
static_assert(sizeof(DebugLogData) <= 64, "DebugLogData exceeds 64 bytes");

/**
 * @brief Initialize debug log (call early in boot)
 * 
 * Reads existing log from FRAM, increments boot count,
 * records reset reason, and writes back.
 */
void debugLog_init(void);

/**
 * @brief Record an error
 * @param errorCode Error code from ERR_* defines
 */
void debugLog_recordError(uint8_t errorCode);

/**
 * @brief Get last error code
 * @return Last recorded error code
 */
uint8_t debugLog_getLastError(void);

/**
 * @brief Get boot count
 * @return Total number of boots
 */
uint32_t debugLog_getBootCount(void);

/**
 * @brief Get firmware version string
 * @param buffer Output buffer (at least 16 bytes)
 * @return Pointer to buffer
 */
char* debugLog_getVersionString(char* buffer);

/**
 * @brief Get build type string
 * @param buffer Output buffer (at least 24 bytes)
 * @return Pointer to buffer
 */
char* debugLog_getBuildTypeString(char* buffer);

/**
 * @brief Mark firmware as validated (call after successful boot)
 * 
 * Clears the validation pending flag. If not called within
 * timeout, bootloader will trigger rollback.
 */
void debugLog_markValidated(void);

/**
 * @brief Check if validation is pending
 * @return true if firmware needs validation
 */
bool debugLog_isValidationPending(void);

/**
 * @brief Set validation pending flag (called by bootloader before new FW)
 */
void debugLog_setValidationPending(void);

/**
 * @brief Record a rollback event
 */
void debugLog_recordRollback(void);

/**
 * @brief Get rollback count
 * @return Number of rollbacks performed
 */
uint8_t debugLog_getRollbackCount(void);

/**
 * @brief Update accumulated uptime
 * @param seconds Seconds to add to total uptime
 */
void debugLog_addUptime(uint32_t seconds);

/**
 * @brief Get the raw debug log data (for BLE transmission)
 * @param data Output structure
 */
void debugLog_getData(DebugLogData* data);

/**
 * @brief Get reset reason as string
 * @param reason Reset reason code
 * @param buffer Output buffer (at least 24 bytes)
 * @return Pointer to buffer
 */
char* debugLog_getResetReasonString(uint8_t reason, char* buffer);

#endif // DEBUG_LOG_H
