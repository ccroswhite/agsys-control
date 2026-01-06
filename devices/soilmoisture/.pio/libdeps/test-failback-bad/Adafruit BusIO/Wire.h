/**
 * @file Wire.h
 * @brief Stub Wire/I2C header to satisfy Adafruit BusIO compilation
 * 
 * This project only uses SPI devices (FRAM, Flash, LoRa).
 * The Adafruit BusIO library includes I2C support that requires Wire.h,
 * but we don't use any I2C functionality.
 * 
 * This stub provides empty definitions so the library compiles.
 * Any actual I2C calls would fail at link time (which is intentional).
 */

#ifndef WIRE_STUB_H
#define WIRE_STUB_H

#include <Arduino.h>

// Minimal TwoWire stub class
class TwoWire {
public:
    TwoWire() {}
    void begin() {}
    void begin(uint8_t addr) {}
    void end() {}
    void setClock(uint32_t freq) {}
    
    void beginTransmission(uint8_t addr) {}
    uint8_t endTransmission(bool stop = true) { return 0; }
    
    size_t write(uint8_t data) { return 1; }
    size_t write(const uint8_t* data, size_t len) { return len; }
    
    uint8_t requestFrom(uint8_t addr, uint8_t qty, bool stop = true) { return 0; }
    int available() { return 0; }
    int read() { return -1; }
    int peek() { return -1; }
    void flush() {}
};

// Global Wire instance (never actually used)
extern TwoWire Wire;

#endif // WIRE_STUB_H
