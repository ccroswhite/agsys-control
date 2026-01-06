/**
 * @file debug_log.cpp
 * @brief Persistent debug logging to FRAM
 */

#include "debug_log.h"
#include "nvram_layout.h"
#include "nvram.h"
#include <string.h>
#include <Arduino.h>

// FRAM address for debug log (using reserved protected region)
#define DEBUG_LOG_ADDR      NVRAM_PROTECTED_RESERVED
#define DEBUG_LOG_MAGIC     0x44424C47  // "DBLG"
#define DEBUG_LOG_VERSION   1

// External NVRAM instance (from main.cpp)
extern NVRAM nvram;

// Local copy of debug log data
static DebugLogData s_logData;
static bool s_initialized = false;

// Simple CRC32 implementation
static uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

// Read debug log from FRAM
static bool readFromFRAM() {
    uint8_t buffer[sizeof(DebugLogData)];
    
    if (!nvram.read(DEBUG_LOG_ADDR, buffer, sizeof(buffer))) {
        return false;
    }
    
    memcpy(&s_logData, buffer, sizeof(s_logData));
    
    // Verify magic and CRC
    if (s_logData.magic != DEBUG_LOG_MAGIC) {
        return false;
    }
    
    uint32_t storedCrc = s_logData.crc;
    s_logData.crc = 0;
    uint32_t calcCrc = crc32((uint8_t*)&s_logData, sizeof(s_logData) - 4);
    s_logData.crc = storedCrc;
    
    return (storedCrc == calcCrc);
}

// Write debug log to FRAM
static bool writeToFRAM() {
    // Calculate CRC (excluding CRC field itself)
    s_logData.crc = 0;
    s_logData.crc = crc32((uint8_t*)&s_logData, sizeof(s_logData) - 4);
    
    return nvram.write(DEBUG_LOG_ADDR, (uint8_t*)&s_logData, sizeof(s_logData));
}

// Initialize with defaults
static void initDefaults() {
    memset(&s_logData, 0, sizeof(s_logData));
    s_logData.magic = DEBUG_LOG_MAGIC;
    s_logData.version = DEBUG_LOG_VERSION;
    
    // Set build type based on compile flags
    #if defined(TEST_MODE_CYCLE_READINGS)
    s_logData.buildType = BUILD_TYPE_TEST_CYCLE;
    #elif defined(TEST_MODE_POWER_ALL)
    s_logData.buildType = BUILD_TYPE_TEST_POWER;
    #elif defined(TEST_MODE_FAILBACK_GOOD)
    s_logData.buildType = BUILD_TYPE_TEST_FAILBACK_GOOD;
    #elif defined(TEST_MODE_FAILBACK_BAD)
    s_logData.buildType = BUILD_TYPE_TEST_FAILBACK_BAD;
    #elif defined(RELEASE_BUILD)
    s_logData.buildType = BUILD_TYPE_RELEASE;
    #else
    s_logData.buildType = BUILD_TYPE_DEBUG;
    #endif
    
    s_logData.fwVersionMajor = FW_VERSION_MAJOR;
    s_logData.fwVersionMinor = FW_VERSION_MINOR;
    s_logData.fwVersionPatch = FW_VERSION_PATCH;
}

// Get reset reason from nRF52 registers
static uint8_t getResetReason() {
    uint32_t reason = NRF_POWER->RESETREAS;
    
    // Clear the reset reason register
    NRF_POWER->RESETREAS = 0xFFFFFFFF;
    
    if (reason & (1 << 0)) return RESET_REASON_PIN_RESET;
    if (reason & (1 << 1)) return RESET_REASON_WATCHDOG;
    if (reason & (1 << 2)) return RESET_REASON_SOFT_RESET;
    if (reason & (1 << 3)) return RESET_REASON_LOCKUP;
    if (reason & (1 << 16)) return RESET_REASON_GPIO_WAKE;
    if (reason & (1 << 18)) return RESET_REASON_DIF;
    
    return RESET_REASON_POWER_ON;
}

void debugLog_init(void) {
    if (s_initialized) return;
    
    // Try to read existing log from FRAM
    if (!readFromFRAM()) {
        // Initialize with defaults if read fails
        initDefaults();
    }
    
    // Update version info (in case firmware was updated)
    s_logData.fwVersionMajor = FW_VERSION_MAJOR;
    s_logData.fwVersionMinor = FW_VERSION_MINOR;
    s_logData.fwVersionPatch = FW_VERSION_PATCH;
    
    // Update build type
    #if defined(TEST_MODE_CYCLE_READINGS)
    s_logData.buildType = BUILD_TYPE_TEST_CYCLE;
    #elif defined(TEST_MODE_POWER_ALL)
    s_logData.buildType = BUILD_TYPE_TEST_POWER;
    #elif defined(TEST_MODE_FAILBACK_GOOD)
    s_logData.buildType = BUILD_TYPE_TEST_FAILBACK_GOOD;
    #elif defined(TEST_MODE_FAILBACK_BAD)
    s_logData.buildType = BUILD_TYPE_TEST_FAILBACK_BAD;
    #elif defined(RELEASE_BUILD)
    s_logData.buildType = BUILD_TYPE_RELEASE;
    #else
    s_logData.buildType = BUILD_TYPE_DEBUG;
    #endif
    
    // Record reset reason
    s_logData.lastResetReason = getResetReason();
    
    // Check for watchdog reset (indicates previous crash)
    if (s_logData.lastResetReason == RESET_REASON_WATCHDOG) {
        debugLog_recordError(ERR_WATCHDOG_RESET);
    }
    
    // Increment boot count
    s_logData.bootCount++;
    s_logData.lastBootTime = millis() / 1000;
    
    // Write updated log to FRAM
    writeToFRAM();
    
    s_initialized = true;
}

void debugLog_recordError(uint8_t errorCode) {
    if (!s_initialized) return;
    
    s_logData.lastError = errorCode;
    s_logData.lastErrorTime = millis() / 1000;
    s_logData.errorCount++;
    
    // Add to error history (circular buffer)
    s_logData.errorHistory[s_logData.errorHistoryIdx] = errorCode;
    s_logData.errorHistoryIdx = (s_logData.errorHistoryIdx + 1) % 8;
    
    writeToFRAM();
}

uint8_t debugLog_getLastError(void) {
    return s_logData.lastError;
}

uint32_t debugLog_getBootCount(void) {
    return s_logData.bootCount;
}

char* debugLog_getVersionString(char* buffer) {
    sprintf(buffer, "%d.%d.%d", 
            s_logData.fwVersionMajor,
            s_logData.fwVersionMinor,
            s_logData.fwVersionPatch);
    return buffer;
}

char* debugLog_getBuildTypeString(char* buffer) {
    switch (s_logData.buildType) {
        case BUILD_TYPE_RELEASE:
            strcpy(buffer, "release");
            break;
        case BUILD_TYPE_DEBUG:
            strcpy(buffer, "debug");
            break;
        case BUILD_TYPE_TEST_CYCLE:
            strcpy(buffer, "test-cycle-readings");
            break;
        case BUILD_TYPE_TEST_POWER:
            strcpy(buffer, "test-power-all");
            break;
        case BUILD_TYPE_TEST_FAILBACK_GOOD:
            strcpy(buffer, "test-failback-good");
            break;
        case BUILD_TYPE_TEST_FAILBACK_BAD:
            strcpy(buffer, "test-failback-bad");
            break;
        default:
            strcpy(buffer, "unknown");
            break;
    }
    return buffer;
}

void debugLog_markValidated(void) {
    if (!s_initialized) return;
    
    s_logData.validationPending = 0;
    writeToFRAM();
}

bool debugLog_isValidationPending(void) {
    return s_logData.validationPending != 0;
}

void debugLog_setValidationPending(void) {
    if (!s_initialized) return;
    
    s_logData.validationPending = 1;
    writeToFRAM();
}

void debugLog_recordRollback(void) {
    if (!s_initialized) return;
    
    s_logData.rollbackCount++;
    debugLog_recordError(ERR_ROLLBACK_TRIGGERED);
}

uint8_t debugLog_getRollbackCount(void) {
    return s_logData.rollbackCount;
}

void debugLog_addUptime(uint32_t seconds) {
    if (!s_initialized) return;
    
    s_logData.uptimeTotal += seconds;
    writeToFRAM();
}

void debugLog_getData(DebugLogData* data) {
    memcpy(data, &s_logData, sizeof(DebugLogData));
}

char* debugLog_getResetReasonString(uint8_t reason, char* buffer) {
    switch (reason) {
        case RESET_REASON_POWER_ON:
            strcpy(buffer, "power-on");
            break;
        case RESET_REASON_PIN_RESET:
            strcpy(buffer, "pin-reset");
            break;
        case RESET_REASON_WATCHDOG:
            strcpy(buffer, "watchdog");
            break;
        case RESET_REASON_SOFT_RESET:
            strcpy(buffer, "soft-reset");
            break;
        case RESET_REASON_LOCKUP:
            strcpy(buffer, "lockup");
            break;
        case RESET_REASON_GPIO_WAKE:
            strcpy(buffer, "gpio-wake");
            break;
        case RESET_REASON_DIF:
            strcpy(buffer, "debug-if");
            break;
        default:
            strcpy(buffer, "unknown");
            break;
    }
    return buffer;
}
