/**
 * @file security.h
 * @brief Security and code protection for production builds
 * 
 * This module handles:
 * - APPROTECT (Access Port Protection) for code readout protection
 * - Device ID access from FICR
 * 
 * APPROTECT prevents external debuggers from reading flash/RAM.
 * Once enabled, only a full chip erase can disable it.
 * OTA updates still work because the CPU can write its own flash.
 */

#ifndef SECURITY_H
#define SECURITY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the 64-bit device ID from FICR
 * 
 * This is a factory-programmed, globally unique identifier.
 * It cannot be modified and survives chip erase.
 * 
 * @return 64-bit device ID
 */
uint64_t security_getDeviceId(void);

/**
 * @brief Get device ID as two 32-bit words
 * 
 * @param low  Pointer to store lower 32 bits
 * @param high Pointer to store upper 32 bits
 */
void security_getDeviceIdWords(uint32_t* low, uint32_t* high);

/**
 * @brief Copy device ID to a byte array (8 bytes, big-endian)
 * 
 * @param buffer 8-byte buffer to store device ID
 */
void security_getDeviceIdBytes(uint8_t* buffer);

/**
 * @brief Check if APPROTECT is currently enabled
 * 
 * @return true if debug port is protected
 */
bool security_isApprotectEnabled(void);

/**
 * @brief Enable APPROTECT (code readout protection)
 * 
 * WARNING: This is a one-way operation!
 * Once enabled, the only way to disable is a full chip erase,
 * which wipes all flash and UICR.
 * 
 * This function:
 * 1. Writes 0x00 to UICR.APPROTECT
 * 2. Triggers a system reset to apply the change
 * 
 * After reset, external debuggers cannot:
 * - Read flash (your code)
 * - Read RAM
 * - Read FICR/UICR
 * - Single-step debug
 * 
 * OTA updates still work because the CPU writes its own flash.
 * 
 * @note Only call this in production builds!
 * @note This function does not return - it resets the device.
 */
void security_enableApprotect(void);

/**
 * @brief Initialize security module
 * 
 * In release builds (RELEASE_BUILD defined), this will:
 * - Check if APPROTECT is already enabled
 * - If not, enable it and reset
 * 
 * In debug builds, this does nothing.
 */
void security_init(void);

#ifdef __cplusplus
}
#endif

#endif // SECURITY_H
