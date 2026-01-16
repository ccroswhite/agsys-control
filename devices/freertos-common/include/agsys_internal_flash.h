/**
 * @file agsys_internal_flash.h
 * @brief Internal Flash Memory Layout for nRF52 MCUs
 * 
 * Defines the canonical internal flash layout for all AgSys devices.
 * This header is shared between the bootloader and application firmware.
 * 
 * Supported MCUs:
 * - nRF52832 (512KB flash) with S132 SoftDevice
 * - nRF52840 (1MB flash) with S140 SoftDevice
 * 
 * Memory Layout (nRF52832 with S132 v7.2.0):
 *   0x00000000 - MBR (4KB)              - Nordic, frozen
 *   0x00001000 - SoftDevice S132 (152KB) - Nordic BLE stack
 *   0x00026000 - Application (264KB)    - User firmware
 *   0x00068000 - Recovery Loader (8KB)  - Minimal recovery
 *   0x0006A000 - Bootloader (32KB)      - OTA + signature verify
 *   0x00072000 - Bootloader Settings (8KB)
 *   0x00074000 - MBR Params (4KB)
 *   0x00075000 - Reserved (44KB)
 *   0x00080000 - End of flash
 * 
 * Memory Layout (nRF52840 with S140 v7.2.0):
 *   0x00000000 - MBR (4KB)              - Nordic, frozen
 *   0x00001000 - SoftDevice S140 (156KB) - Nordic BLE stack
 *   0x00027000 - Application (824KB)    - User firmware
 *   0x000F1000 - Recovery Loader (8KB)  - Minimal recovery
 *   0x000F3000 - Bootloader (32KB)      - OTA + signature verify
 *   0x000FB000 - Bootloader Settings (8KB)
 *   0x000FD000 - MBR Params (4KB)
 *   0x000FE000 - Reserved (8KB)
 *   0x00100000 - End of flash
 */

#ifndef AGSYS_INTERNAL_FLASH_H
#define AGSYS_INTERNAL_FLASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * MCU DETECTION
 * ========================================================================== */

#if defined(NRF52832_XXAA) || defined(NRF52832_XXAB)
    #define AGSYS_MCU_NRF52832      1
    #define AGSYS_FLASH_TOTAL_SIZE  (512 * 1024)    /* 512KB */
#elif defined(NRF52840_XXAA)
    #define AGSYS_MCU_NRF52840      1
    #define AGSYS_FLASH_TOTAL_SIZE  (1024 * 1024)   /* 1MB */
#else
    #error "Unsupported MCU - define NRF52832_XXAA or NRF52840_XXAA"
#endif

/* ==========================================================================
 * COMMON CONSTANTS
 * ========================================================================== */

#define AGSYS_FLASH_PAGE_SIZE       0x1000          /* 4KB page size */
#define AGSYS_BOOTLOADER_SIZE       0x8000          /* 32KB bootloader */
#define AGSYS_RECOVERY_SIZE         0x2000          /* 8KB recovery loader */
#define AGSYS_BL_SETTINGS_SIZE      0x2000          /* 8KB bootloader settings */
#define AGSYS_MBR_PARAMS_SIZE       0x1000          /* 4KB MBR params */

/* ==========================================================================
 * nRF52832 FLASH LAYOUT (S132 v7.2.0)
 * ========================================================================== */

#ifdef AGSYS_MCU_NRF52832

/* MBR - Nordic Master Boot Record (frozen) */
#define AGSYS_NRF52832_MBR_ADDR             0x00000000
#define AGSYS_NRF52832_MBR_SIZE             0x00001000  /* 4KB */

/* SoftDevice S132 v7.2.0 */
#define AGSYS_NRF52832_SD_ADDR              0x00001000
#define AGSYS_NRF52832_SD_SIZE              0x00025000  /* 148KB */

/* Application */
#define AGSYS_NRF52832_APP_ADDR             0x00026000
#define AGSYS_NRF52832_APP_SIZE             0x00042000  /* 264KB */
#define AGSYS_NRF52832_APP_END              0x00068000

/* Recovery Loader */
#define AGSYS_NRF52832_RECOVERY_ADDR        0x00068000
#define AGSYS_NRF52832_RECOVERY_SIZE        0x00002000  /* 8KB */

/* Bootloader */
#define AGSYS_NRF52832_BL_ADDR              0x0006A000
#define AGSYS_NRF52832_BL_SIZE              0x00008000  /* 32KB */

/* Bootloader Settings */
#define AGSYS_NRF52832_BL_SETTINGS_ADDR     0x00072000
#define AGSYS_NRF52832_BL_SETTINGS_SIZE     0x00002000  /* 8KB */

/* MBR Params */
#define AGSYS_NRF52832_MBR_PARAMS_ADDR      0x00074000
#define AGSYS_NRF52832_MBR_PARAMS_SIZE      0x00001000  /* 4KB */

/* End of flash */
#define AGSYS_NRF52832_FLASH_END            0x00080000  /* 512KB */

/* Aliases for generic code */
#define AGSYS_APP_ADDR                      AGSYS_NRF52832_APP_ADDR
#define AGSYS_APP_SIZE                      AGSYS_NRF52832_APP_SIZE
#define AGSYS_APP_END                       AGSYS_NRF52832_APP_END
#define AGSYS_RECOVERY_ADDR                 AGSYS_NRF52832_RECOVERY_ADDR
#define AGSYS_BL_ADDR                       AGSYS_NRF52832_BL_ADDR
#define AGSYS_BL_SIZE                       AGSYS_NRF52832_BL_SIZE
#define AGSYS_BL_SETTINGS_ADDR              AGSYS_NRF52832_BL_SETTINGS_ADDR

#endif /* AGSYS_MCU_NRF52832 */

/* ==========================================================================
 * nRF52840 FLASH LAYOUT (S140 v7.2.0)
 * ========================================================================== */

#ifdef AGSYS_MCU_NRF52840

/* MBR - Nordic Master Boot Record (frozen) */
#define AGSYS_NRF52840_MBR_ADDR             0x00000000
#define AGSYS_NRF52840_MBR_SIZE             0x00001000  /* 4KB */

/* SoftDevice S140 v7.2.0 */
#define AGSYS_NRF52840_SD_ADDR              0x00001000
#define AGSYS_NRF52840_SD_SIZE              0x00026000  /* 152KB */

/* Application */
#define AGSYS_NRF52840_APP_ADDR             0x00027000
#define AGSYS_NRF52840_APP_SIZE             0x000CA000  /* 808KB */
#define AGSYS_NRF52840_APP_END              0x000F1000

/* Recovery Loader */
#define AGSYS_NRF52840_RECOVERY_ADDR        0x000F1000
#define AGSYS_NRF52840_RECOVERY_SIZE        0x00002000  /* 8KB */

/* Bootloader */
#define AGSYS_NRF52840_BL_ADDR              0x000F3000
#define AGSYS_NRF52840_BL_SIZE              0x00008000  /* 32KB */

/* Bootloader Settings */
#define AGSYS_NRF52840_BL_SETTINGS_ADDR     0x000FB000
#define AGSYS_NRF52840_BL_SETTINGS_SIZE     0x00002000  /* 8KB */

/* MBR Params */
#define AGSYS_NRF52840_MBR_PARAMS_ADDR      0x000FD000
#define AGSYS_NRF52840_MBR_PARAMS_SIZE      0x00001000  /* 4KB */

/* End of flash */
#define AGSYS_NRF52840_FLASH_END            0x00100000  /* 1MB */

/* Aliases for generic code */
#define AGSYS_APP_ADDR                      AGSYS_NRF52840_APP_ADDR
#define AGSYS_APP_SIZE                      AGSYS_NRF52840_APP_SIZE
#define AGSYS_APP_END                       AGSYS_NRF52840_APP_END
#define AGSYS_RECOVERY_ADDR                 AGSYS_NRF52840_RECOVERY_ADDR
#define AGSYS_BL_ADDR                       AGSYS_NRF52840_BL_ADDR
#define AGSYS_BL_SIZE                       AGSYS_NRF52840_BL_SIZE
#define AGSYS_BL_SETTINGS_ADDR              AGSYS_NRF52840_BL_SETTINGS_ADDR

#endif /* AGSYS_MCU_NRF52840 */

/* ==========================================================================
 * VALIDATION
 * ========================================================================== */

/* Compile-time checks */
#if defined(AGSYS_MCU_NRF52832)
_Static_assert(AGSYS_NRF52832_APP_ADDR + AGSYS_NRF52832_APP_SIZE == AGSYS_NRF52832_APP_END,
               "nRF52832 app region mismatch");
_Static_assert(AGSYS_NRF52832_BL_ADDR + AGSYS_NRF52832_BL_SIZE == AGSYS_NRF52832_BL_SETTINGS_ADDR,
               "nRF52832 bootloader region mismatch");
#endif

#if defined(AGSYS_MCU_NRF52840)
_Static_assert(AGSYS_NRF52840_APP_ADDR + AGSYS_NRF52840_APP_SIZE == AGSYS_NRF52840_APP_END,
               "nRF52840 app region mismatch");
_Static_assert(AGSYS_NRF52840_BL_ADDR + AGSYS_NRF52840_BL_SIZE == AGSYS_NRF52840_BL_SETTINGS_ADDR,
               "nRF52840 bootloader region mismatch");
#endif

#ifdef __cplusplus
}
#endif

#endif /* AGSYS_INTERNAL_FLASH_H */
