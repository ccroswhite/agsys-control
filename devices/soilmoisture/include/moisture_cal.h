/**
 * @file moisture_cal.h
 * @brief Moisture Probe Calibration Storage Interface
 * 
 * Stores per-probe calibration data in FRAM:
 * - f_air: frequency in air (factory/hardware calibration)
 * - f_dry: frequency in dry soil (field calibration)
 * - f_wet: frequency in wet soil (field calibration)
 * 
 * Calibration data is stored in the protected region of FRAM
 * and survives firmware updates.
 */

#ifndef MOISTURE_CAL_H
#define MOISTURE_CAL_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

// Calibration magic value
#define MOISTURE_CAL_MAGIC      0x4D434C42  // "MCLB" - Moisture CaLiBration

// Calibration version (increment when structure changes)
#define MOISTURE_CAL_VERSION    1

// Calibration status flags
#define CAL_STATUS_F_AIR_SET    0x01    // f_air has been calibrated
#define CAL_STATUS_F_DRY_SET    0x02    // f_dry has been calibrated
#define CAL_STATUS_F_WET_SET    0x04    // f_wet has been calibrated
#define CAL_STATUS_COMPLETE     0x07    // All calibration complete

// Per-probe calibration data (32 bytes each)
typedef struct {
    uint32_t f_air;         // Frequency in air (Hz) - factory calibration
    uint32_t f_dry;         // Frequency in dry soil (Hz) - field calibration
    uint32_t f_wet;         // Frequency in wet soil (Hz) - field calibration
    int8_t   cal_temp;      // Temperature at calibration (°C)
    uint8_t  status;        // Calibration status flags
    uint8_t  reserved[2];   // Reserved for future use
    uint32_t cal_timestamp; // Unix timestamp of last calibration
    uint8_t  padding[12];   // Pad to 32 bytes
} __attribute__((packed)) MoistureCalibration;

// Calibration block header (stored in FRAM)
typedef struct {
    uint32_t magic;                             // MOISTURE_CAL_MAGIC
    uint8_t  version;                           // MOISTURE_CAL_VERSION
    uint8_t  numProbes;                         // Number of probes configured
    uint8_t  reserved[2];                       // Reserved
    MoistureCalibration probes[MAX_PROBES];     // Per-probe calibration (4 × 32 = 128 bytes)
    uint32_t crc;                               // CRC32 of entire block
} __attribute__((packed)) MoistureCalBlock;

// Total size: 4 + 1 + 1 + 2 + 128 + 4 = 140 bytes

/**
 * @brief Initialize moisture calibration system
 * 
 * Loads calibration data from FRAM. If no valid data exists,
 * initializes with defaults.
 * 
 * @return true if calibration data loaded successfully
 */
bool moistureCal_init(void);

/**
 * @brief Check if calibration data exists and is valid
 * @return true if valid calibration data is present
 */
bool moistureCal_isValid(void);

/**
 * @brief Check if a specific probe needs f_air calibration
 * 
 * @param probeIndex Probe number (0-3)
 * @return true if f_air is not set (needs first-boot calibration)
 */
bool moistureCal_needsAirCal(uint8_t probeIndex);

/**
 * @brief Check if a specific probe has complete field calibration
 * 
 * @param probeIndex Probe number (0-3)
 * @return true if both f_dry and f_wet are set
 */
bool moistureCal_isFieldCalComplete(uint8_t probeIndex);

/**
 * @brief Get calibration data for a probe
 * 
 * @param probeIndex Probe number (0-3)
 * @param cal Output: calibration data
 * @return true if successful
 */
bool moistureCal_get(uint8_t probeIndex, MoistureCalibration* cal);

/**
 * @brief Set f_air calibration for a probe
 * 
 * @param probeIndex Probe number (0-3)
 * @param f_air Frequency in air (Hz)
 * @return true if successful
 */
bool moistureCal_setAir(uint8_t probeIndex, uint32_t f_air);

/**
 * @brief Set f_dry calibration for a probe
 * 
 * @param probeIndex Probe number (0-3)
 * @param f_dry Frequency in dry soil (Hz)
 * @return true if successful
 */
bool moistureCal_setDry(uint8_t probeIndex, uint32_t f_dry);

/**
 * @brief Set f_wet calibration for a probe
 * 
 * @param probeIndex Probe number (0-3)
 * @param f_wet Frequency in wet soil (Hz)
 * @return true if successful
 */
bool moistureCal_setWet(uint8_t probeIndex, uint32_t f_wet);

/**
 * @brief Set all calibration values for a probe at once
 * 
 * Used for transferring calibration from another sensor.
 * 
 * @param probeIndex Probe number (0-3)
 * @param f_air Frequency in air (Hz), or 0 to keep existing
 * @param f_dry Frequency in dry soil (Hz), or 0 to keep existing
 * @param f_wet Frequency in wet soil (Hz), or 0 to keep existing
 * @return true if successful
 */
bool moistureCal_setAll(uint8_t probeIndex, uint32_t f_air, uint32_t f_dry, uint32_t f_wet);

/**
 * @brief Clear calibration for a probe
 * 
 * @param probeIndex Probe number (0-3)
 * @return true if successful
 */
bool moistureCal_clear(uint8_t probeIndex);

/**
 * @brief Clear all calibration data
 * @return true if successful
 */
bool moistureCal_clearAll(void);

/**
 * @brief Get calibration status flags for a probe
 * 
 * @param probeIndex Probe number (0-3)
 * @return Status flags (CAL_STATUS_*)
 */
uint8_t moistureCal_getStatus(uint8_t probeIndex);

/**
 * @brief Save calibration data to FRAM
 * 
 * Called automatically by set functions, but can be called
 * manually to ensure data is persisted.
 * 
 * @return true if successful
 */
bool moistureCal_save(void);

#endif // MOISTURE_CAL_H
