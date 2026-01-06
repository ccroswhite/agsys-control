/**
 * @file security.cpp
 * @brief Security and code protection implementation
 */

#include "security.h"
#include <nrf.h>

uint64_t security_getDeviceId(void) {
    uint32_t low = NRF_FICR->DEVICEID[0];
    uint32_t high = NRF_FICR->DEVICEID[1];
    return ((uint64_t)high << 32) | low;
}

void security_getDeviceIdWords(uint32_t* low, uint32_t* high) {
    if (low) *low = NRF_FICR->DEVICEID[0];
    if (high) *high = NRF_FICR->DEVICEID[1];
}

void security_getDeviceIdBytes(uint8_t* buffer) {
    if (!buffer) return;
    
    uint32_t low = NRF_FICR->DEVICEID[0];
    uint32_t high = NRF_FICR->DEVICEID[1];
    
    // Big-endian format (most significant byte first)
    buffer[0] = (high >> 24) & 0xFF;
    buffer[1] = (high >> 16) & 0xFF;
    buffer[2] = (high >> 8) & 0xFF;
    buffer[3] = high & 0xFF;
    buffer[4] = (low >> 24) & 0xFF;
    buffer[5] = (low >> 16) & 0xFF;
    buffer[6] = (low >> 8) & 0xFF;
    buffer[7] = low & 0xFF;
}

bool security_isApprotectEnabled(void) {
    // APPROTECT is enabled when the register value is NOT 0xFF
    // 0xFF = disabled (default, unprogrammed flash)
    // 0x00 = enabled (protected)
    return (NRF_UICR->APPROTECT != 0xFFFFFFFF);
}

void security_enableApprotect(void) {
    // Don't do anything if already protected
    if (security_isApprotectEnabled()) {
        return;
    }
    
    // Enable write mode for NVMC (Non-Volatile Memory Controller)
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
    
    // Write 0x00 to APPROTECT to enable protection
    // This is a one-way operation - cannot be undone without full erase
    NRF_UICR->APPROTECT = 0x00;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
    
    // Disable write mode
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
    
    // Reset to apply the protection
    // Protection only takes effect after reset
    NVIC_SystemReset();
    
    // This point is never reached
}

void security_init(void) {
#ifdef RELEASE_BUILD
    // In release builds, ensure APPROTECT is enabled
    if (!security_isApprotectEnabled()) {
        // Enable protection and reset
        // This function does not return
        security_enableApprotect();
    }
#endif
    // In debug builds, do nothing - leave debug port accessible
}
