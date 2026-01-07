/**
 * @file valve.cpp
 * @brief Valve state machine implementation
 */

#include "valve.h"
#include "hbridge.h"

static ValveState currentState = VALVE_STATE_IDLE;
static uint8_t statusFlags = STATUS_FLAG_ONLINE;
static uint32_t operationStartTime = 0;
static uint32_t lastCurrentSample = 0;
static uint16_t lastCurrentMA = 0;

void valve_init(void) {
    // Initialize H-Bridge
    hbridge_init();
    
    // Configure limit switch pins
    pinMode(PIN_LIMIT_OPEN, INPUT_PULLUP);
    pinMode(PIN_LIMIT_CLOSED, INPUT_PULLUP);
    
    // Determine initial state from limit switches
    if (valve_is_open()) {
        currentState = VALVE_STATE_OPEN;
        statusFlags |= STATUS_FLAG_OPEN;
        DEBUG_PRINTLN("Valve: Initial state OPEN");
    } else if (valve_is_closed()) {
        currentState = VALVE_STATE_CLOSED;
        statusFlags |= STATUS_FLAG_CLOSED;
        DEBUG_PRINTLN("Valve: Initial state CLOSED");
    } else {
        currentState = VALVE_STATE_IDLE;
        DEBUG_PRINTLN("Valve: Initial state UNKNOWN");
    }
    
    DEBUG_PRINTLN("Valve: Initialized");
}

void valve_open(void) {
    if (valve_is_open()) {
        DEBUG_PRINTLN("Valve: Already open");
        return;
    }
    
    currentState = VALVE_STATE_OPENING;
    statusFlags &= ~(STATUS_FLAG_OPEN | STATUS_FLAG_CLOSED | STATUS_FLAG_FAULT | 
                     STATUS_FLAG_OVERCURRENT | STATUS_FLAG_TIMEOUT | STATUS_FLAG_STALL);
    statusFlags |= STATUS_FLAG_MOVING;
    operationStartTime = millis();
    
    hbridge_open();
    DEBUG_PRINTLN("Valve: Opening...");
}

void valve_close(void) {
    if (valve_is_closed()) {
        DEBUG_PRINTLN("Valve: Already closed");
        return;
    }
    
    currentState = VALVE_STATE_CLOSING;
    statusFlags &= ~(STATUS_FLAG_OPEN | STATUS_FLAG_CLOSED | STATUS_FLAG_FAULT |
                     STATUS_FLAG_OVERCURRENT | STATUS_FLAG_TIMEOUT | STATUS_FLAG_STALL);
    statusFlags |= STATUS_FLAG_MOVING;
    operationStartTime = millis();
    
    hbridge_close();
    DEBUG_PRINTLN("Valve: Closing...");
}

void valve_stop(void) {
    hbridge_stop();
    statusFlags &= ~STATUS_FLAG_MOVING;
    
    // Update position flags
    if (valve_is_open()) {
        currentState = VALVE_STATE_OPEN;
        statusFlags |= STATUS_FLAG_OPEN;
    } else if (valve_is_closed()) {
        currentState = VALVE_STATE_CLOSED;
        statusFlags |= STATUS_FLAG_CLOSED;
    } else {
        currentState = VALVE_STATE_IDLE;
    }
    
    DEBUG_PRINTLN("Valve: Stopped");
}

void valve_emergency_close(void) {
    DEBUG_PRINTLN("Valve: EMERGENCY CLOSE");
    
    currentState = VALVE_STATE_CLOSING;
    statusFlags &= ~(STATUS_FLAG_OPEN | STATUS_FLAG_FAULT);
    statusFlags |= STATUS_FLAG_MOVING;
    operationStartTime = millis();
    
    hbridge_close();
}

void valve_update(void) {
    uint32_t now = millis();
    
    // Sample current periodically during motor operation
    if (currentState == VALVE_STATE_OPENING || currentState == VALVE_STATE_CLOSING) {
        if (now - lastCurrentSample >= CURRENT_SAMPLE_INTERVAL_MS) {
            lastCurrentMA = hbridge_read_current_ma();
            lastCurrentSample = now;
            
            // Check for overcurrent
            if (hbridge_is_overcurrent()) {
                DEBUG_PRINTF("Valve: OVERCURRENT %d mA\n", lastCurrentMA);
                hbridge_stop();
                currentState = VALVE_STATE_FAULT;
                statusFlags |= STATUS_FLAG_FAULT | STATUS_FLAG_OVERCURRENT;
                statusFlags &= ~STATUS_FLAG_MOVING;
                return;
            }
        }
    }
    
    // State machine
    switch (currentState) {
        case VALVE_STATE_OPENING:
            if (valve_is_open()) {
                hbridge_stop();
                currentState = VALVE_STATE_OPEN;
                statusFlags &= ~STATUS_FLAG_MOVING;
                statusFlags |= STATUS_FLAG_OPEN;
                DEBUG_PRINTLN("Valve: OPEN");
            } else if (now - operationStartTime > VALVE_OPERATION_TIMEOUT_MS) {
                hbridge_stop();
                currentState = VALVE_STATE_FAULT;
                statusFlags |= STATUS_FLAG_FAULT | STATUS_FLAG_TIMEOUT;
                statusFlags &= ~STATUS_FLAG_MOVING;
                DEBUG_PRINTLN("Valve: TIMEOUT opening");
            }
            break;
            
        case VALVE_STATE_CLOSING:
            if (valve_is_closed()) {
                hbridge_stop();
                currentState = VALVE_STATE_CLOSED;
                statusFlags &= ~STATUS_FLAG_MOVING;
                statusFlags |= STATUS_FLAG_CLOSED;
                DEBUG_PRINTLN("Valve: CLOSED");
            } else if (now - operationStartTime > VALVE_OPERATION_TIMEOUT_MS) {
                hbridge_stop();
                currentState = VALVE_STATE_FAULT;
                statusFlags |= STATUS_FLAG_FAULT | STATUS_FLAG_TIMEOUT;
                statusFlags &= ~STATUS_FLAG_MOVING;
                DEBUG_PRINTLN("Valve: TIMEOUT closing");
            }
            break;
            
        case VALVE_STATE_OPEN:
        case VALVE_STATE_CLOSED:
        case VALVE_STATE_IDLE:
        case VALVE_STATE_FAULT:
            // No action needed
            break;
    }
}

ValveState valve_get_state(void) {
    return currentState;
}

uint8_t valve_get_status_flags(void) {
    return statusFlags;
}

uint16_t valve_get_current_ma(void) {
    return lastCurrentMA;
}

bool valve_is_open(void) {
    // Limit switch is active LOW (pressed = LOW)
    return (digitalRead(PIN_LIMIT_OPEN) == LOW);
}

bool valve_is_closed(void) {
    // Limit switch is active LOW (pressed = LOW)
    return (digitalRead(PIN_LIMIT_CLOSED) == LOW);
}

void valve_clear_fault(void) {
    if (currentState == VALVE_STATE_FAULT) {
        statusFlags &= ~(STATUS_FLAG_FAULT | STATUS_FLAG_OVERCURRENT | 
                        STATUS_FLAG_TIMEOUT | STATUS_FLAG_STALL);
        
        // Determine current position
        if (valve_is_open()) {
            currentState = VALVE_STATE_OPEN;
            statusFlags |= STATUS_FLAG_OPEN;
        } else if (valve_is_closed()) {
            currentState = VALVE_STATE_CLOSED;
            statusFlags |= STATUS_FLAG_CLOSED;
        } else {
            currentState = VALVE_STATE_IDLE;
        }
        
        DEBUG_PRINTLN("Valve: Fault cleared");
    }
}
