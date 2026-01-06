/**
 * @file config_manager.cpp
 * @brief Configuration manager implementation
 * 
 * Simplified version using FICR for device identity.
 * Customer/Location info managed in backend database.
 */

#include "config_manager.h"
#include "config.h"
#include "security.h"
#include <Arduino.h>
#include <SPI.h>

// External FRAM access functions (defined in nvram.cpp)
extern bool nvramRead(uint32_t addr, uint8_t* data, size_t len);
extern bool nvramWrite(uint32_t addr, const uint8_t* data, size_t len);

// Global instance
ConfigManager configManager;

// CRC32 lookup table
static const uint32_t crc32Table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7A89, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

// ============================================================================
// Constructor
// ============================================================================

ConfigManager::ConfigManager() {
    memset(&_identity, 0, sizeof(_identity));
    memset(&_calibration, 0, sizeof(_calibration));
    memset(&_config, 0, sizeof(_config));
    memset(&_state, 0, sizeof(_state));
    memset(&_stats, 0, sizeof(_stats));
    _configDirty = false;
    _stateDirty = false;
    _statsDirty = false;
}

bool ConfigManager::init() {
    // Load identity from FICR (always succeeds)
    loadIdentity();
    
    // Load configuration blocks from FRAM
    bool calOk = loadCalibration();
    bool configOk = loadConfig();
    bool stateOk = loadState();
    bool statsOk = loadStats();
    
    // Initialize defaults for any missing blocks
    if (!configOk) {
        initDefaultConfig();
        saveConfigInternal();
    }
    
    if (!stateOk) {
        initDefaultState();
        saveStateInternal();
    }
    
    if (!statsOk) {
        initDefaultStats();
        saveStatsInternal();
    }
    
    DEBUG_PRINTF("ConfigManager: deviceId=%016llX, calibrated=%d\n", 
                 _identity.deviceId, _calibration.isCalibrated);
    
    return true;
}

// ============================================================================
// Identity Management (from FICR)
// ============================================================================

void ConfigManager::loadIdentity() {
    // Get device ID from FICR (factory-programmed, immutable)
    _identity.deviceId = security_getDeviceId();
    security_getDeviceIdBytes(_identity.deviceIdBytes);
    
    // Device type and hardware revision are compile-time constants
    _identity.deviceType = DEVICE_TYPE_SOIL_MOISTURE;
    _identity.hwRevision = 1;  // TODO: Define HW_REVISION in config.h
}

// ============================================================================
// Calibration Management
// ============================================================================

bool ConfigManager::loadCalibration() {
    uint8_t buffer[NVRAM_CALIBRATION_SIZE];
    
    if (!nvramRead(NVRAM_CALIBRATION_ADDR, buffer, NVRAM_CALIBRATION_SIZE)) {
        return false;
    }
    
    // Check magic
    uint32_t magic = *(uint32_t*)&buffer[CAL_MAGIC_OFFSET];
    if (magic != NVRAM_MAGIC_CALIBRATION) {
        DEBUG_PRINTLN("Calibration: invalid magic");
        _calibration.isCalibrated = false;
        return false;
    }
    
    // Verify CRC
    if (!verifyCrc32(NVRAM_CALIBRATION_ADDR, NVRAM_CALIBRATION_SIZE - 4,
                     NVRAM_CALIBRATION_ADDR + CAL_CRC_OFFSET)) {
        DEBUG_PRINTLN("Calibration: CRC mismatch");
        _calibration.isCalibrated = false;
        return false;
    }
    
    // Parse calibration data
    _calibration.moistureDry = *(uint16_t*)&buffer[CAL_MOISTURE_DRY_OFFSET];
    _calibration.moistureWet = *(uint16_t*)&buffer[CAL_MOISTURE_WET_OFFSET];
    _calibration.moistureTempCoef = *(int16_t*)&buffer[CAL_MOISTURE_TEMP_COEF];
    _calibration.batteryOffset = *(int16_t*)&buffer[CAL_BATTERY_OFFSET_OFFSET];
    _calibration.batteryScale = *(uint16_t*)&buffer[CAL_BATTERY_SCALE_OFFSET];
    _calibration.tempOffset = *(int16_t*)&buffer[CAL_TEMP_OFFSET_OFFSET];
    _calibration.loraFreqOffset = *(int32_t*)&buffer[CAL_LORA_FREQ_OFFSET];
    _calibration.isCalibrated = true;
    
    return true;
}

bool ConfigManager::setCalibration(const FactoryCalibration& cal) {
    _calibration = cal;
    _calibration.isCalibrated = true;
    return saveCalibrationInternal();
}

bool ConfigManager::saveCalibrationInternal() {
    uint8_t buffer[NVRAM_CALIBRATION_SIZE];
    memset(buffer, 0, sizeof(buffer));
    
    // Write magic and version
    *(uint32_t*)&buffer[CAL_MAGIC_OFFSET] = NVRAM_MAGIC_CALIBRATION;
    buffer[CAL_VERSION_OFFSET] = NVRAM_CALIBRATION_VERSION;
    
    // Write calibration data
    *(uint16_t*)&buffer[CAL_MOISTURE_DRY_OFFSET] = _calibration.moistureDry;
    *(uint16_t*)&buffer[CAL_MOISTURE_WET_OFFSET] = _calibration.moistureWet;
    *(int16_t*)&buffer[CAL_MOISTURE_TEMP_COEF] = _calibration.moistureTempCoef;
    *(int16_t*)&buffer[CAL_BATTERY_OFFSET_OFFSET] = _calibration.batteryOffset;
    *(uint16_t*)&buffer[CAL_BATTERY_SCALE_OFFSET] = _calibration.batteryScale;
    *(int16_t*)&buffer[CAL_TEMP_OFFSET_OFFSET] = _calibration.tempOffset;
    *(int32_t*)&buffer[CAL_LORA_FREQ_OFFSET] = _calibration.loraFreqOffset;
    
    // Calculate and write CRC
    uint32_t crc = calculateCrc32(buffer, NVRAM_CALIBRATION_SIZE - 4);
    *(uint32_t*)&buffer[CAL_CRC_OFFSET] = crc;
    
    return nvramWrite(NVRAM_CALIBRATION_ADDR, buffer, NVRAM_CALIBRATION_SIZE);
}

uint8_t ConfigManager::applyMoistureCalibration(uint16_t rawValue, int16_t temperature) const {
    if (!_calibration.isCalibrated) {
        // No calibration - return raw value scaled to 0-100
        return (uint8_t)constrain(map(rawValue, 0, 4095, 0, 100), 0, 100);
    }
    
    // Apply temperature compensation
    int32_t tempCompensation = 0;
    if (_calibration.moistureTempCoef != 0) {
        // Temperature is in 0.1°C, coefficient is in 0.01 units
        // Reference temperature is 25°C (250 in 0.1°C)
        int32_t tempDelta = temperature - 250;
        tempCompensation = (tempDelta * _calibration.moistureTempCoef) / 1000;
    }
    
    int32_t compensatedValue = rawValue + tempCompensation;
    
    // Map to percentage using calibration values
    int32_t range = _calibration.moistureWet - _calibration.moistureDry;
    if (range == 0) range = 1;  // Avoid division by zero
    
    int32_t percent = ((compensatedValue - _calibration.moistureDry) * 100) / range;
    return (uint8_t)constrain(percent, 0, 100);
}

uint16_t ConfigManager::applyBatteryCalibration(uint16_t rawMv) const {
    if (!_calibration.isCalibrated) {
        return rawMv;
    }
    
    // Apply offset and scale
    // Scale is in 0.001 units (1000 = 1.0)
    int32_t calibrated = rawMv + _calibration.batteryOffset;
    calibrated = (calibrated * _calibration.batteryScale) / 1000;
    
    return (uint16_t)constrain(calibrated, 0, 5000);
}

// ============================================================================
// User Configuration Management
// ============================================================================

bool ConfigManager::loadConfig() {
    uint8_t buffer[NVRAM_USER_CONFIG_SIZE];
    
    if (!nvramRead(NVRAM_USER_CONFIG_ADDR, buffer, NVRAM_USER_CONFIG_SIZE)) {
        return false;
    }
    
    // Check magic
    uint32_t magic = *(uint32_t*)&buffer[CFG_MAGIC_OFFSET];
    if (magic != NVRAM_MAGIC_USER_CONFIG) {
        DEBUG_PRINTLN("Config: invalid magic");
        return false;
    }
    
    // Verify CRC
    if (!verifyCrc32(NVRAM_USER_CONFIG_ADDR, NVRAM_USER_CONFIG_SIZE - 4,
                     NVRAM_USER_CONFIG_ADDR + CFG_CRC_OFFSET)) {
        DEBUG_PRINTLN("Config: CRC mismatch");
        return false;
    }
    
    // Parse config
    _config.sleepIntervalSec = *(uint32_t*)&buffer[CFG_SLEEP_INTERVAL_OFFSET];
    _config.reportIntervalSec = *(uint32_t*)&buffer[CFG_REPORT_INTERVAL_OFFSET];
    _config.lowBatteryThreshMv = *(uint16_t*)&buffer[CFG_LOW_BATT_THRESH_OFFSET];
    _config.critBatteryThreshMv = *(uint16_t*)&buffer[CFG_CRIT_BATT_THRESH_OFFSET];
    _config.moistureLowAlarm = buffer[CFG_MOISTURE_LOW_OFFSET];
    _config.moistureHighAlarm = buffer[CFG_MOISTURE_HIGH_OFFSET];
    _config.loraTxPower = buffer[CFG_LORA_TX_POWER_OFFSET];
    _config.loraSpreadingFactor = buffer[CFG_LORA_SF_OFFSET];
    _config.gatewayId = *(uint32_t*)&buffer[CFG_GATEWAY_ID_OFFSET];
    memcpy(_config.networkKey, &buffer[CFG_NETWORK_KEY_OFFSET], 16);
    
    uint8_t flags = buffer[CFG_FLAGS_OFFSET];
    _config.isPaired = (flags & CFG_FLAG_PAIRED) != 0;
    _config.alarmsEnabled = (flags & CFG_FLAG_ALARMS_ENABLED) != 0;
    
    _configDirty = false;
    return true;
}

void ConfigManager::initDefaultConfig() {
    _config.sleepIntervalSec = SLEEP_INTERVAL_HOURS * 3600;
    _config.reportIntervalSec = SLEEP_INTERVAL_HOURS * 3600;
    _config.lowBatteryThreshMv = BATTERY_LOW_THRESHOLD_MV;
    _config.critBatteryThreshMv = BATTERY_CRITICAL_MV;
    _config.moistureLowAlarm = 20;
    _config.moistureHighAlarm = 80;
    _config.loraTxPower = LORA_TX_POWER_DBM;
    _config.loraSpreadingFactor = LORA_SPREADING_FACTOR;
    _config.gatewayId = 0;
    memset(_config.networkKey, 0, 16);
    _config.isPaired = false;
    _config.alarmsEnabled = false;
}

bool ConfigManager::setConfig(const UserConfig& config) {
    _config = config;
    return saveConfigInternal();
}

bool ConfigManager::saveConfig() {
    if (!_configDirty) return true;
    return saveConfigInternal();
}

bool ConfigManager::saveConfigInternal() {
    uint8_t buffer[NVRAM_USER_CONFIG_SIZE];
    memset(buffer, 0, sizeof(buffer));
    
    // Write magic and version
    *(uint32_t*)&buffer[CFG_MAGIC_OFFSET] = NVRAM_MAGIC_USER_CONFIG;
    buffer[CFG_VERSION_OFFSET] = NVRAM_USER_CONFIG_VERSION;
    
    // Write flags
    uint8_t flags = 0;
    if (_config.isPaired) flags |= CFG_FLAG_PAIRED;
    if (_config.alarmsEnabled) flags |= CFG_FLAG_ALARMS_ENABLED;
    buffer[CFG_FLAGS_OFFSET] = flags;
    
    // Write config data
    *(uint32_t*)&buffer[CFG_SLEEP_INTERVAL_OFFSET] = _config.sleepIntervalSec;
    *(uint32_t*)&buffer[CFG_REPORT_INTERVAL_OFFSET] = _config.reportIntervalSec;
    *(uint16_t*)&buffer[CFG_LOW_BATT_THRESH_OFFSET] = _config.lowBatteryThreshMv;
    *(uint16_t*)&buffer[CFG_CRIT_BATT_THRESH_OFFSET] = _config.critBatteryThreshMv;
    buffer[CFG_MOISTURE_LOW_OFFSET] = _config.moistureLowAlarm;
    buffer[CFG_MOISTURE_HIGH_OFFSET] = _config.moistureHighAlarm;
    buffer[CFG_LORA_TX_POWER_OFFSET] = _config.loraTxPower;
    buffer[CFG_LORA_SF_OFFSET] = _config.loraSpreadingFactor;
    *(uint32_t*)&buffer[CFG_GATEWAY_ID_OFFSET] = _config.gatewayId;
    memcpy(&buffer[CFG_NETWORK_KEY_OFFSET], _config.networkKey, 16);
    
    // Calculate and write CRC
    uint32_t crc = calculateCrc32(buffer, NVRAM_USER_CONFIG_SIZE - 4);
    *(uint32_t*)&buffer[CFG_CRC_OFFSET] = crc;
    
    bool ok = nvramWrite(NVRAM_USER_CONFIG_ADDR, buffer, NVRAM_USER_CONFIG_SIZE);
    if (ok) _configDirty = false;
    return ok;
}

bool ConfigManager::resetConfigToDefaults() {
    initDefaultConfig();
    return saveConfigInternal();
}

// ============================================================================
// Runtime State Management
// ============================================================================

bool ConfigManager::loadState() {
    uint8_t buffer[NVRAM_RUNTIME_STATE_SIZE];
    
    if (!nvramRead(NVRAM_RUNTIME_STATE_ADDR, buffer, NVRAM_RUNTIME_STATE_SIZE)) {
        return false;
    }
    
    // Check magic
    uint32_t magic = *(uint32_t*)&buffer[STATE_MAGIC_OFFSET];
    if (magic != NVRAM_MAGIC_RUNTIME) {
        return false;
    }
    
    // Parse state
    _state.bootCount = *(uint32_t*)&buffer[STATE_BOOT_COUNT_OFFSET];
    _state.lastBootTime = *(uint32_t*)&buffer[STATE_LAST_BOOT_OFFSET];
    _state.lastReportTime = *(uint32_t*)&buffer[STATE_LAST_REPORT_OFFSET];
    _state.lastAckedSequence = *(uint16_t*)&buffer[STATE_LAST_ACK_SEQ_OFFSET];
    _state.pendingLogCount = *(uint16_t*)&buffer[STATE_PENDING_LOGS_OFFSET];
    _state.currentFwVersion = *(uint32_t*)&buffer[STATE_FW_VERSION_OFFSET];
    _state.previousFwVersion = *(uint32_t*)&buffer[STATE_PREV_FW_VERSION];
    _state.otaStatus = buffer[STATE_OTA_STATUS_OFFSET];
    _state.otaProgress = buffer[STATE_OTA_PROGRESS_OFFSET];
    
    _stateDirty = false;
    return true;
}

void ConfigManager::initDefaultState() {
    memset(&_state, 0, sizeof(_state));
}

bool ConfigManager::saveState() {
    if (!_stateDirty) return true;
    return saveStateInternal();
}

bool ConfigManager::saveStateInternal() {
    uint8_t buffer[NVRAM_RUNTIME_STATE_SIZE];
    memset(buffer, 0, sizeof(buffer));
    
    // Write magic and version
    *(uint32_t*)&buffer[STATE_MAGIC_OFFSET] = NVRAM_MAGIC_RUNTIME;
    buffer[STATE_VERSION_OFFSET] = NVRAM_RUNTIME_VERSION;
    
    // Write state data
    *(uint32_t*)&buffer[STATE_BOOT_COUNT_OFFSET] = _state.bootCount;
    *(uint32_t*)&buffer[STATE_LAST_BOOT_OFFSET] = _state.lastBootTime;
    *(uint32_t*)&buffer[STATE_LAST_REPORT_OFFSET] = _state.lastReportTime;
    *(uint16_t*)&buffer[STATE_LAST_ACK_SEQ_OFFSET] = _state.lastAckedSequence;
    *(uint16_t*)&buffer[STATE_PENDING_LOGS_OFFSET] = _state.pendingLogCount;
    *(uint32_t*)&buffer[STATE_FW_VERSION_OFFSET] = _state.currentFwVersion;
    *(uint32_t*)&buffer[STATE_PREV_FW_VERSION] = _state.previousFwVersion;
    buffer[STATE_OTA_STATUS_OFFSET] = _state.otaStatus;
    buffer[STATE_OTA_PROGRESS_OFFSET] = _state.otaProgress;
    
    bool ok = nvramWrite(NVRAM_RUNTIME_STATE_ADDR, buffer, NVRAM_RUNTIME_STATE_SIZE);
    if (ok) _stateDirty = false;
    return ok;
}

void ConfigManager::recordBoot(uint32_t timestamp) {
    _state.bootCount++;
    _state.lastBootTime = timestamp;
    _stateDirty = true;
}

void ConfigManager::recordReport(uint32_t timestamp, uint16_t sequence) {
    _state.lastReportTime = timestamp;
    _state.lastAckedSequence = sequence;
    _stateDirty = true;
}

// ============================================================================
// Statistics Management
// ============================================================================

bool ConfigManager::loadStats() {
    uint8_t buffer[NVRAM_STATS_SIZE];
    
    if (!nvramRead(NVRAM_STATS_ADDR, buffer, NVRAM_STATS_SIZE)) {
        return false;
    }
    
    // Check magic
    uint32_t magic = *(uint32_t*)&buffer[STATS_MAGIC_OFFSET];
    if (magic != NVRAM_MAGIC_STATS) {
        return false;
    }
    
    // Parse stats
    _stats.txSuccess = *(uint32_t*)&buffer[STATS_TX_SUCCESS_OFFSET];
    _stats.txFail = *(uint32_t*)&buffer[STATS_TX_FAIL_OFFSET];
    _stats.rxSuccess = *(uint32_t*)&buffer[STATS_RX_SUCCESS_OFFSET];
    _stats.rxFail = *(uint32_t*)&buffer[STATS_RX_FAIL_OFFSET];
    _stats.otaSuccess = *(uint16_t*)&buffer[STATS_OTA_SUCCESS_OFFSET];
    _stats.otaFail = *(uint16_t*)&buffer[STATS_OTA_FAIL_OFFSET];
    _stats.unexpectedResets = *(uint16_t*)&buffer[STATS_RESET_COUNT_OFFSET];
    _stats.lowBatteryEvents = *(uint16_t*)&buffer[STATS_LOW_BATT_COUNT];
    _stats.minBatteryMv = *(uint16_t*)&buffer[STATS_MIN_BATT_MV_OFFSET];
    _stats.maxTemperature = *(int16_t*)&buffer[STATS_MAX_TEMP_OFFSET];
    _stats.minTemperature = *(int16_t*)&buffer[STATS_MIN_TEMP_OFFSET];
    _stats.uptimeHours = *(uint32_t*)&buffer[STATS_UPTIME_HOURS_OFFSET];
    
    _statsDirty = false;
    return true;
}

void ConfigManager::initDefaultStats() {
    memset(&_stats, 0, sizeof(_stats));
    _stats.minBatteryMv = 5000;  // Start high so first reading updates it
    _stats.maxTemperature = -400; // -40°C in 0.1°C
    _stats.minTemperature = 850;  // 85°C in 0.1°C
}

bool ConfigManager::saveStats() {
    if (!_statsDirty) return true;
    return saveStatsInternal();
}

bool ConfigManager::saveStatsInternal() {
    uint8_t buffer[NVRAM_STATS_SIZE];
    memset(buffer, 0, sizeof(buffer));
    
    // Write magic and version
    *(uint32_t*)&buffer[STATS_MAGIC_OFFSET] = NVRAM_MAGIC_STATS;
    buffer[STATS_VERSION_OFFSET] = NVRAM_STATS_VERSION;
    
    // Write stats data
    *(uint32_t*)&buffer[STATS_TX_SUCCESS_OFFSET] = _stats.txSuccess;
    *(uint32_t*)&buffer[STATS_TX_FAIL_OFFSET] = _stats.txFail;
    *(uint32_t*)&buffer[STATS_RX_SUCCESS_OFFSET] = _stats.rxSuccess;
    *(uint32_t*)&buffer[STATS_RX_FAIL_OFFSET] = _stats.rxFail;
    *(uint16_t*)&buffer[STATS_OTA_SUCCESS_OFFSET] = _stats.otaSuccess;
    *(uint16_t*)&buffer[STATS_OTA_FAIL_OFFSET] = _stats.otaFail;
    *(uint16_t*)&buffer[STATS_RESET_COUNT_OFFSET] = _stats.unexpectedResets;
    *(uint16_t*)&buffer[STATS_LOW_BATT_COUNT] = _stats.lowBatteryEvents;
    *(uint16_t*)&buffer[STATS_MIN_BATT_MV_OFFSET] = _stats.minBatteryMv;
    *(int16_t*)&buffer[STATS_MAX_TEMP_OFFSET] = _stats.maxTemperature;
    *(int16_t*)&buffer[STATS_MIN_TEMP_OFFSET] = _stats.minTemperature;
    *(uint32_t*)&buffer[STATS_UPTIME_HOURS_OFFSET] = _stats.uptimeHours;
    
    bool ok = nvramWrite(NVRAM_STATS_ADDR, buffer, NVRAM_STATS_SIZE);
    if (ok) _statsDirty = false;
    return ok;
}

void ConfigManager::recordTx(bool success) {
    if (success) {
        _stats.txSuccess++;
    } else {
        _stats.txFail++;
    }
    _statsDirty = true;
}

void ConfigManager::recordRx(bool success) {
    if (success) {
        _stats.rxSuccess++;
    } else {
        _stats.rxFail++;
    }
    _statsDirty = true;
}

void ConfigManager::recordOta(bool success) {
    if (success) {
        _stats.otaSuccess++;
    } else {
        _stats.otaFail++;
    }
    _statsDirty = true;
}

void ConfigManager::updateEnvironmentStats(uint16_t batteryMv, int16_t temperature) {
    bool changed = false;
    
    if (batteryMv < _stats.minBatteryMv) {
        _stats.minBatteryMv = batteryMv;
        changed = true;
    }
    
    if (temperature > _stats.maxTemperature) {
        _stats.maxTemperature = temperature;
        changed = true;
    }
    
    if (temperature < _stats.minTemperature) {
        _stats.minTemperature = temperature;
        changed = true;
    }
    
    if (changed) {
        _statsDirty = true;
    }
}

// ============================================================================
// Firmware Version Management
// ============================================================================

uint32_t ConfigManager::packVersion(uint8_t major, uint8_t minor, uint8_t patch) {
    return ((uint32_t)major << 16) | ((uint32_t)minor << 8) | patch;
}

bool ConfigManager::checkVersionChange(uint8_t major, uint8_t minor, uint8_t patch) {
    uint32_t currentVersion = packVersion(major, minor, patch);
    
    if (_state.currentFwVersion != currentVersion) {
        _state.previousFwVersion = _state.currentFwVersion;
        _state.currentFwVersion = currentVersion;
        _stateDirty = true;
        return true;
    }
    
    return false;
}

void ConfigManager::migrateData(uint32_t fromVersion, uint32_t toVersion) {
    // Add migration logic here as needed
    DEBUG_PRINTF("Migrating data from v%d.%d.%d to v%d.%d.%d\n",
                 (fromVersion >> 16) & 0xFF, (fromVersion >> 8) & 0xFF, fromVersion & 0xFF,
                 (toVersion >> 16) & 0xFF, (toVersion >> 8) & 0xFF, toVersion & 0xFF);
}

// ============================================================================
// Factory Reset
// ============================================================================

bool ConfigManager::factoryReset() {
    DEBUG_PRINTLN("Performing factory reset...");
    
    // Clear user config (but keep calibration)
    initDefaultConfig();
    if (!saveConfigInternal()) return false;
    
    // Clear runtime state
    initDefaultState();
    if (!saveStateInternal()) return false;
    
    // Clear statistics
    initDefaultStats();
    if (!saveStatsInternal()) return false;
    
    DEBUG_PRINTLN("Factory reset complete");
    return true;
}

bool ConfigManager::fullErase() {
    DEBUG_PRINTLN("Performing full erase...");
    
    // Erase all FRAM regions
    uint8_t zeros[256];
    memset(zeros, 0, sizeof(zeros));
    
    // Erase protected region
    for (uint32_t addr = NVRAM_PROTECTED_START; addr < NVRAM_PROTECTED_START + NVRAM_PROTECTED_SIZE; addr += sizeof(zeros)) {
        size_t len = min((size_t)(NVRAM_PROTECTED_START + NVRAM_PROTECTED_SIZE - addr), sizeof(zeros));
        nvramWrite(addr, zeros, len);
    }
    
    // Erase managed region
    for (uint32_t addr = NVRAM_MANAGED_START; addr < NVRAM_MANAGED_START + NVRAM_MANAGED_SIZE; addr += sizeof(zeros)) {
        size_t len = min((size_t)(NVRAM_MANAGED_START + NVRAM_MANAGED_SIZE - addr), sizeof(zeros));
        nvramWrite(addr, zeros, len);
    }
    
    // Re-initialize defaults
    initDefaultConfig();
    initDefaultState();
    initDefaultStats();
    
    DEBUG_PRINTLN("Full erase complete");
    return true;
}

// ============================================================================
// CRC Utilities
// ============================================================================

uint32_t ConfigManager::calculateCrc32(const uint8_t* data, int len) {
    uint32_t crc = 0xFFFFFFFF;
    
    for (int i = 0; i < len; i++) {
        crc = crc32Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

bool ConfigManager::verifyCrc32(uint32_t addr, int dataLen, uint32_t crcAddr) {
    // Read data and calculate CRC
    uint8_t buffer[256];
    uint32_t crc = 0xFFFFFFFF;
    
    int remaining = dataLen;
    uint32_t currentAddr = addr;
    
    while (remaining > 0) {
        int chunkSize = min(remaining, (int)sizeof(buffer));
        if (!nvramRead(currentAddr, buffer, chunkSize)) {
            return false;
        }
        
        for (int i = 0; i < chunkSize; i++) {
            crc = crc32Table[(crc ^ buffer[i]) & 0xFF] ^ (crc >> 8);
        }
        
        remaining -= chunkSize;
        currentAddr += chunkSize;
    }
    
    crc ^= 0xFFFFFFFF;
    
    // Read stored CRC
    uint32_t storedCrc;
    if (!nvramRead(crcAddr, (uint8_t*)&storedCrc, 4)) {
        return false;
    }
    
    return crc == storedCrc;
}

void ConfigManager::writeCrc32(uint32_t addr, int dataLen, uint32_t crcAddr) {
    // Read data and calculate CRC
    uint8_t buffer[256];
    uint32_t crc = 0xFFFFFFFF;
    
    int remaining = dataLen;
    uint32_t currentAddr = addr;
    
    while (remaining > 0) {
        int chunkSize = min(remaining, (int)sizeof(buffer));
        nvramRead(currentAddr, buffer, chunkSize);
        
        for (int i = 0; i < chunkSize; i++) {
            crc = crc32Table[(crc ^ buffer[i]) & 0xFF] ^ (crc >> 8);
        }
        
        remaining -= chunkSize;
        currentAddr += chunkSize;
    }
    
    crc ^= 0xFFFFFFFF;
    
    // Write CRC
    nvramWrite(crcAddr, (uint8_t*)&crc, 4);
}
