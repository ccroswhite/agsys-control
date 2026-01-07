/**
 * @file schedule_manager.cpp
 * @brief Schedule management implementation
 */

#include "schedule_manager.h"
#include "rtc.h"
#include <Adafruit_FRAM_SPI.h>

// FRAM instance (extern from main or create here)
extern Adafruit_FRAM_SPI fram;

// Schedule data in RAM
static ScheduleHeader header;
static ScheduleEntry entries[MAX_SCHEDULE_ENTRIES];
static bool initialized = false;

bool schedule_init(void) {
    DEBUG_PRINTLN("Schedule: Initializing...");
    
    // Clear RAM
    memset(&header, 0, sizeof(header));
    memset(entries, 0, sizeof(entries));
    
    // Load from FRAM
    if (!schedule_load()) {
        DEBUG_PRINTLN("Schedule: No valid data, creating empty schedule");
        header.magic = SCHEDULE_MAGIC;
        header.version = SCHEDULE_VERSION;
        header.entry_count = 0;
        header.last_sync = 0;
        header.checksum = 0;
        schedule_save();
    }
    
    initialized = true;
    DEBUG_PRINTF("Schedule: Loaded %d entries\n", header.entry_count);
    return true;
}

bool schedule_load(void) {
    // Read header from FRAM
    uint8_t* headerPtr = (uint8_t*)&header;
    for (size_t i = 0; i < sizeof(ScheduleHeader); i++) {
        headerPtr[i] = fram.read8(FRAM_ADDR_SCHEDULES + i);
    }
    
    // Validate magic
    if (header.magic != SCHEDULE_MAGIC) {
        DEBUG_PRINTLN("Schedule: Invalid magic number");
        return false;
    }
    
    // Validate version
    if (header.version != SCHEDULE_VERSION) {
        DEBUG_PRINTF("Schedule: Version mismatch (got %d, expected %d)\n", 
                    header.version, SCHEDULE_VERSION);
        return false;
    }
    
    // Validate entry count
    if (header.entry_count > MAX_SCHEDULE_ENTRIES) {
        DEBUG_PRINTLN("Schedule: Entry count exceeds maximum");
        return false;
    }
    
    // Read entries
    uint32_t addr = FRAM_ADDR_SCHEDULES + sizeof(ScheduleHeader);
    for (uint16_t i = 0; i < header.entry_count; i++) {
        uint8_t* entryPtr = (uint8_t*)&entries[i];
        for (size_t j = 0; j < sizeof(ScheduleEntry); j++) {
            entryPtr[j] = fram.read8(addr++);
        }
    }
    
    DEBUG_PRINTLN("Schedule: Loaded from FRAM");
    return true;
}

bool schedule_save(void) {
    // Update checksum (simple sum for now)
    header.checksum = 0;
    uint8_t* ptr = (uint8_t*)entries;
    for (size_t i = 0; i < header.entry_count * sizeof(ScheduleEntry); i++) {
        header.checksum += ptr[i];
    }
    
    // Write header
    uint8_t* headerPtr = (uint8_t*)&header;
    for (size_t i = 0; i < sizeof(ScheduleHeader); i++) {
        fram.writeEnable(true);
        fram.write8(FRAM_ADDR_SCHEDULES + i, headerPtr[i]);
    }
    
    // Write entries
    uint32_t addr = FRAM_ADDR_SCHEDULES + sizeof(ScheduleHeader);
    for (uint16_t i = 0; i < header.entry_count; i++) {
        uint8_t* entryPtr = (uint8_t*)&entries[i];
        for (size_t j = 0; j < sizeof(ScheduleEntry); j++) {
            fram.writeEnable(true);
            fram.write8(addr++, entryPtr[j]);
        }
    }
    
    DEBUG_PRINTLN("Schedule: Saved to FRAM");
    return true;
}

bool schedule_add(ScheduleEntry* entry) {
    if (header.entry_count >= MAX_SCHEDULE_ENTRIES) {
        DEBUG_PRINTLN("Schedule: Maximum entries reached");
        return false;
    }
    
    memcpy(&entries[header.entry_count], entry, sizeof(ScheduleEntry));
    header.entry_count++;
    
    return schedule_save();
}

bool schedule_update(uint16_t index, ScheduleEntry* entry) {
    if (index >= header.entry_count) {
        return false;
    }
    
    memcpy(&entries[index], entry, sizeof(ScheduleEntry));
    return schedule_save();
}

bool schedule_remove(uint16_t index) {
    if (index >= header.entry_count) {
        return false;
    }
    
    // Shift entries down
    for (uint16_t i = index; i < header.entry_count - 1; i++) {
        memcpy(&entries[i], &entries[i + 1], sizeof(ScheduleEntry));
    }
    
    header.entry_count--;
    return schedule_save();
}

bool schedule_clear_all(void) {
    header.entry_count = 0;
    memset(entries, 0, sizeof(entries));
    return schedule_save();
}

uint16_t schedule_get_count(void) {
    return header.entry_count;
}

ScheduleEntry* schedule_get_entry(uint16_t index) {
    if (index >= header.entry_count) {
        return NULL;
    }
    return &entries[index];
}

ScheduleHeader* schedule_get_header(void) {
    return &header;
}

int16_t schedule_check_pending(void) {
    if (!initialized || header.entry_count == 0) {
        return -1;
    }
    
    // Get current time
    uint16_t current_minutes = rtc_get_minutes_from_midnight();
    uint8_t current_dow = rtc_get_day_of_week();
    uint8_t dow_mask = (1 << current_dow);
    
    for (uint16_t i = 0; i < header.entry_count; i++) {
        ScheduleEntry* entry = &entries[i];
        
        // Check if enabled
        if (!(entry->flags & SCHED_FLAG_ENABLED)) {
            continue;
        }
        
        // Check day of week
        if (!(entry->days_of_week & dow_mask)) {
            continue;
        }
        
        // Check if within start window (allow 1 minute tolerance)
        if (current_minutes >= entry->start_time_min && 
            current_minutes < entry->start_time_min + 2) {
            return i;
        }
    }
    
    return -1;
}

void schedule_mark_run(uint16_t index) {
    if (index >= header.entry_count) {
        return;
    }
    
    // Could add last_run timestamp to entry if needed
    DEBUG_PRINTF("Schedule: Marked entry %d as run\n", index);
}

bool schedule_validate(void) {
    if (header.magic != SCHEDULE_MAGIC) {
        return false;
    }
    
    if (header.version != SCHEDULE_VERSION) {
        return false;
    }
    
    if (header.entry_count > MAX_SCHEDULE_ENTRIES) {
        return false;
    }
    
    // Validate each entry
    for (uint16_t i = 0; i < header.entry_count; i++) {
        if (entries[i].valve_id < ACTUATOR_ADDR_MIN || 
            entries[i].valve_id > ACTUATOR_ADDR_MAX) {
            return false;
        }
        if (entries[i].start_time_min >= 1440) {  // 24 * 60
            return false;
        }
    }
    
    return true;
}
