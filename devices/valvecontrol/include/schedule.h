/**
 * @file schedule.h
 * @brief Schedule data structures for valve controller
 */

#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <Arduino.h>

/* ==========================================================================
 * SCHEDULE ENTRY STRUCTURE
 * ========================================================================== */

// Schedule flags
#define SCHED_FLAG_ENABLED          0x01    // Schedule is active
#define SCHED_FLAG_SKIP_IF_WET      0x02    // Skip if soil moisture above threshold
#define SCHED_FLAG_VOLUME_BASED     0x04    // Stop when target volume reached
#define SCHED_FLAG_REPEAT_WEEKLY    0x08    // Repeat every week

// Days of week bitmask
#define DAY_SUNDAY                  0x01
#define DAY_MONDAY                  0x02
#define DAY_TUESDAY                 0x04
#define DAY_WEDNESDAY               0x08
#define DAY_THURSDAY                0x10
#define DAY_FRIDAY                  0x20
#define DAY_SATURDAY                0x40
#define DAY_EVERYDAY                0x7F

/**
 * @brief Schedule entry stored in FRAM
 * 
 * Size: 12 bytes per entry
 * Max entries: 256 (3KB total)
 */
typedef struct __attribute__((packed)) {
    uint8_t valve_id;           // Valve/actuator address (1-64)
    uint8_t days_of_week;       // Bitmask: bit0=Sun, bit1=Mon, ... bit6=Sat
    uint16_t start_time_min;    // Minutes from midnight (0-1439)
    uint16_t duration_min;      // Duration in minutes
    uint16_t target_gallons;    // Target volume (0 = time-based only)
    uint8_t priority;           // For conflict resolution (higher = more important)
    uint8_t flags;              // SCHED_FLAG_* bitmask
    uint8_t flow_group;         // Zones sharing a supply line (0-15)
    uint8_t max_concurrent;     // Max zones in this group running at once
} ScheduleEntry;

/**
 * @brief Schedule header stored at start of schedule area
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;             // 0x5343484E ("SCHN")
    uint16_t version;           // Schedule format version
    uint16_t entry_count;       // Number of valid entries
    uint32_t last_sync;         // Unix timestamp of last sync with property controller
    uint32_t checksum;          // CRC32 of all entries
} ScheduleHeader;

#define SCHEDULE_MAGIC              0x5343484E
#define SCHEDULE_VERSION            1

/* ==========================================================================
 * IRRIGATION EVENT LOG
 * ========================================================================== */

// Event types
#define EVENT_VALVE_OPENED          0x01
#define EVENT_VALVE_CLOSED          0x02
#define EVENT_SCHEDULE_RUN          0x03
#define EVENT_SCHEDULE_SKIPPED      0x04
#define EVENT_MANUAL_OVERRIDE       0x05
#define EVENT_EMERGENCY_CLOSE       0x06
#define EVENT_POWER_FAIL            0x07
#define EVENT_POWER_RESTORE         0x08
#define EVENT_ACTUATOR_FAULT        0x09
#define EVENT_COMM_TIMEOUT          0x0A

/**
 * @brief Event log entry
 * 
 * Size: 12 bytes per entry
 */
typedef struct __attribute__((packed)) {
    uint32_t timestamp;         // Unix timestamp
    uint8_t event_type;         // EVENT_* type
    uint8_t valve_id;           // Valve involved (0 for system events)
    uint16_t duration_sec;      // Duration (for valve events)
    uint16_t volume_gallons;    // Volume used (if available)
    uint8_t flags;              // Additional flags
    uint8_t reserved;           // Padding
} EventLogEntry;

/* ==========================================================================
 * ACTUATOR STATE CACHE
 * ========================================================================== */

// Actuator status flags
#define ACTUATOR_FLAG_ONLINE        0x01    // Responding to CAN
#define ACTUATOR_FLAG_OPEN          0x02    // Valve is open
#define ACTUATOR_FLAG_CLOSED        0x04    // Valve is closed
#define ACTUATOR_FLAG_MOVING        0x08    // Valve is in motion
#define ACTUATOR_FLAG_FAULT         0x10    // Fault detected
#define ACTUATOR_FLAG_OVERCURRENT   0x20    // Overcurrent detected

/**
 * @brief Cached state for each actuator
 * 
 * Size: 8 bytes per actuator × 64 = 512 bytes
 */
typedef struct __attribute__((packed)) {
    uint8_t address;            // CAN address (1-64)
    uint8_t status_flags;       // ACTUATOR_FLAG_* bitmask
    uint16_t last_current_ma;   // Last measured motor current
    uint32_t last_seen;         // Unix timestamp of last CAN response
} ActuatorState;

#endif // SCHEDULE_H
