/**
 * @file nvram.h
 * @brief NVRAM (SPI FRAM) Driver Interface
 * 
 * Provides persistent storage for configuration and data logging.
 * Uses Adafruit FRAM SPI library for FM25V02 access.
 * Device identity comes from FICR (see security.h).
 */

#ifndef NVRAM_H
#define NVRAM_H

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_FRAM_SPI.h>
#include "config.h"
#include "protocol.h"

class NVRAM {
public:
    NVRAM(uint8_t csPin);
    
    // Initialize NVRAM
    bool begin();
    
    // Raw read/write
    bool read(uint16_t addr, uint8_t* data, uint16_t len);
    bool write(uint16_t addr, const uint8_t* data, uint16_t len);
    
    // Log management
    bool logAppend(const LogEntry* entry);
    uint16_t logPendingCount();
    bool logReadPending(uint16_t index, LogEntry* entry);
    bool logMarkTransmitted(uint16_t count);
    uint16_t logCount();
    bool logClear();
    
    // Power management
    void sleep();
    void wake();

private:
    uint8_t _csPin;
    bool _initialized;
    Adafruit_FRAM_SPI* _fram;
    
    // Log management state
    uint16_t _logHead;
    uint16_t _logTail;
    uint16_t _logCount;
    uint16_t _pendingCount;
    
    void loadConfig();
    void saveConfig();
};

#endif // NVRAM_H
