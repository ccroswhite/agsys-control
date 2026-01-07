/**
 * @file ble_calibration.cpp
 * @brief BLE Calibration Service Implementation
 */

#include "ble_calibration.h"
#include "moisture_probe.h"
#include "moisture_cal.h"
#include <string.h>

// Custom UUID base: 12340000-1234-5678-9ABC-DEF012345678
const uint8_t BLECalibrationService::UUID128_BASE[16] = {
    0x78, 0x56, 0x34, 0x12, 0xF0, 0xDE, 0xBC, 0x9A,
    0x78, 0x56, 0x34, 0x12, 0x00, 0x00, 0x34, 0x12
};

// Global instance
BLECalibrationService bleCalibration;

// Static pointer for callbacks
static BLECalibrationService* s_instance = nullptr;

// Weak definition - can be overridden by main application
__attribute__((weak)) void onAutoCalibrationRequested(uint8_t probeIndex) {
    DEBUG_PRINTF("BLE: Auto-calibration requested for probe %d (not implemented)\n", probeIndex);
}

BLECalibrationService::BLECalibrationService() 
    : BLEService(UUID128_BASE),
      _probeSelectChar(UUID128_BASE),
      _rawFrequencyChar(UUID128_BASE),
      _fAirChar(UUID128_BASE),
      _fDryChar(UUID128_BASE),
      _fWetChar(UUID128_BASE),
      _commandChar(UUID128_BASE),
      _moistureChar(UUID128_BASE),
      _statusChar(UUID128_BASE),
      _allMoistureChar(UUID128_BASE),
      _selectedProbe(0)
{
    s_instance = this;
    
    // Set characteristic UUIDs
    uint8_t uuid[16];
    
    memcpy(uuid, UUID128_BASE, 16);
    uuid[12] = CAL_UUID_PROBE_SELECT & 0xFF;
    uuid[13] = (CAL_UUID_PROBE_SELECT >> 8) & 0xFF;
    _probeSelectChar.setUuid(uuid);
    
    memcpy(uuid, UUID128_BASE, 16);
    uuid[12] = CAL_UUID_RAW_FREQUENCY & 0xFF;
    uuid[13] = (CAL_UUID_RAW_FREQUENCY >> 8) & 0xFF;
    _rawFrequencyChar.setUuid(uuid);
    
    memcpy(uuid, UUID128_BASE, 16);
    uuid[12] = CAL_UUID_F_AIR & 0xFF;
    uuid[13] = (CAL_UUID_F_AIR >> 8) & 0xFF;
    _fAirChar.setUuid(uuid);
    
    memcpy(uuid, UUID128_BASE, 16);
    uuid[12] = CAL_UUID_F_DRY & 0xFF;
    uuid[13] = (CAL_UUID_F_DRY >> 8) & 0xFF;
    _fDryChar.setUuid(uuid);
    
    memcpy(uuid, UUID128_BASE, 16);
    uuid[12] = CAL_UUID_F_WET & 0xFF;
    uuid[13] = (CAL_UUID_F_WET >> 8) & 0xFF;
    _fWetChar.setUuid(uuid);
    
    memcpy(uuid, UUID128_BASE, 16);
    uuid[12] = CAL_UUID_COMMAND & 0xFF;
    uuid[13] = (CAL_UUID_COMMAND >> 8) & 0xFF;
    _commandChar.setUuid(uuid);
    
    memcpy(uuid, UUID128_BASE, 16);
    uuid[12] = CAL_UUID_MOISTURE & 0xFF;
    uuid[13] = (CAL_UUID_MOISTURE >> 8) & 0xFF;
    _moistureChar.setUuid(uuid);
    
    memcpy(uuid, UUID128_BASE, 16);
    uuid[12] = CAL_UUID_STATUS & 0xFF;
    uuid[13] = (CAL_UUID_STATUS >> 8) & 0xFF;
    _statusChar.setUuid(uuid);
    
    memcpy(uuid, UUID128_BASE, 16);
    uuid[12] = CAL_UUID_ALL_MOISTURE & 0xFF;
    uuid[13] = (CAL_UUID_ALL_MOISTURE >> 8) & 0xFF;
    _allMoistureChar.setUuid(uuid);
}

err_t BLECalibrationService::begin() {
    VERIFY_STATUS(BLEService::begin());
    
    // Probe Select (R/W uint8)
    _probeSelectChar.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
    _probeSelectChar.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    _probeSelectChar.setFixedLen(1);
    _probeSelectChar.setWriteCallback(probeSelectWriteCallback);
    VERIFY_STATUS(_probeSelectChar.begin());
    _probeSelectChar.write8(0);
    
    // Raw Frequency (R/N uint32)
    _rawFrequencyChar.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
    _rawFrequencyChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    _rawFrequencyChar.setFixedLen(4);
    VERIFY_STATUS(_rawFrequencyChar.begin());
    _rawFrequencyChar.write32(0);
    
    // F_Air (R/W uint32)
    _fAirChar.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
    _fAirChar.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    _fAirChar.setFixedLen(4);
    _fAirChar.setWriteCallback(fAirWriteCallback);
    VERIFY_STATUS(_fAirChar.begin());
    
    // F_Dry (R/W uint32)
    _fDryChar.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
    _fDryChar.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    _fDryChar.setFixedLen(4);
    _fDryChar.setWriteCallback(fDryWriteCallback);
    VERIFY_STATUS(_fDryChar.begin());
    
    // F_Wet (R/W uint32)
    _fWetChar.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
    _fWetChar.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    _fWetChar.setFixedLen(4);
    _fWetChar.setWriteCallback(fWetWriteCallback);
    VERIFY_STATUS(_fWetChar.begin());
    
    // Command (W uint8)
    _commandChar.setProperties(CHR_PROPS_WRITE);
    _commandChar.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
    _commandChar.setFixedLen(1);
    _commandChar.setWriteCallback(commandWriteCallback);
    VERIFY_STATUS(_commandChar.begin());
    
    // Moisture (R/N uint8)
    _moistureChar.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
    _moistureChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    _moistureChar.setFixedLen(1);
    VERIFY_STATUS(_moistureChar.begin());
    _moistureChar.write8(255);  // 255 = not calibrated
    
    // Status (R uint8)
    _statusChar.setProperties(CHR_PROPS_READ);
    _statusChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    _statusChar.setFixedLen(1);
    VERIFY_STATUS(_statusChar.begin());
    
    // All Moisture (R/N uint8[4])
    _allMoistureChar.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
    _allMoistureChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    _allMoistureChar.setFixedLen(MAX_PROBES);
    VERIFY_STATUS(_allMoistureChar.begin());
    
    // Initialize with calibration data
    updateCalibrationChars();
    
    return ERROR_NONE;
}

void BLECalibrationService::updateCalibrationChars() {
    MoistureCalibration cal;
    if (moistureCal_get(_selectedProbe, &cal)) {
        _fAirChar.write32(cal.f_air);
        _fDryChar.write32(cal.f_dry);
        _fWetChar.write32(cal.f_wet);
        _statusChar.write8(cal.status);
    } else {
        _fAirChar.write32(0);
        _fDryChar.write32(0);
        _fWetChar.write32(0);
        _statusChar.write8(0);
    }
}

void BLECalibrationService::update() {
    updateCalibrationChars();
}

void BLECalibrationService::updateFrequency(uint32_t frequency) {
    _rawFrequencyChar.write32(frequency);
    _rawFrequencyChar.notify32(frequency);
}

void BLECalibrationService::updateMoisture(uint8_t moisturePercent) {
    _moistureChar.write8(moisturePercent);
    _moistureChar.notify8(moisturePercent);
}

void BLECalibrationService::updateAllMoisture(uint8_t moisture[MAX_PROBES]) {
    _allMoistureChar.write(moisture, MAX_PROBES);
    _allMoistureChar.notify(moisture, MAX_PROBES);
}

uint8_t BLECalibrationService::getSelectedProbe() {
    return _selectedProbe;
}

void BLECalibrationService::probeSelectWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    if (len >= 1 && s_instance) {
        uint8_t probe = data[0];
        if (probe < NUM_MOISTURE_PROBES) {
            s_instance->_selectedProbe = probe;
            s_instance->_probeSelectChar.write8(probe);
            s_instance->updateCalibrationChars();
            DEBUG_PRINTF("BLE: Selected probe %d\n", probe);
        }
    }
}

void BLECalibrationService::fAirWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    if (len >= 4 && s_instance) {
        uint32_t f_air = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        uint8_t probe = s_instance->_selectedProbe;
        
        if (moistureCal_setAir(probe, f_air)) {
            s_instance->_fAirChar.write32(f_air);
            DEBUG_PRINTF("BLE: Probe %d f_air set to %lu Hz\n", probe, f_air);
        }
    }
}

void BLECalibrationService::fDryWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    if (len >= 4 && s_instance) {
        uint32_t f_dry = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        uint8_t probe = s_instance->_selectedProbe;
        
        if (moistureCal_setDry(probe, f_dry)) {
            s_instance->_fDryChar.write32(f_dry);
            DEBUG_PRINTF("BLE: Probe %d f_dry set to %lu Hz\n", probe, f_dry);
        }
    }
}

void BLECalibrationService::fWetWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    if (len >= 4 && s_instance) {
        uint32_t f_wet = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        uint8_t probe = s_instance->_selectedProbe;
        
        if (moistureCal_setWet(probe, f_wet)) {
            s_instance->_fWetChar.write32(f_wet);
            DEBUG_PRINTF("BLE: Probe %d f_wet set to %lu Hz\n", probe, f_wet);
        }
    }
}

void BLECalibrationService::commandWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    if (len < 1 || !s_instance) return;
    
    uint8_t cmd = data[0];
    uint8_t probe = s_instance->_selectedProbe;
    
    switch (cmd) {
        case CAL_CMD_CAPTURE_AIR: {
            // Measure current frequency and save as f_air
            moistureProbe_powerOn();
            uint32_t freq = moistureProbe_measureFrequency(probe, PROBE_MEASUREMENT_MS);
            moistureProbe_powerOff();
            
            if (freq > 0 && moistureCal_setAir(probe, freq)) {
                s_instance->updateFrequency(freq);
                s_instance->updateCalibrationChars();
                DEBUG_PRINTF("BLE: Captured f_air = %lu Hz for probe %d\n", freq, probe);
            }
            break;
        }
        
        case CAL_CMD_CAPTURE_DRY: {
            // Measure current frequency and save as f_dry
            moistureProbe_powerOn();
            uint32_t freq = moistureProbe_measureFrequency(probe, PROBE_MEASUREMENT_MS);
            moistureProbe_powerOff();
            
            if (freq > 0 && moistureCal_setDry(probe, freq)) {
                s_instance->updateFrequency(freq);
                s_instance->updateCalibrationChars();
                DEBUG_PRINTF("BLE: Captured f_dry = %lu Hz for probe %d\n", freq, probe);
            }
            break;
        }
        
        case CAL_CMD_CAPTURE_WET: {
            // Measure current frequency and save as f_wet
            moistureProbe_powerOn();
            uint32_t freq = moistureProbe_measureFrequency(probe, PROBE_MEASUREMENT_MS);
            moistureProbe_powerOff();
            
            if (freq > 0 && moistureCal_setWet(probe, freq)) {
                s_instance->updateFrequency(freq);
                s_instance->updateCalibrationChars();
                DEBUG_PRINTF("BLE: Captured f_wet = %lu Hz for probe %d\n", freq, probe);
            }
            break;
        }
        
        case CAL_CMD_CLEAR_PROBE: {
            moistureCal_clear(probe);
            s_instance->updateCalibrationChars();
            DEBUG_PRINTF("BLE: Cleared calibration for probe %d\n", probe);
            break;
        }
        
        case CAL_CMD_CLEAR_ALL: {
            moistureCal_clearAll();
            s_instance->updateCalibrationChars();
            DEBUG_PRINTLN("BLE: Cleared all calibration");
            break;
        }
        
        case CAL_CMD_TRIGGER_MEASURE: {
            // Take a measurement and update characteristics
            ProbeReading reading;
            if (moistureProbe_readSingle(probe, &reading)) {
                s_instance->updateFrequency(reading.frequency);
                s_instance->updateMoisture(reading.moisturePercent);
                DEBUG_PRINTF("BLE: Probe %d - freq=%lu, moisture=%d%%\n", 
                             probe, reading.frequency, reading.moisturePercent);
            }
            break;
        }
        
        case CAL_CMD_START_AUTO_CAL: {
            // Request auto-calibration (handled by main application)
            onAutoCalibrationRequested(probe);
            break;
        }
        
        default:
            DEBUG_PRINTF("BLE: Unknown command %d\n", cmd);
            break;
    }
}
