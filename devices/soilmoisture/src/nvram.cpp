/**
 * @file nvram.cpp
 * @brief NVRAM (SPI FRAM) Driver Implementation
 * 
 * Uses Adafruit FRAM SPI library for FM25V02 access.
 */

#include "nvram.h"

// Config structure stored in NVRAM
struct NVRAMConfig {
    uint16_t logHead;
    uint16_t logTail;
    uint16_t logCount;
    uint16_t pendingCount;
    uint32_t bootCount;
    uint8_t  reserved[22];
} __attribute__((packed));

NVRAM::NVRAM(uint8_t csPin) : _csPin(csPin), _initialized(false), _fram(nullptr),
    _logHead(0), _logTail(0), _logCount(0), _pendingCount(0) {
}

bool NVRAM::begin() {
    // Create FRAM instance with CS pin and SPI clock speed
    _fram = new Adafruit_FRAM_SPI(_csPin, &SPI, SPI_CLOCK_NVRAM_HZ);
    
    if (_fram == nullptr) {
        DEBUG_PRINTLN("NVRAM: Failed to allocate FRAM object");
        return false;
    }
    
    // Initialize FRAM with 2-byte addressing (FM25V02 = 256Kbit = 32KB, uses 2-byte addr)
    if (!_fram->begin(2)) {
        DEBUG_PRINTLN("NVRAM: No device detected");
        delete _fram;
        _fram = nullptr;
        return false;
    }
    
    // Read device ID for verification
    uint8_t mfgId;
    uint16_t prodId;
    if (_fram->getDeviceID(&mfgId, &prodId)) {
        DEBUG_PRINTF("NVRAM: Mfg ID = 0x%02X, Product ID = 0x%04X\n", mfgId, prodId);
    }
    
    // Load configuration
    loadConfig();
    
    _initialized = true;
    return true;
}

void NVRAM::loadConfig() {
    NVRAMConfig config;
    read(NVRAM_CONFIG_ADDR, (uint8_t*)&config, sizeof(config));
    
    // Validate config
    if (config.logHead >= NVRAM_LOG_MAX_ENTRIES ||
        config.logTail >= NVRAM_LOG_MAX_ENTRIES) {
        // Initialize fresh config
        _logHead = 0;
        _logTail = 0;
        _logCount = 0;
        _pendingCount = 0;
        saveConfig();
        DEBUG_PRINTLN("NVRAM: Initialized fresh config");
    } else {
        _logHead = config.logHead;
        _logTail = config.logTail;
        _logCount = config.logCount;
        _pendingCount = config.pendingCount;
        DEBUG_PRINT("NVRAM: Loaded config, log count = ");
        DEBUG_PRINTLN(_logCount);
    }
}

void NVRAM::saveConfig() {
    NVRAMConfig config;
    config.logHead = _logHead;
    config.logTail = _logTail;
    config.logCount = _logCount;
    config.pendingCount = _pendingCount;
    config.bootCount = 0;
    memset(config.reserved, 0, sizeof(config.reserved));
    
    write(NVRAM_CONFIG_ADDR, (const uint8_t*)&config, sizeof(config));
}

// UUID functions removed - device identity now comes from FICR
// See security.h for security_getDeviceId()

bool NVRAM::read(uint16_t addr, uint8_t* data, uint16_t len) {
    if (!_initialized || _fram == nullptr) {
        return false;
    }
    if (addr + len > NVRAM_SIZE_BYTES) {
        return false;
    }
    
    // Use Adafruit FRAM library bulk read
    return _fram->read(addr, data, len);
}

bool NVRAM::write(uint16_t addr, const uint8_t* data, uint16_t len) {
    if (!_initialized || _fram == nullptr) {
        return false;
    }
    if (addr + len > NVRAM_SIZE_BYTES) {
        return false;
    }
    
    // Use Adafruit FRAM library bulk write
    return _fram->write(addr, data, len);
}

bool NVRAM::logAppend(const LogEntry* entry) {
    if (!_initialized) {
        return false;
    }
    
    // Calculate address for new entry
    uint16_t addr = NVRAM_LOG_START_ADDR + (_logHead * NVRAM_LOG_ENTRY_SIZE);
    
    // Write entry
    if (!write(addr, (const uint8_t*)entry, sizeof(LogEntry))) {
        return false;
    }
    
    // Update head pointer (circular)
    _logHead = (_logHead + 1) % NVRAM_LOG_MAX_ENTRIES;
    
    // Update counts
    if (_logCount < NVRAM_LOG_MAX_ENTRIES) {
        _logCount++;
    } else {
        // Buffer full, advance tail (overwrite oldest)
        _logTail = (_logTail + 1) % NVRAM_LOG_MAX_ENTRIES;
    }
    
    _pendingCount++;
    if (_pendingCount > _logCount) {
        _pendingCount = _logCount;
    }
    
    saveConfig();
    return true;
}

uint16_t NVRAM::logPendingCount() {
    return _pendingCount;
}

bool NVRAM::logReadPending(uint16_t index, LogEntry* entry) {
    if (index >= _pendingCount) {
        return false;
    }
    
    // Calculate position of pending entry
    uint16_t pos = (_logHead + NVRAM_LOG_MAX_ENTRIES - _pendingCount + index) % NVRAM_LOG_MAX_ENTRIES;
    uint16_t addr = NVRAM_LOG_START_ADDR + (pos * NVRAM_LOG_ENTRY_SIZE);
    
    return read(addr, (uint8_t*)entry, sizeof(LogEntry));
}

bool NVRAM::logMarkTransmitted(uint16_t count) {
    if (count > _pendingCount) {
        count = _pendingCount;
    }
    
    _pendingCount -= count;
    saveConfig();
    
    return true;
}

uint16_t NVRAM::logCount() {
    return _logCount;
}

bool NVRAM::logClear() {
    _logHead = 0;
    _logTail = 0;
    _logCount = 0;
    _pendingCount = 0;
    
    saveConfig();
    return true;
}

void NVRAM::sleep() {
    if (_fram != nullptr) {
        _fram->enterSleep();
    }
}

void NVRAM::wake() {
    if (_fram != nullptr) {
        _fram->exitSleep();
    }
}
