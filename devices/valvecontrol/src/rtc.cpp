/**
 * @file rtc.cpp
 * @brief RV-3028 RTC module implementation
 * 
 * The RV-3028 has a built-in Unix time counter which simplifies
 * timestamp handling significantly.
 */

#include "rtc.h"

// RV-3028 Register addresses
#define RV3028_SECONDS          0x00
#define RV3028_MINUTES          0x01
#define RV3028_HOURS            0x02
#define RV3028_WEEKDAY          0x03
#define RV3028_DATE             0x04
#define RV3028_MONTH            0x05
#define RV3028_YEAR             0x06
#define RV3028_UNIX_TIME0       0x1B  // Unix time LSB
#define RV3028_UNIX_TIME1       0x1C
#define RV3028_UNIX_TIME2       0x1D
#define RV3028_UNIX_TIME3       0x1E  // Unix time MSB
#define RV3028_STATUS           0x0E
#define RV3028_CONTROL1         0x0F
#define RV3028_CONTROL2         0x10

// Status register bits
#define STATUS_PORF             0x01  // Power-on reset flag
#define STATUS_BSF              0x04  // Backup switchover flag

// Control2 register bits
#define CTRL2_TSE               0x80  // Timestamp enable

static uint8_t readRegister(uint8_t reg) {
    Wire.beginTransmission(RTC_I2C_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(RTC_I2C_ADDR, (uint8_t)1);
    return Wire.read();
}

static void writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(RTC_I2C_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

static uint8_t bcdToDec(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static uint8_t decToBcd(uint8_t dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

bool rtc_init(void) {
    DEBUG_PRINTLN("RTC: Initializing RV-3028...");
    
    Wire.begin();
    Wire.setClock(400000);  // 400kHz I2C
    
    // Check if RTC is responding
    Wire.beginTransmission(RTC_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        DEBUG_PRINTLN("RTC: Device not found!");
        return false;
    }
    
    // Check status register
    uint8_t status = readRegister(RV3028_STATUS);
    
    if (status & STATUS_PORF) {
        DEBUG_PRINTLN("RTC: Power-on reset detected, time may be invalid");
        // Clear the flag
        writeRegister(RV3028_STATUS, status & ~STATUS_PORF);
    }
    
    if (status & STATUS_BSF) {
        DEBUG_PRINTLN("RTC: Backup switchover occurred");
        writeRegister(RV3028_STATUS, status & ~STATUS_BSF);
    }
    
    DEBUG_PRINTLN("RTC: Initialized");
    return true;
}

uint32_t rtc_get_unix_time(void) {
    uint32_t unix_time = 0;
    
    Wire.beginTransmission(RTC_I2C_ADDR);
    Wire.write(RV3028_UNIX_TIME0);
    Wire.endTransmission(false);
    Wire.requestFrom(RTC_I2C_ADDR, (uint8_t)4);
    
    unix_time = Wire.read();
    unix_time |= ((uint32_t)Wire.read() << 8);
    unix_time |= ((uint32_t)Wire.read() << 16);
    unix_time |= ((uint32_t)Wire.read() << 24);
    
    return unix_time;
}

bool rtc_set_unix_time(uint32_t timestamp) {
    Wire.beginTransmission(RTC_I2C_ADDR);
    Wire.write(RV3028_UNIX_TIME0);
    Wire.write(timestamp & 0xFF);
    Wire.write((timestamp >> 8) & 0xFF);
    Wire.write((timestamp >> 16) & 0xFF);
    Wire.write((timestamp >> 24) & 0xFF);
    
    if (Wire.endTransmission() != 0) {
        DEBUG_PRINTLN("RTC: Failed to set Unix time");
        return false;
    }
    
    DEBUG_PRINTF("RTC: Set Unix time to %lu\n", timestamp);
    return true;
}

uint8_t rtc_get_hour(void) {
    uint8_t hours = readRegister(RV3028_HOURS);
    return bcdToDec(hours & 0x3F);  // Mask out 24h bit
}

uint8_t rtc_get_minute(void) {
    uint8_t minutes = readRegister(RV3028_MINUTES);
    return bcdToDec(minutes & 0x7F);
}

uint8_t rtc_get_day_of_week(void) {
    uint8_t weekday = readRegister(RV3028_WEEKDAY);
    return weekday & 0x07;  // 0=Sunday, 6=Saturday
}

uint16_t rtc_get_minutes_from_midnight(void) {
    uint8_t hours = rtc_get_hour();
    uint8_t minutes = rtc_get_minute();
    return (hours * 60) + minutes;
}

bool rtc_is_battery_low(void) {
    // RV-3028 doesn't have a direct battery low indicator
    // Check if backup switchover occurred recently
    uint8_t status = readRegister(RV3028_STATUS);
    return (status & STATUS_BSF) != 0;
}
