/**
 * @file config_manager.h
 * @brief Configuration manager for persistent device settings
 * 
 * Manages device configuration stored in protected FRAM regions.
 * Configuration survives firmware updates.
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "nvram_layout.h"

/* ==========================================================================
 * DATA STRUCTURES
 * ========================================================================== */

/**
 * Device identity - from nRF52832 FICR (factory-programmed, immutable)
 * No FRAM storage needed - identity is tied to the chip.
 */
struct DeviceIdentity {
    uint64_t deviceId;          // 64-bit FICR device ID
    uint8_t  deviceIdBytes[8];  // Device ID as bytes (big-endian)
    uint8_t  deviceType;        // Device type (compile-time constant)
    uint8_t  hwRevision;        // Hardware revision (compile-time constant)
};

/**
 * Factory calibration - set at manufacturing or field calibration
 */
struct FactoryCalibration {
    uint16_t moistureDry;       // ADC value when dry
    uint16_t moistureWet;       // ADC value when saturated
    int16_t  moistureTempCoef;  // Temperature coefficient (0.01 units)
    int16_t  batteryOffset;     // Battery voltage offset (mV)
    uint16_t batteryScale;      // Battery voltage scale (0.001 units, 1000 = 1.0)
    int16_t  tempOffset;        // Temperature offset (0.1°C)
    int32_t  loraFreqOffset;    // LoRa frequency offset (Hz)
    bool     isCalibrated;      // Calibration data valid
};

// Customer and Location info is managed in the backend database, keyed by device ID.
// No local storage needed - reduces FRAM usage and simplifies device management.

/**
 * User configuration - can be changed by user/controller
 */
struct UserConfig {
    uint32_t sleepIntervalSec;      // Sleep interval in seconds
    uint32_t reportIntervalSec;     // Report interval in seconds
    uint16_t lowBatteryThreshMv;    // Low battery warning threshold
    uint16_t critBatteryThreshMv;   // Critical battery threshold
    uint8_t  moistureLowAlarm;      // Low moisture alarm (%)
    uint8_t  moistureHighAlarm;     // High moisture alarm (%)
    uint8_t  loraTxPower;           // LoRa TX power (dBm)
    uint8_t  loraSpreadingFactor;   // LoRa spreading factor
    uint32_t gatewayId;              // Paired gateway ID
    uint8_t  networkKey[16];        // Network encryption key
    bool     isPaired;              // Paired with controller
    bool     alarmsEnabled;         // Moisture alarms enabled
};

/**
 * Runtime state - may be cleared on major firmware updates
 */
struct RuntimeState {
    uint32_t bootCount;             // Total boot count
    uint32_t lastBootTime;          // Last boot timestamp
    uint32_t lastReportTime;        // Last successful report
    uint16_t lastAckedSequence;     // Last acknowledged sequence number
    uint16_t pendingLogCount;       // Number of pending log entries
    uint32_t currentFwVersion;      // Current firmware version (packed)
    uint32_t previousFwVersion;     // Previous firmware version
    uint8_t  otaStatus;             // Current OTA status
    uint8_t  otaProgress;           // OTA progress percentage
    uint32_t otaAnnounceId;         // Current OTA announce ID
    uint16_t otaChunksReceived;     // OTA chunks received
    uint16_t otaTotalChunks;        // OTA total chunks
};

/**
 * Device statistics - may be cleared on major firmware updates
 */
struct DeviceStats {
    uint32_t txSuccess;             // Successful transmissions
    uint32_t txFail;                // Failed transmissions
    uint32_t rxSuccess;             // Successful receptions
    uint32_t rxFail;                // Failed receptions
    uint16_t otaSuccess;            // Successful OTA updates
    uint16_t otaFail;               // Failed OTA updates
    uint16_t unexpectedResets;      // Unexpected reset count
    uint16_t lowBatteryEvents;      // Low battery event count
    uint16_t minBatteryMv;          // Minimum battery voltage seen
    int16_t  maxTemperature;        // Maximum temperature (0.1°C)
    int16_t  minTemperature;        // Minimum temperature (0.1°C)
    uint32_t uptimeHours;           // Total uptime in hours
};

/* ==========================================================================
 * CONFIGURATION MANAGER CLASS
 * ========================================================================== */

class ConfigManager {
public:
    ConfigManager();
    
    /**
     * Initialize the configuration manager
     * Loads all configuration from FRAM, initializes defaults if needed
     * @return true if successful
     */
    bool init();
    
    /**
     * Get 64-bit device ID from FICR
     */
    uint64_t getDeviceId() const { return _identity.deviceId; }
    
    /**
     * Check if factory calibration is present
     */
    bool isCalibrated() const { return _calibration.isCalibrated; }
    
    // ---- Identity (read-only, from FICR) ----
    
    const DeviceIdentity& getIdentity() const { return _identity; }
    const uint8_t* getDeviceIdBytes() const { return _identity.deviceIdBytes; }
    
    // ---- Factory Calibration ----
    
    const FactoryCalibration& getCalibration() const { return _calibration; }
    
    /**
     * Set factory calibration values
     * @return true if successful
     */
    bool setCalibration(const FactoryCalibration& cal);
    
    /**
     * Apply moisture calibration to raw ADC value
     * @param rawValue Raw ADC reading
     * @param temperature Current temperature (0.1°C) for compensation
     * @return Moisture percentage (0-100)
     */
    uint8_t applyMoistureCalibration(uint16_t rawValue, int16_t temperature = 250) const;
    
    /**
     * Apply battery calibration to raw ADC value
     * @param rawMv Raw battery voltage in mV
     * @return Calibrated battery voltage in mV
     */
    uint16_t applyBatteryCalibration(uint16_t rawMv) const;
    
    // Customer and Location info is managed in the backend database.
    // No local storage or accessors needed.
    
    // ---- User Configuration ----
    
    const UserConfig& getConfig() const { return _config; }
    UserConfig& getConfigMutable() { _configDirty = true; return _config; }
    
    /**
     * Set user configuration
     * @return true if successful
     */
    bool setConfig(const UserConfig& config);
    
    /**
     * Save user configuration to FRAM (if dirty)
     * @return true if successful
     */
    bool saveConfig();
    
    /**
     * Reset user configuration to defaults
     * @return true if successful
     */
    bool resetConfigToDefaults();
    
    // ---- Runtime State ----
    
    const RuntimeState& getState() const { return _state; }
    RuntimeState& getStateMutable() { _stateDirty = true; return _state; }
    
    /**
     * Save runtime state to FRAM (if dirty)
     */
    bool saveState();
    
    /**
     * Increment boot count and update last boot time
     */
    void recordBoot(uint32_t timestamp);
    
    /**
     * Record successful report
     */
    void recordReport(uint32_t timestamp, uint16_t sequence);
    
    // ---- Statistics ----
    
    const DeviceStats& getStats() const { return _stats; }
    DeviceStats& getStatsMutable() { _statsDirty = true; return _stats; }
    
    /**
     * Save statistics to FRAM (if dirty)
     */
    bool saveStats();
    
    /**
     * Record transmission result
     */
    void recordTx(bool success);
    
    /**
     * Record reception result
     */
    void recordRx(bool success);
    
    /**
     * Record OTA result
     */
    void recordOta(bool success);
    
    /**
     * Update battery/temperature statistics
     */
    void updateEnvironmentStats(uint16_t batteryMv, int16_t temperature);
    
    // ---- Firmware Version Management ----
    
    /**
     * Get packed firmware version
     */
    static uint32_t packVersion(uint8_t major, uint8_t minor, uint8_t patch);
    
    /**
     * Check if firmware version changed since last boot
     * @return true if version changed (migration may be needed)
     */
    bool checkVersionChange(uint8_t major, uint8_t minor, uint8_t patch);
    
    /**
     * Perform data migration if needed after firmware update
     */
    void migrateData(uint32_t fromVersion, uint32_t toVersion);
    
    // ---- Factory Reset ----
    
    /**
     * Perform factory reset (clears user config, state, stats, logs)
     * Does NOT clear identity or factory calibration
     */
    bool factoryReset();
    
    /**
     * Full erase (clears everything including identity)
     * Use with caution - device will need re-provisioning
     */
    bool fullErase();
    
private:
    // Loaded data
    DeviceIdentity _identity;
    FactoryCalibration _calibration;
    UserConfig _config;
    RuntimeState _state;
    DeviceStats _stats;
    
    // State flags
    bool _configDirty;
    bool _stateDirty;
    bool _statsDirty;
    
    // Internal methods
    void loadIdentity();  // From FICR, always succeeds
    bool loadCalibration();
    bool loadConfig();
    bool loadState();
    bool loadStats();
    
    bool saveCalibrationInternal();
    bool saveConfigInternal();
    bool saveStateInternal();
    bool saveStatsInternal();
    
    void initDefaultConfig();
    void initDefaultState();
    void initDefaultStats();
    
    uint32_t calculateCrc32(const uint8_t* data, int len);
    bool verifyCrc32(uint32_t addr, int dataLen, uint32_t crcAddr);
    void writeCrc32(uint32_t addr, int dataLen, uint32_t crcAddr);
};

// Global instance
extern ConfigManager configManager;

#endif // CONFIG_MANAGER_H
