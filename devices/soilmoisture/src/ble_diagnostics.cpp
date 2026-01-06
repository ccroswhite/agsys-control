/**
 * @file ble_diagnostics.cpp
 * @brief BLE Diagnostics Service implementation
 */

#include "ble_diagnostics.h"
#include "debug_log.h"
#include <string.h>

// Custom UUID base: 12340000-1234-5678-9ABC-DEF012345678
const uint8_t BLEDiagnosticsService::UUID128_BASE[16] = {
    0x78, 0x56, 0x34, 0x12, 0xF0, 0xDE, 0xBC, 0x9A,
    0x78, 0x56, 0x34, 0x12, 0x00, 0x00, 0x34, 0x12
};

// Global instance
BLEDiagnosticsService bleDiagnostics;

BLEDiagnosticsService::BLEDiagnosticsService() 
    : BLEService(UUID128_BASE),
      _versionChar(UUID128_BASE),
      _buildTypeChar(UUID128_BASE),
      _bootCountChar(UUID128_BASE),
      _lastErrorChar(UUID128_BASE),
      _debugLogChar(UUID128_BASE)
{
    // Set characteristic UUIDs (modify the 12th and 13th bytes)
    uint8_t uuid[16];
    
    memcpy(uuid, UUID128_BASE, 16);
    uuid[12] = DIAG_UUID_VERSION & 0xFF;
    uuid[13] = (DIAG_UUID_VERSION >> 8) & 0xFF;
    _versionChar.setUuid(uuid);
    
    memcpy(uuid, UUID128_BASE, 16);
    uuid[12] = DIAG_UUID_BUILD_TYPE & 0xFF;
    uuid[13] = (DIAG_UUID_BUILD_TYPE >> 8) & 0xFF;
    _buildTypeChar.setUuid(uuid);
    
    memcpy(uuid, UUID128_BASE, 16);
    uuid[12] = DIAG_UUID_BOOT_COUNT & 0xFF;
    uuid[13] = (DIAG_UUID_BOOT_COUNT >> 8) & 0xFF;
    _bootCountChar.setUuid(uuid);
    
    memcpy(uuid, UUID128_BASE, 16);
    uuid[12] = DIAG_UUID_LAST_ERROR & 0xFF;
    uuid[13] = (DIAG_UUID_LAST_ERROR >> 8) & 0xFF;
    _lastErrorChar.setUuid(uuid);
    
    memcpy(uuid, UUID128_BASE, 16);
    uuid[12] = DIAG_UUID_DEBUG_LOG & 0xFF;
    uuid[13] = (DIAG_UUID_DEBUG_LOG >> 8) & 0xFF;
    _debugLogChar.setUuid(uuid);
}

err_t BLEDiagnosticsService::begin() {
    // Start the service
    VERIFY_STATUS(BLEService::begin());
    
    // Version characteristic (read-only string)
    _versionChar.setProperties(CHR_PROPS_READ);
    _versionChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    _versionChar.setMaxLen(16);
    _versionChar.setFixedLen(false);
    VERIFY_STATUS(_versionChar.begin());
    
    // Build type characteristic (read-only string)
    _buildTypeChar.setProperties(CHR_PROPS_READ);
    _buildTypeChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    _buildTypeChar.setMaxLen(24);
    _buildTypeChar.setFixedLen(false);
    VERIFY_STATUS(_buildTypeChar.begin());
    
    // Boot count characteristic (read-only uint32)
    _bootCountChar.setProperties(CHR_PROPS_READ);
    _bootCountChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    _bootCountChar.setFixedLen(4);
    VERIFY_STATUS(_bootCountChar.begin());
    
    // Last error characteristic (read-only uint8)
    _lastErrorChar.setProperties(CHR_PROPS_READ);
    _lastErrorChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    _lastErrorChar.setFixedLen(1);
    VERIFY_STATUS(_lastErrorChar.begin());
    
    // Debug log characteristic (read-only struct)
    _debugLogChar.setProperties(CHR_PROPS_READ);
    _debugLogChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    _debugLogChar.setFixedLen(sizeof(DebugLogData));
    VERIFY_STATUS(_debugLogChar.begin());
    
    // Initial update
    update();
    
    return ERROR_NONE;
}

void BLEDiagnosticsService::update() {
    // Update version string
    debugLog_getVersionString(_versionStr);
    _versionChar.write(_versionStr, strlen(_versionStr));
    
    // Update build type string
    debugLog_getBuildTypeString(_buildTypeStr);
    _buildTypeChar.write(_buildTypeStr, strlen(_buildTypeStr));
    
    // Update boot count
    uint32_t bootCount = debugLog_getBootCount();
    _bootCountChar.write32(bootCount);
    
    // Update last error
    uint8_t lastError = debugLog_getLastError();
    _lastErrorChar.write8(lastError);
    
    // Update full debug log
    DebugLogData logData;
    debugLog_getData(&logData);
    _debugLogChar.write(&logData, sizeof(logData));
}
