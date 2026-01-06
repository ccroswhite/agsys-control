/**
 * @file ble_diagnostics.h
 * @brief BLE Diagnostics Service for querying device status
 * 
 * Provides BLE characteristics for:
 * - Firmware version query
 * - Boot count
 * - Last error code
 * - Debug log dump
 * - Reset reason
 */

#ifndef BLE_DIAGNOSTICS_H
#define BLE_DIAGNOSTICS_H

#include <bluefruit.h>

// Custom UUID for Diagnostics Service
// Base: 12340000-1234-5678-9ABC-DEF012345678
// Service: 12340001-...
// Characteristics: 12340002-... through 12340006-...

#define DIAG_UUID_SERVICE       0x0001
#define DIAG_UUID_VERSION       0x0002  // Read: version string
#define DIAG_UUID_BUILD_TYPE    0x0003  // Read: build type string
#define DIAG_UUID_BOOT_COUNT    0x0004  // Read: 4-byte boot count
#define DIAG_UUID_LAST_ERROR    0x0005  // Read: 1-byte error code
#define DIAG_UUID_DEBUG_LOG     0x0006  // Read: full debug log struct

/**
 * @brief BLE Diagnostics Service class
 */
class BLEDiagnosticsService : public BLEService {
public:
    BLEDiagnosticsService();
    
    /**
     * @brief Initialize the service and characteristics
     * @return err_t (0 on success)
     */
    err_t begin() override;
    
    /**
     * @brief Update all characteristic values from debug log
     * Call this periodically or after state changes
     */
    void update();

private:
    // Base UUID for custom service
    static const uint8_t UUID128_BASE[16];
    
    // Characteristics
    BLECharacteristic _versionChar;
    BLECharacteristic _buildTypeChar;
    BLECharacteristic _bootCountChar;
    BLECharacteristic _lastErrorChar;
    BLECharacteristic _debugLogChar;
    
    // Buffers for characteristic data
    char _versionStr[16];
    char _buildTypeStr[24];
};

// Global instance
extern BLEDiagnosticsService bleDiagnostics;

#endif // BLE_DIAGNOSTICS_H
