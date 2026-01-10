/**
 * @file can_bus.cpp
 * @brief CAN bus communication module implementation
 */

#include "can_bus.h"

// MCP2515 instance
static MCP2515 mcp2515(PIN_CAN_CS);

// Actuator status array
static ActuatorStatus actuators[MAX_ACTUATORS];

// Interrupt flag
static volatile bool canInterruptFlag = false;

// ISR for CAN interrupt
static void canISR(void) {
    canInterruptFlag = true;
}

bool canbus_init(void) {
    DEBUG_PRINTLN("CAN: Initializing...");
    
    // Initialize actuator array
    for (int i = 0; i < MAX_ACTUATORS; i++) {
        actuators[i].address = i + 1;
        actuators[i].status_flags = 0;
        actuators[i].current_ma = 0;
        actuators[i].last_seen = 0;
        actuators[i].online = false;
        memset(actuators[i].uid, 0, 8);
        actuators[i].uid_known = false;
    }
    
    // Reset MCP2515
    mcp2515.reset();
    
    // Set bitrate (1 Mbps with 16MHz crystal)
    if (mcp2515.setBitrate(CAN_SPEED, CAN_CLOCK) != MCP2515::ERROR_OK) {
        DEBUG_PRINTLN("CAN: Failed to set bitrate");
        return false;
    }
    
    // Set normal mode
    if (mcp2515.setNormalMode() != MCP2515::ERROR_OK) {
        DEBUG_PRINTLN("CAN: Failed to set normal mode");
        return false;
    }
    
    // Attach interrupt
    pinMode(PIN_CAN_INT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_CAN_INT), canISR, FALLING);
    
    DEBUG_PRINTLN("CAN: Initialized at 1 Mbps");
    return true;
}

bool canbus_open_valve(uint8_t address) {
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) {
        return false;
    }
    
    struct can_frame frame;
    frame.can_id = CAN_ID_VALVE_OPEN;
    frame.can_dlc = 1;
    frame.data[0] = address;
    
    DEBUG_PRINTF("CAN: Open valve %d\n", address);
    return (mcp2515.sendMessage(&frame) == MCP2515::ERROR_OK);
}

bool canbus_close_valve(uint8_t address) {
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) {
        return false;
    }
    
    struct can_frame frame;
    frame.can_id = CAN_ID_VALVE_CLOSE;
    frame.can_dlc = 1;
    frame.data[0] = address;
    
    DEBUG_PRINTF("CAN: Close valve %d\n", address);
    return (mcp2515.sendMessage(&frame) == MCP2515::ERROR_OK);
}

bool canbus_stop_valve(uint8_t address) {
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) {
        return false;
    }
    
    struct can_frame frame;
    frame.can_id = CAN_ID_VALVE_STOP;
    frame.can_dlc = 1;
    frame.data[0] = address;
    
    DEBUG_PRINTF("CAN: Stop valve %d\n", address);
    return (mcp2515.sendMessage(&frame) == MCP2515::ERROR_OK);
}

bool canbus_emergency_close_all(void) {
    struct can_frame frame;
    frame.can_id = CAN_ID_EMERGENCY_CLOSE;
    frame.can_dlc = 0;
    
    DEBUG_PRINTLN("CAN: EMERGENCY CLOSE ALL");
    
    // Send multiple times for reliability
    bool success = true;
    for (int i = 0; i < 3; i++) {
        if (mcp2515.sendMessage(&frame) != MCP2515::ERROR_OK) {
            success = false;
        }
        delay(5);
    }
    
    return success;
}

bool canbus_query_status(uint8_t address) {
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) {
        return false;
    }
    
    struct can_frame frame;
    frame.can_id = CAN_ID_VALVE_QUERY;
    frame.can_dlc = 1;
    frame.data[0] = address;
    
    return (mcp2515.sendMessage(&frame) == MCP2515::ERROR_OK);
}

bool canbus_query_all(void) {
    bool success = true;
    
    for (int i = 0; i < MAX_ACTUATORS; i++) {
        if (actuators[i].online) {
            if (!canbus_query_status(i + 1)) {
                success = false;
            }
            delay(2);  // Small delay between queries
        }
    }
    
    return success;
}

void canbus_process(void) {
    struct can_frame frame;
    
    while (mcp2515.readMessage(&frame) == MCP2515::ERROR_OK) {
        // Check if this is a status response
        if (frame.can_id >= CAN_ID_STATUS_BASE && 
            frame.can_id < CAN_ID_STATUS_BASE + MAX_ACTUATORS) {
            
            uint8_t addr = frame.can_id - CAN_ID_STATUS_BASE;
            
            if (addr >= ACTUATOR_ADDR_MIN && addr <= ACTUATOR_ADDR_MAX) {
                uint8_t idx = addr - 1;
                
                actuators[idx].status_flags = frame.data[0];
                actuators[idx].current_ma = (frame.data[1] << 8) | frame.data[2];
                actuators[idx].last_seen = millis();
                actuators[idx].online = true;
                
                DEBUG_PRINTF("CAN: Actuator %d status=0x%02X current=%dmA\n",
                            addr, frame.data[0], actuators[idx].current_ma);
            }
        }
        // Check if this is a UID response
        else if (frame.can_id >= CAN_ID_UID_RESPONSE_BASE && 
                 frame.can_id < CAN_ID_UID_RESPONSE_BASE + MAX_ACTUATORS) {
            
            uint8_t addr = frame.can_id - CAN_ID_UID_RESPONSE_BASE;
            
            if (addr >= ACTUATOR_ADDR_MIN && addr <= ACTUATOR_ADDR_MAX && frame.can_dlc == 8) {
                uint8_t idx = addr - 1;
                
                memcpy(actuators[idx].uid, frame.data, 8);
                actuators[idx].uid_known = true;
                actuators[idx].last_seen = millis();
                actuators[idx].online = true;
                
                DEBUG_PRINTF("CAN: Actuator %d UID=%02X%02X%02X%02X%02X%02X%02X%02X\n",
                            addr,
                            frame.data[0], frame.data[1], frame.data[2], frame.data[3],
                            frame.data[4], frame.data[5], frame.data[6], frame.data[7]);
            }
        }
    }
    
    canInterruptFlag = false;
}

ActuatorStatus* canbus_get_actuator(uint8_t address) {
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) {
        return NULL;
    }
    return &actuators[address - 1];
}

uint8_t canbus_get_online_count(void) {
    uint8_t count = 0;
    for (int i = 0; i < MAX_ACTUATORS; i++) {
        if (actuators[i].online) {
            count++;
        }
    }
    return count;
}

bool canbus_is_actuator_online(uint8_t address) {
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) {
        return false;
    }
    return actuators[address - 1].online;
}

uint8_t canbus_get_valve_state(uint8_t address) {
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) {
        return 0;  // Unknown/error state
    }
    return actuators[address - 1].status_flags;
}

uint16_t canbus_get_motor_current(uint8_t address) {
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) {
        return 0;
    }
    return actuators[address - 1].current_ma;
}

bool canbus_has_message(void) {
    return canInterruptFlag;
}

/* ==========================================================================
 * UID DISCOVERY AND LOOKUP
 * ========================================================================== */

bool canbus_discover_all(void) {
    DEBUG_PRINTLN("CAN: Sending discovery broadcast...");
    
    struct can_frame frame;
    frame.can_id = CAN_ID_DISCOVER_ALL;
    frame.can_dlc = 0;
    
    return (mcp2515.sendMessage(&frame) == MCP2515::ERROR_OK);
}

bool canbus_query_uid(uint8_t address) {
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) {
        return false;
    }
    
    struct can_frame frame;
    frame.can_id = CAN_ID_UID_QUERY;
    frame.can_dlc = 1;
    frame.data[0] = address;
    
    DEBUG_PRINTF("CAN: Query UID for address %d\n", address);
    return (mcp2515.sendMessage(&frame) == MCP2515::ERROR_OK);
}

bool canbus_uid_equals(const ActuatorUID a, const ActuatorUID b) {
    return memcmp(a, b, 8) == 0;
}

uint8_t canbus_lookup_address_by_uid(const ActuatorUID uid) {
    for (int i = 0; i < MAX_ACTUATORS; i++) {
        if (actuators[i].uid_known && canbus_uid_equals(actuators[i].uid, uid)) {
            return actuators[i].address;
        }
    }
    return 0;  // Not found
}

ActuatorStatus* canbus_get_actuator_by_uid(const ActuatorUID uid) {
    uint8_t addr = canbus_lookup_address_by_uid(uid);
    if (addr == 0) {
        return NULL;
    }
    return &actuators[addr - 1];
}

/* ==========================================================================
 * UID-BASED VALVE COMMANDS
 * ========================================================================== */

bool canbus_open_valve_by_uid(const ActuatorUID uid) {
    uint8_t addr = canbus_lookup_address_by_uid(uid);
    if (addr == 0) {
        DEBUG_PRINTLN("CAN: UID not found for open command");
        return false;
    }
    return canbus_open_valve(addr);
}

bool canbus_close_valve_by_uid(const ActuatorUID uid) {
    uint8_t addr = canbus_lookup_address_by_uid(uid);
    if (addr == 0) {
        DEBUG_PRINTLN("CAN: UID not found for close command");
        return false;
    }
    return canbus_close_valve(addr);
}

bool canbus_stop_valve_by_uid(const ActuatorUID uid) {
    uint8_t addr = canbus_lookup_address_by_uid(uid);
    if (addr == 0) {
        DEBUG_PRINTLN("CAN: UID not found for stop command");
        return false;
    }
    return canbus_stop_valve(addr);
}
