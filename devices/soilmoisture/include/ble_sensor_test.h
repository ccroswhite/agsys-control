/**
 * @file ble_sensor_test.h
 * @brief BLE Sensor Test Service
 * 
 * Provides BLE characteristics for testing sensor parameters:
 * - H-bridge frequency control (read/write)
 * - Trigger measurement (write)
 * - Read measurement result (read/notify)
 */

#ifndef BLE_SENSOR_TEST_H
#define BLE_SENSOR_TEST_H

#include <bluefruit.h>

// Service UUID: 12340002-1234-5678-9ABC-DEF012345678
#define SENSOR_TEST_UUID_SERVICE    0x0002

// Characteristic UUIDs
#define SENSOR_TEST_UUID_FREQUENCY  0x0201  // R/W uint32 - frequency in Hz
#define SENSOR_TEST_UUID_TRIGGER    0x0202  // W uint8 - write 1 to trigger measurement
#define SENSOR_TEST_UUID_RESULT     0x0203  // R/N uint16 - raw ADC result
#define SENSOR_TEST_UUID_MOISTURE   0x0204  // R/N uint8 - moisture percentage

class BLESensorTestService : public BLEService {
public:
    BLESensorTestService();
    
    err_t begin();
    
    // Update result characteristics after measurement
    void updateResults(uint16_t rawAdc, uint8_t moisturePercent);
    
    // Get current frequency setting
    uint32_t getFrequency();

private:
    BLECharacteristic _frequencyChar;
    BLECharacteristic _triggerChar;
    BLECharacteristic _resultChar;
    BLECharacteristic _moistureChar;
    
    static const uint8_t UUID128_BASE[16];
    
    // Callbacks
    static void frequencyWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
    static void triggerWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
};

// Global instance
extern BLESensorTestService bleSensorTest;

#endif // BLE_SENSOR_TEST_H
