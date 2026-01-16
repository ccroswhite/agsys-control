/**
 * @file agsys_approtect.h
 * @brief APPROTECT Configuration for Production Builds
 * 
 * When AGSYS_ENABLE_APPROTECT is defined, this configures the UICR
 * to disable SWD/JTAG debug access, preventing code readout.
 * 
 * WARNING: Once APPROTECT is enabled, the chip can only be recovered
 * by a full erase, which destroys all flash contents including the
 * bootloader. Only use for production builds!
 * 
 * Usage:
 *   make PRODUCTION=1 release
 */

#ifndef AGSYS_APPROTECT_H
#define AGSYS_APPROTECT_H

#include "nrf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * UICR APPROTECT CONFIGURATION
 * ========================================================================== */

#ifdef AGSYS_ENABLE_APPROTECT

/*
 * APPROTECT register (UICR address 0x10001208):
 *   0x00 = Protected (debug access disabled)
 *   0xFF = Unprotected (default, debug access enabled)
 * 
 * This is written to UICR during programming. The value is checked
 * at boot and if protected, SWD/JTAG is disabled.
 */

/* Place APPROTECT value in UICR section */
#if defined(__GNUC__)
__attribute__((section(".uicr_approtect")))
const uint32_t uicr_approtect = 0x00000000;  /* Protected */
#endif

#endif /* AGSYS_ENABLE_APPROTECT */

/* ==========================================================================
 * RUNTIME CHECK
 * ========================================================================== */

/**
 * @brief Check if APPROTECT is enabled
 * @return true if debug access is disabled
 */
static inline bool agsys_is_approtect_enabled(void)
{
    /* Read APPROTECT from UICR */
    uint32_t approtect = NRF_UICR->APPROTECT;
    return (approtect == 0x00000000);
}

/**
 * @brief Get APPROTECT status string
 * @return "LOCKED" or "UNLOCKED"
 */
static inline const char* agsys_approtect_status(void)
{
    return agsys_is_approtect_enabled() ? "LOCKED" : "UNLOCKED";
}

#ifdef __cplusplus
}
#endif

#endif /* AGSYS_APPROTECT_H */
