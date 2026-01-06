/**
 * @file firmware_backup.cpp
 * @brief Firmware backup and rollback implementation
 * 
 * Uses Adafruit SPIFlash library for W25Q16 access.
 * Firmware is encrypted using AES-256-CTR before storage.
 * 
 * Internal flash write uses nRF52 NVMC (Non-Volatile Memory Controller).
 */

#include "firmware_backup.h"
#include "firmware_crypto.h"
#include "debug_log.h"
#include "config.h"
#include <string.h>
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_SPIFlash.h>

// nRF52 NVMC (Non-Volatile Memory Controller) access
// Direct register access for flash operations
#define NVMC_REG_READY          (*(volatile uint32_t*)0x4001E400)
#define NVMC_REG_CONFIG         (*(volatile uint32_t*)0x4001E504)
#define NVMC_REG_ERASEPAGE      (*(volatile uint32_t*)0x4001E508)

#define NVMC_MODE_READ   0  // Read only
#define NVMC_MODE_WRITE  1  // Write enabled
#define NVMC_MODE_ERASE  2  // Erase enabled

// Flash transport and instance
static Adafruit_FlashTransport_SPI s_flashTransport(PIN_FLASH_CS, &SPI);
static Adafruit_SPIFlash s_flash(&s_flashTransport);

// Backup header (cached in RAM)
static FwBackupHeader s_header;
static bool s_initialized = false;

// Validation timer state
static uint32_t s_validationStartTime = 0;
static bool s_validationTimerActive = false;

// nRF52 internal flash parameters
#define NRF52_FLASH_PAGE_SIZE       4096
#define NRF52_APP_START_ADDR        0x26000     // After SoftDevice (SD132 v6.x)
#define NRF52_APP_END_ADDR          0x7A000     // Before bootloader settings

// CRC32 implementation
static uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

// Read header from flash
static bool readHeader(void) {
    if (!s_flash.readBuffer(FW_BACKUP_HEADER_ADDR, (uint8_t*)&s_header, sizeof(s_header))) {
        return false;
    }
    
    // Verify magic
    if (s_header.magic != FW_BACKUP_MAGIC) {
        return false;
    }
    
    // Verify CRC
    uint32_t storedCrc = s_header.headerCrc;
    s_header.headerCrc = 0;
    uint32_t calcCrc = crc32((uint8_t*)&s_header, sizeof(s_header) - 4);
    s_header.headerCrc = storedCrc;
    
    return (storedCrc == calcCrc);
}

// Write header to flash
static bool writeHeader(void) {
    // Calculate CRC
    s_header.headerCrc = 0;
    s_header.headerCrc = crc32((uint8_t*)&s_header, sizeof(s_header) - 4);
    
    // Erase header sector (4KB, but we only use 256 bytes)
    if (!s_flash.eraseSector(0)) {
        return false;
    }
    
    // Write header
    return s_flash.writeBuffer(FW_BACKUP_HEADER_ADDR, (uint8_t*)&s_header, sizeof(s_header));
}

// Initialize header with defaults
static void initHeader(void) {
    memset(&s_header, 0, sizeof(s_header));
    s_header.magic = FW_BACKUP_MAGIC;
    s_header.version = 2;  // Version 2 = with expectedVersion/failedVersion
    s_header.activeSlot = 0;
    s_header.slotAStatus = FW_BACKUP_STATUS_EMPTY;
    s_header.slotBStatus = FW_BACKUP_STATUS_EMPTY;
}

// Compare version arrays
static bool versionsMatch(const uint8_t* v1, const uint8_t* v2) {
    return (v1[0] == v2[0] && v1[1] == v2[1] && v1[2] == v2[2]);
}

// Get current running version
static void getCurrentVersion(uint8_t* version) {
    version[0] = FIRMWARE_VERSION_MAJOR;
    version[1] = FIRMWARE_VERSION_MINOR;
    version[2] = FIRMWARE_VERSION_PATCH;
    version[3] = 0;
}

bool fw_backup_init(void) {
    if (s_initialized) {
        return true;
    }
    
    // Initialize crypto first
    fw_crypto_init();
    
    // Initialize flash
    if (!s_flash.begin()) {
        DEBUG_PRINTLN("FW Backup: Flash init failed");
        return false;
    }
    
    DEBUG_PRINTF("FW Backup: Flash size = %lu bytes\n", s_flash.size());
    
    // Try to read existing header
    if (!readHeader()) {
        DEBUG_PRINTLN("FW Backup: No valid header, initializing");
        initHeader();
        if (!writeHeader()) {
            DEBUG_PRINTLN("FW Backup: Failed to write header");
            return false;
        }
    }
    
    DEBUG_PRINTF("FW Backup: Active slot = %c, Rollbacks = %d\n", 
                 'A' + s_header.activeSlot, s_header.rollbackCount);
    
    s_initialized = true;
    return true;
}

bool fw_backup_check_rollback(void) {
    if (!s_initialized) {
        return false;
    }
    
    // Get current running version
    uint8_t currentVersion[4];
    getCurrentVersion(currentVersion);
    
    // Check if an expected version is set (non-zero means OTA was attempted)
    bool hasExpectedVersion = (s_header.expectedVersion[0] != 0 || 
                               s_header.expectedVersion[1] != 0 || 
                               s_header.expectedVersion[2] != 0);
    
    if (!hasExpectedVersion) {
        // No OTA was attempted, nothing to validate
        return false;
    }
    
    // Compare running version with expected version
    if (versionsMatch(currentVersion, s_header.expectedVersion)) {
        // Version matches - OTA succeeded, clear expected version
        DEBUG_PRINTF("FW Backup: Running expected version %d.%d.%d\n",
                     currentVersion[0], currentVersion[1], currentVersion[2]);
        return false;
    }
    
    // Version mismatch - OTA failed or rollback already happened
    DEBUG_PRINTF("FW Backup: Version mismatch! Running %d.%d.%d, expected %d.%d.%d\n",
                 currentVersion[0], currentVersion[1], currentVersion[2],
                 s_header.expectedVersion[0], s_header.expectedVersion[1], 
                 s_header.expectedVersion[2]);
    
    // Record the failed version
    memcpy(s_header.failedVersion, s_header.expectedVersion, 4);
    
    // Clear expected version (so we don't keep trying to rollback)
    memset(s_header.expectedVersion, 0, 4);
    
    // Check if we have a valid backup to restore
    uint8_t backupSlot = 1 - s_header.activeSlot;
    uint8_t backupStatus = (backupSlot == 0) ? s_header.slotAStatus : s_header.slotBStatus;
    
    if (backupStatus != FW_BACKUP_STATUS_VALID) {
        DEBUG_PRINTLN("FW Backup: No valid backup to restore!");
        debugLog_recordError(ERR_ROLLBACK_TRIGGERED);
        writeHeader();
        return false;
    }
    
    // Increment rollback count
    s_header.rollbackCount++;
    s_header.activeSlot = backupSlot;
    writeHeader();
    
    DEBUG_PRINTF("FW Backup: Rolling back to slot %c (v%d.%d.%d)\n",
                 'A' + backupSlot,
                 (backupSlot == 0) ? s_header.slotAVersion[0] : s_header.slotBVersion[0],
                 (backupSlot == 0) ? s_header.slotAVersion[1] : s_header.slotBVersion[1],
                 (backupSlot == 0) ? s_header.slotAVersion[2] : s_header.slotBVersion[2]);
    
    // Record rollback in debug log
    debugLog_recordRollback();
    
    // Restore backup (does not return on success)
    fw_backup_restore();
    
    return true;
}

void fw_backup_validate(void) {
    if (!s_initialized) {
        return;
    }
    
    // Stop validation timer
    s_validationTimerActive = false;
    
    uint8_t currentSlot = s_header.activeSlot;
    
    // Mark current slot as valid
    if (currentSlot == 0) {
        s_header.slotAStatus = FW_BACKUP_STATUS_VALID;
    } else {
        s_header.slotBStatus = FW_BACKUP_STATUS_VALID;
    }
    
    // Clear expected version (validation complete)
    memset(s_header.expectedVersion, 0, 4);
    s_header.validationStartMs = 0;
    writeHeader();
    
    DEBUG_PRINTLN("FW Backup: Firmware validated");
    
    // Also mark in debug log
    debugLog_markValidated();
}

void fw_backup_set_expected_version(uint8_t major, uint8_t minor, uint8_t patch) {
    if (!s_initialized) {
        return;
    }
    
    s_header.expectedVersion[0] = major;
    s_header.expectedVersion[1] = minor;
    s_header.expectedVersion[2] = patch;
    s_header.expectedVersion[3] = 0;
    writeHeader();
    
    DEBUG_PRINTF("FW Backup: Expected version set to %d.%d.%d\n", major, minor, patch);
}

bool fw_backup_was_rollback(void) {
    if (!s_initialized) {
        return false;
    }
    
    // If failedVersion is non-zero, a rollback occurred
    return (s_header.failedVersion[0] != 0 || 
            s_header.failedVersion[1] != 0 || 
            s_header.failedVersion[2] != 0);
}

bool fw_backup_get_failed_version(uint8_t* major, uint8_t* minor, uint8_t* patch) {
    if (!s_initialized) {
        return false;
    }
    
    if (!fw_backup_was_rollback()) {
        return false;
    }
    
    *major = s_header.failedVersion[0];
    *minor = s_header.failedVersion[1];
    *patch = s_header.failedVersion[2];
    return true;
}

void fw_backup_start_validation_timer(void) {
    if (!s_initialized) {
        return;
    }
    
    s_validationStartTime = millis();
    s_validationTimerActive = true;
    
    // Mark current slot as pending validation
    uint8_t currentSlot = s_header.activeSlot;
    if (currentSlot == 0) {
        s_header.slotAStatus = FW_BACKUP_STATUS_PENDING;
    } else {
        s_header.slotBStatus = FW_BACKUP_STATUS_PENDING;
    }
    s_header.validationStartMs = s_validationStartTime;
    writeHeader();
    
    DEBUG_PRINTLN("FW Backup: Validation timer started");
}

bool fw_backup_check_validation_timeout(void) {
    if (!s_initialized || !s_validationTimerActive) {
        return false;
    }
    
    uint32_t elapsed = millis() - s_validationStartTime;
    if (elapsed >= FW_VALIDATION_TIMEOUT_MS) {
        DEBUG_PRINTLN("FW Backup: Validation timeout expired!");
        s_validationTimerActive = false;
        
        // Trigger rollback
        return fw_backup_check_rollback();
    }
    
    return false;
}

bool fw_backup_is_validation_pending(void) {
    if (!s_initialized) {
        return false;
    }
    
    uint8_t currentSlot = s_header.activeSlot;
    uint8_t currentStatus = (currentSlot == 0) ? s_header.slotAStatus : s_header.slotBStatus;
    
    return (currentStatus == FW_BACKUP_STATUS_PENDING);
}

bool fw_backup_create(uint32_t fwSize) {
    if (!s_initialized) {
        return false;
    }
    
    if (fwSize > FW_BACKUP_SLOT_SIZE) {
        DEBUG_PRINTLN("FW Backup: Firmware too large for backup slot");
        return false;
    }
    
    // Determine which slot to use (opposite of current)
    uint8_t backupSlot = 1 - s_header.activeSlot;
    uint32_t slotAddr = (backupSlot == 0) ? FW_BACKUP_SLOT_A_ADDR : FW_BACKUP_SLOT_B_ADDR;
    
    DEBUG_PRINTF("FW Backup: Creating backup in slot %c (%lu bytes)\n", 
                 'A' + backupSlot, fwSize);
    
    // Erase backup slot (need to erase in 4KB sectors)
    uint32_t sectorsNeeded = (fwSize + 4095) / 4096;
    for (uint32_t i = 0; i < sectorsNeeded; i++) {
        if (!s_flash.eraseSector((slotAddr / 4096) + i)) {
            DEBUG_PRINTF("FW Backup: Failed to erase sector %lu\n", i);
            return false;
        }
    }
    
    // Read from internal flash, encrypt, write to external flash
    // Internal flash starts at 0x00000000 for nRF52
    uint8_t buffer[256];
    uint32_t offset = 0;
    uint32_t crc = 0xFFFFFFFF;
    
    while (offset < fwSize) {
        uint32_t chunkSize = (fwSize - offset > 256) ? 256 : (fwSize - offset);
        
        // Read from internal flash
        memcpy(buffer, (void*)(0x00000000 + offset), chunkSize);
        
        // Update CRC before encryption
        for (uint32_t i = 0; i < chunkSize; i++) {
            crc ^= buffer[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }
        
        // Encrypt chunk
        // Pad to 16-byte boundary for AES
        uint32_t paddedSize = (chunkSize + 15) & ~15;
        if (paddedSize > chunkSize) {
            memset(buffer + chunkSize, 0, paddedSize - chunkSize);
        }
        fw_crypto_encrypt(buffer, paddedSize, offset);
        
        // Write to external flash
        if (!s_flash.writeBuffer(slotAddr + offset, buffer, paddedSize)) {
            DEBUG_PRINTF("FW Backup: Write failed at offset %lu\n", offset);
            return false;
        }
        
        offset += chunkSize;
    }
    
    crc = ~crc;
    
    // Update header
    if (backupSlot == 0) {
        s_header.slotAStatus = FW_BACKUP_STATUS_VALID;
        s_header.slotASize = fwSize;
        s_header.slotACrc = crc;
        s_header.slotAVersion[0] = FIRMWARE_VERSION_MAJOR;
        s_header.slotAVersion[1] = FIRMWARE_VERSION_MINOR;
        s_header.slotAVersion[2] = FIRMWARE_VERSION_PATCH;
        s_header.slotAVersion[3] = 0;
    } else {
        s_header.slotBStatus = FW_BACKUP_STATUS_VALID;
        s_header.slotBSize = fwSize;
        s_header.slotBCrc = crc;
        s_header.slotBVersion[0] = FIRMWARE_VERSION_MAJOR;
        s_header.slotBVersion[1] = FIRMWARE_VERSION_MINOR;
        s_header.slotBVersion[2] = FIRMWARE_VERSION_PATCH;
        s_header.slotBVersion[3] = 0;
    }
    
    writeHeader();
    
    DEBUG_PRINTF("FW Backup: Backup created, CRC = 0x%08lX\n", crc);
    return true;
}

bool fw_backup_restore(void) {
    if (!s_initialized) {
        return false;
    }
    
    uint8_t backupSlot = s_header.activeSlot;
    uint32_t slotAddr = (backupSlot == 0) ? FW_BACKUP_SLOT_A_ADDR : FW_BACKUP_SLOT_B_ADDR;
    uint32_t fwSize = (backupSlot == 0) ? s_header.slotASize : s_header.slotBSize;
    uint32_t expectedCrc = (backupSlot == 0) ? s_header.slotACrc : s_header.slotBCrc;
    
    DEBUG_PRINTF("FW Backup: Restoring from slot %c (%lu bytes)\n", 
                 'A' + backupSlot, fwSize);
    
    if (fwSize == 0 || fwSize > FW_BACKUP_SLOT_SIZE) {
        DEBUG_PRINTLN("FW Backup: Invalid backup size");
        return false;
    }
    
    // Read, decrypt, verify CRC, then write to internal flash
    // This is a critical operation - if it fails, device may be bricked
    
    // First pass: verify CRC without writing
    uint8_t buffer[256];
    uint32_t offset = 0;
    uint32_t crc = 0xFFFFFFFF;
    
    while (offset < fwSize) {
        uint32_t chunkSize = (fwSize - offset > 256) ? 256 : (fwSize - offset);
        uint32_t paddedSize = (chunkSize + 15) & ~15;
        
        // Read from external flash
        if (!s_flash.readBuffer(slotAddr + offset, buffer, paddedSize)) {
            DEBUG_PRINTF("FW Backup: Read failed at offset %lu\n", offset);
            return false;
        }
        
        // Decrypt chunk
        fw_crypto_decrypt(buffer, paddedSize, offset);
        
        // Update CRC
        for (uint32_t i = 0; i < chunkSize; i++) {
            crc ^= buffer[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }
        
        offset += chunkSize;
    }
    
    crc = ~crc;
    
    if (crc != expectedCrc) {
        DEBUG_PRINTF("FW Backup: CRC mismatch! Expected 0x%08lX, got 0x%08lX\n", 
                     expectedCrc, crc);
        return false;
    }
    
    DEBUG_PRINTLN("FW Backup: CRC verified, writing to internal flash...");
    
    // Second pass: actually write to internal flash
    // WARNING: This erases and rewrites the running firmware!
    // We must disable interrupts and be very careful here
    
    // Disable all interrupts - this is critical
    __disable_irq();
    
    // Calculate number of pages to erase
    uint32_t pagesNeeded = (fwSize + NRF52_FLASH_PAGE_SIZE - 1) / NRF52_FLASH_PAGE_SIZE;
    
    // Enable erase mode
    NVMC_REG_CONFIG = NVMC_MODE_ERASE;
    while (NVMC_REG_READY == 0);
    
    // Erase application pages
    for (uint32_t page = 0; page < pagesNeeded; page++) {
        uint32_t pageAddr = NRF52_APP_START_ADDR + (page * NRF52_FLASH_PAGE_SIZE);
        NVMC_REG_ERASEPAGE = pageAddr;
        while (NVMC_REG_READY == 0);
    }
    
    // Enable write mode
    NVMC_REG_CONFIG = NVMC_MODE_WRITE;
    while (NVMC_REG_READY == 0);
    
    // Second pass: read from external flash, decrypt, write to internal flash
    offset = 0;
    while (offset < fwSize) {
        uint32_t chunkSize = (fwSize - offset > 256) ? 256 : (fwSize - offset);
        uint32_t paddedSize = (chunkSize + 15) & ~15;
        
        // Read from external flash
        s_flash.readBuffer(slotAddr + offset, buffer, paddedSize);
        
        // Decrypt chunk
        fw_crypto_decrypt(buffer, paddedSize, offset);
        
        // Write to internal flash (must be word-aligned, 4 bytes at a time)
        uint32_t destAddr = NRF52_APP_START_ADDR + offset;
        uint32_t* src = (uint32_t*)buffer;
        uint32_t wordsToWrite = (chunkSize + 3) / 4;
        
        for (uint32_t w = 0; w < wordsToWrite; w++) {
            *(volatile uint32_t*)(destAddr + w * 4) = src[w];
            while (NVMC_REG_READY == 0);
        }
        
        offset += chunkSize;
    }
    
    // Disable write mode (read-only)
    NVMC_REG_CONFIG = NVMC_MODE_READ;
    while (NVMC_REG_READY == 0);
    
    // Re-enable interrupts (though we're about to reset anyway)
    __enable_irq();
    
    DEBUG_PRINTLN("FW Backup: Restore complete, resetting...");
    delay(100);
    
    // Trigger system reset to boot into restored firmware
    NVIC_SystemReset();
    
    // Should not reach here
    return true;
}

void fw_backup_force_rollback(void) {
    if (!s_initialized) {
        return;
    }
    
    DEBUG_PRINTLN("FW Backup: Forced rollback requested");
    
    // Switch to backup slot
    uint8_t backupSlot = 1 - s_header.activeSlot;
    uint8_t backupStatus = (backupSlot == 0) ? s_header.slotAStatus : s_header.slotBStatus;
    
    if (backupStatus != FW_BACKUP_STATUS_VALID) {
        DEBUG_PRINTLN("FW Backup: No valid backup available");
        return;
    }
    
    s_header.activeSlot = backupSlot;
    s_header.rollbackCount++;
    writeHeader();
    
    debugLog_recordRollback();
    
    fw_backup_restore();
}

bool fw_backup_get_status(FwBackupHeader* header) {
    if (!s_initialized) {
        return false;
    }
    
    memcpy(header, &s_header, sizeof(FwBackupHeader));
    return true;
}

void fw_backup_erase_all(void) {
    if (!s_initialized) {
        return;
    }
    
    DEBUG_PRINTLN("FW Backup: Erasing all backup data");
    
    // Erase first 1MB (both slots + header)
    // This takes a while...
    for (uint32_t addr = 0; addr < 0x100000; addr += 0x10000) {
        s_flash.eraseBlock(addr);
    }
    
    // Reinitialize header
    initHeader();
    writeHeader();
    
    DEBUG_PRINTLN("FW Backup: Erase complete");
}

uint8_t fw_backup_get_rollback_count(void) {
    return s_header.rollbackCount;
}
