/**
 * @file ble_calibration.h
 * @brief BLE Calibration Service for Moisture Probes
 * 
 * Provides BLE characteristics for:
 * - Reading raw frequency from probes
 * - Reading/writing calibration values (f_air, f_dry, f_wet)
 * - Triggering calibration capture
 * - Reading moisture percentage
 * - Reading calibration status
 */

#ifndef BLE_CALIBRATION_H
#define BLE_CALIBRATION_H

#include <bluefruit.h>
#include "config.h"

// Service UUID: 12340003-1234-5678-9ABC-DEF012345678
#define CAL_SERVICE_UUID            0x0003

// Characteristic UUIDs
#define CAL_UUID_PROBE_SELECT       0x0301  // R/W uint8 - select probe (0-3)
#define CAL_UUID_RAW_FREQUENCY      0x0302  // R/N uint32 - current frequency (Hz)
#define CAL_UUID_F_AIR              0x0303  // R/W uint32 - f_air calibration
#define CAL_UUID_F_DRY              0x0304  // R/W uint32 - f_dry calibration
#define CAL_UUID_F_WET              0x0305  // R/W uint32 - f_wet calibration
#define CAL_UUID_COMMAND            0x0306  // W uint8 - calibration command
#define CAL_UUID_MOISTURE           0x0307  // R/N uint8 - moisture percentage
#define CAL_UUID_STATUS             0x0308  // R uint8 - calibration status flags
#define CAL_UUID_ALL_MOISTURE       0x0309  // R/N uint8[4] - all probes moisture %

// Calibration commands
#define CAL_CMD_CAPTURE_AIR         1   // Capture current frequency as f_air
#define CAL_CMD_CAPTURE_DRY         2   // Capture current frequency as f_dry
#define CAL_CMD_CAPTURE_WET         3   // Capture current frequency as f_wet
#define CAL_CMD_CLEAR_PROBE         4   // Clear calibration for selected probe
#define CAL_CMD_CLEAR_ALL           5   // Clear all calibration data
#define CAL_CMD_TRIGGER_MEASURE     6   // Trigger a measurement
#define CAL_CMD_START_AUTO_CAL      7   // Start auto f_air calibration

class BLECalibrationService : public BLEService {
public:
    BLECalibrationService();
    
    err_t begin();
    
    // Update characteristics with current values
    void update();
    
    // Update frequency for selected probe
    void updateFrequency(uint32_t frequency);
    
    // Update moisture for selected probe
    void updateMoisture(uint8_t moisturePercent);
    
    // Update all probes moisture
    void updateAllMoisture(uint8_t moisture[MAX_PROBES]);
    
    // Get currently selected probe
    uint8_t getSelectedProbe();

private:
    BLECharacteristic _probeSelectChar;
    BLECharacteristic _rawFrequencyChar;
    BLECharacteristic _fAirChar;
    BLECharacteristic _fDryChar;
    BLECharacteristic _fWetChar;
    BLECharacteristic _commandChar;
    BLECharacteristic _moistureChar;
    BLECharacteristic _statusChar;
    BLECharacteristic _allMoistureChar;
    
    uint8_t _selectedProbe;
    
    static const uint8_t UUID128_BASE[16];
    
    // Callbacks
    static void probeSelectWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
    static void fAirWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
    static void fDryWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
    static void fWetWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
    static void commandWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
    
    // Helper to update calibration characteristics for selected probe
    void updateCalibrationChars();
};

// Global instance
extern BLECalibrationService bleCalibration;

// Callback for auto-calibration request (implemented in main or calibration module)
extern void onAutoCalibrationRequested(uint8_t probeIndex);

#endif // BLE_CALIBRATION_H
