/**
 * @file moisture_cal.cpp
 * @brief Moisture Probe Calibration Storage Implementation
 * 
 * Stores calibration data in FRAM protected region.
 */

#include <Arduino.h>
#include "moisture_cal.h"
#include "nvram.h"
#include "nvram_layout.h"

// FRAM address for moisture calibration block
// Using the reserved area in protected region (0x00C0 - 0x00FF, 64 bytes)
// But we need 140 bytes, so we'll use part of the calibration area
// Repurposing NVRAM_CALIBRATION_ADDR (0x0000) since old H-bridge cal is obsolete
#define MOISTURE_CAL_ADDR       NVRAM_CALIBRATION_ADDR

// External NVRAM instance (defined in nvram.cpp or main.cpp)
extern NVRAM nvram;

// Local copy of calibration data
static MoistureCalBlock s_calBlock;
static bool s_initialized = false;

// Forward declarations
static uint32_t calculateCrc(const MoistureCalBlock* block);
static bool loadFromFram(void);
static bool saveToFram(void);

/**
 * @brief Calculate CRC32 of calibration block (excluding CRC field)
 */
static uint32_t calculateCrc(const MoistureCalBlock* block) {
    // Simple CRC32 implementation
    const uint8_t* data = (const uint8_t*)block;
    size_t len = sizeof(MoistureCalBlock) - sizeof(uint32_t);  // Exclude CRC field
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

/**
 * @brief Load calibration data from FRAM
 */
static bool loadFromFram(void) {
    // Read entire block from FRAM
    if (!nvram.read(MOISTURE_CAL_ADDR, (uint8_t*)&s_calBlock, sizeof(s_calBlock))) {
        DEBUG_PRINTLN("MoistureCal: FRAM read failed");
        return false;
    }
    
    // Verify magic
    if (s_calBlock.magic != MOISTURE_CAL_MAGIC) {
        DEBUG_PRINTLN("MoistureCal: Invalid magic, initializing");
        return false;
    }
    
    // Verify CRC
    uint32_t expectedCrc = calculateCrc(&s_calBlock);
    if (s_calBlock.crc != expectedCrc) {
        DEBUG_PRINTF("MoistureCal: CRC mismatch (got 0x%08lX, expected 0x%08lX)\n",
                     s_calBlock.crc, expectedCrc);
        return false;
    }
    
    // Check version compatibility
    if (s_calBlock.version != MOISTURE_CAL_VERSION) {
        DEBUG_PRINTF("MoistureCal: Version mismatch (got %d, expected %d)\n",
                     s_calBlock.version, MOISTURE_CAL_VERSION);
        // Could add migration logic here for future versions
        return false;
    }
    
    DEBUG_PRINTLN("MoistureCal: Loaded from FRAM");
    return true;
}

/**
 * @brief Save calibration data to FRAM
 */
static bool saveToFram(void) {
    // Update CRC before saving
    s_calBlock.crc = calculateCrc(&s_calBlock);
    
    // Write to FRAM
    if (!nvram.write(MOISTURE_CAL_ADDR, (const uint8_t*)&s_calBlock, sizeof(s_calBlock))) {
        DEBUG_PRINTLN("MoistureCal: FRAM write failed");
        return false;
    }
    
    DEBUG_PRINTLN("MoistureCal: Saved to FRAM");
    return true;
}

/**
 * @brief Initialize with default values
 */
static void initDefaults(void) {
    memset(&s_calBlock, 0, sizeof(s_calBlock));
    s_calBlock.magic = MOISTURE_CAL_MAGIC;
    s_calBlock.version = MOISTURE_CAL_VERSION;
    s_calBlock.numProbes = NUM_MOISTURE_PROBES;
    
    // All probes start uncalibrated
    for (uint8_t i = 0; i < MAX_PROBES; i++) {
        s_calBlock.probes[i].status = 0;
    }
}

/**
 * @brief Initialize moisture calibration system
 */
bool moistureCal_init(void) {
    if (s_initialized) {
        return true;
    }
    
    // Try to load from FRAM
    if (!loadFromFram()) {
        // Initialize with defaults
        initDefaults();
        // Don't save yet - wait for actual calibration
    }
    
    s_initialized = true;
    
    // Log calibration status
    for (uint8_t i = 0; i < NUM_MOISTURE_PROBES; i++) {
        DEBUG_PRINTF("MoistureCal: Probe %d - f_air=%lu, f_dry=%lu, f_wet=%lu, status=0x%02X\n",
                     i, s_calBlock.probes[i].f_air, s_calBlock.probes[i].f_dry,
                     s_calBlock.probes[i].f_wet, s_calBlock.probes[i].status);
    }
    
    return true;
}

/**
 * @brief Check if calibration data exists and is valid
 */
bool moistureCal_isValid(void) {
    return s_initialized && (s_calBlock.magic == MOISTURE_CAL_MAGIC);
}

/**
 * @brief Check if a specific probe needs f_air calibration
 */
bool moistureCal_needsAirCal(uint8_t probeIndex) {
    if (!s_initialized || probeIndex >= MAX_PROBES) {
        return true;
    }
    return !(s_calBlock.probes[probeIndex].status & CAL_STATUS_F_AIR_SET);
}

/**
 * @brief Check if a specific probe has complete field calibration
 */
bool moistureCal_isFieldCalComplete(uint8_t probeIndex) {
    if (!s_initialized || probeIndex >= MAX_PROBES) {
        return false;
    }
    uint8_t required = CAL_STATUS_F_DRY_SET | CAL_STATUS_F_WET_SET;
    return (s_calBlock.probes[probeIndex].status & required) == required;
}

/**
 * @brief Get calibration data for a probe
 */
bool moistureCal_get(uint8_t probeIndex, MoistureCalibration* cal) {
    if (!s_initialized || probeIndex >= MAX_PROBES || cal == NULL) {
        return false;
    }
    
    memcpy(cal, &s_calBlock.probes[probeIndex], sizeof(MoistureCalibration));
    return true;
}

/**
 * @brief Set f_air calibration for a probe
 */
bool moistureCal_setAir(uint8_t probeIndex, uint32_t f_air) {
    if (!s_initialized || probeIndex >= MAX_PROBES) {
        return false;
    }
    
    s_calBlock.probes[probeIndex].f_air = f_air;
    s_calBlock.probes[probeIndex].status |= CAL_STATUS_F_AIR_SET;
    s_calBlock.probes[probeIndex].cal_timestamp = 0;  // TODO: get real timestamp
    
    DEBUG_PRINTF("MoistureCal: Probe %d f_air set to %lu Hz\n", probeIndex, f_air);
    
    return saveToFram();
}

/**
 * @brief Set f_dry calibration for a probe
 */
bool moistureCal_setDry(uint8_t probeIndex, uint32_t f_dry) {
    if (!s_initialized || probeIndex >= MAX_PROBES) {
        return false;
    }
    
    s_calBlock.probes[probeIndex].f_dry = f_dry;
    s_calBlock.probes[probeIndex].status |= CAL_STATUS_F_DRY_SET;
    s_calBlock.probes[probeIndex].cal_timestamp = 0;  // TODO: get real timestamp
    
    DEBUG_PRINTF("MoistureCal: Probe %d f_dry set to %lu Hz\n", probeIndex, f_dry);
    
    return saveToFram();
}

/**
 * @brief Set f_wet calibration for a probe
 */
bool moistureCal_setWet(uint8_t probeIndex, uint32_t f_wet) {
    if (!s_initialized || probeIndex >= MAX_PROBES) {
        return false;
    }
    
    s_calBlock.probes[probeIndex].f_wet = f_wet;
    s_calBlock.probes[probeIndex].status |= CAL_STATUS_F_WET_SET;
    s_calBlock.probes[probeIndex].cal_timestamp = 0;  // TODO: get real timestamp
    
    DEBUG_PRINTF("MoistureCal: Probe %d f_wet set to %lu Hz\n", probeIndex, f_wet);
    
    return saveToFram();
}

/**
 * @brief Set all calibration values for a probe at once
 */
bool moistureCal_setAll(uint8_t probeIndex, uint32_t f_air, uint32_t f_dry, uint32_t f_wet) {
    if (!s_initialized || probeIndex >= MAX_PROBES) {
        return false;
    }
    
    MoistureCalibration* cal = &s_calBlock.probes[probeIndex];
    
    if (f_air != 0) {
        cal->f_air = f_air;
        cal->status |= CAL_STATUS_F_AIR_SET;
    }
    if (f_dry != 0) {
        cal->f_dry = f_dry;
        cal->status |= CAL_STATUS_F_DRY_SET;
    }
    if (f_wet != 0) {
        cal->f_wet = f_wet;
        cal->status |= CAL_STATUS_F_WET_SET;
    }
    
    cal->cal_timestamp = 0;  // TODO: get real timestamp
    
    DEBUG_PRINTF("MoistureCal: Probe %d set all - f_air=%lu, f_dry=%lu, f_wet=%lu\n",
                 probeIndex, cal->f_air, cal->f_dry, cal->f_wet);
    
    return saveToFram();
}

/**
 * @brief Clear calibration for a probe
 */
bool moistureCal_clear(uint8_t probeIndex) {
    if (!s_initialized || probeIndex >= MAX_PROBES) {
        return false;
    }
    
    memset(&s_calBlock.probes[probeIndex], 0, sizeof(MoistureCalibration));
    
    DEBUG_PRINTF("MoistureCal: Probe %d calibration cleared\n", probeIndex);
    
    return saveToFram();
}

/**
 * @brief Clear all calibration data
 */
bool moistureCal_clearAll(void) {
    if (!s_initialized) {
        return false;
    }
    
    initDefaults();
    
    DEBUG_PRINTLN("MoistureCal: All calibration cleared");
    
    return saveToFram();
}

/**
 * @brief Get calibration status flags for a probe
 */
uint8_t moistureCal_getStatus(uint8_t probeIndex) {
    if (!s_initialized || probeIndex >= MAX_PROBES) {
        return 0;
    }
    return s_calBlock.probes[probeIndex].status;
}

/**
 * @brief Save calibration data to FRAM
 */
bool moistureCal_save(void) {
    if (!s_initialized) {
        return false;
    }
    return saveToFram();
}
