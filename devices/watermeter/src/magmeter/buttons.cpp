/**
 * @file buttons.cpp
 * @brief Button handler implementation for Mag Meter
 * 
 * Handles debouncing and long-press detection for 5 navigation buttons.
 */

#include "buttons.h"
#include "magmeter_config.h"

#define NUM_BUTTONS 5

// Button indices
#define BTN_IDX_UP      0
#define BTN_IDX_DOWN    1
#define BTN_IDX_LEFT    2
#define BTN_IDX_RIGHT   3
#define BTN_IDX_SELECT  4

// Button state tracking
typedef struct {
    uint8_t pin;
    bool lastState;
    bool currentState;
    uint32_t pressStartTime;
    bool longPressTriggered;
} ButtonState_t;

static ButtonState_t buttons[NUM_BUTTONS] = {
    {PIN_BTN_UP, true, true, 0, false},
    {PIN_BTN_DOWN, true, true, 0, false},
    {PIN_BTN_LEFT, true, true, 0, false},
    {PIN_BTN_RIGHT, true, true, 0, false},
    {PIN_BTN_SELECT, true, true, 0, false}
};

static uint32_t lastDebounceTime[NUM_BUTTONS] = {0, 0, 0, 0, 0};

// Short press event mapping
static const ButtonEvent_t shortEvents[NUM_BUTTONS] = {
    BTN_UP_SHORT, BTN_DOWN_SHORT, BTN_LEFT_SHORT, BTN_RIGHT_SHORT, BTN_SELECT_SHORT
};

// Long press event mapping
static const ButtonEvent_t longEvents[NUM_BUTTONS] = {
    BTN_UP_LONG, BTN_DOWN_LONG, BTN_LEFT_LONG, BTN_RIGHT_LONG, BTN_SELECT_LONG
};

void buttons_init(void) {
    pinMode(PIN_BTN_UP, INPUT_PULLUP);
    pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
    pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
    pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
    pinMode(PIN_BTN_SELECT, INPUT_PULLUP);
    
    // Initialize states
    for (int i = 0; i < NUM_BUTTONS; i++) {
        buttons[i].lastState = true;  // Active LOW, so unpressed = HIGH
        buttons[i].currentState = true;
        buttons[i].pressStartTime = 0;
        buttons[i].longPressTriggered = false;
    }
}

ButtonEvent_t buttons_poll(void) {
    uint32_t now = millis();
    ButtonEvent_t event = BTN_NONE;
    
    for (int i = 0; i < NUM_BUTTONS; i++) {
        bool reading = digitalRead(buttons[i].pin);
        
        // Debounce
        if (reading != buttons[i].lastState) {
            lastDebounceTime[i] = now;
        }
        
        if ((now - lastDebounceTime[i]) > BTN_DEBOUNCE_MS) {
            // Reading is stable
            if (reading != buttons[i].currentState) {
                buttons[i].currentState = reading;
                
                if (!reading) {
                    // Button pressed (active LOW)
                    buttons[i].pressStartTime = now;
                    buttons[i].longPressTriggered = false;
                } else {
                    // Button released
                    if (!buttons[i].longPressTriggered) {
                        // Short press - generate event on release
                        event = shortEvents[i];
                    }
                    buttons[i].pressStartTime = 0;
                }
            }
            
            // Check for long press while held
            if (!buttons[i].currentState && !buttons[i].longPressTriggered) {
                if (buttons[i].pressStartTime > 0 && 
                    (now - buttons[i].pressStartTime) >= BTN_LONG_PRESS_MS) {
                    buttons[i].longPressTriggered = true;
                    event = longEvents[i];
                }
            }
        }
        
        buttons[i].lastState = reading;
    }
    
    return event;
}

bool buttons_anyPressed(void) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (!buttons[i].currentState) return true;
    }
    return false;
}

bool buttons_isSelectHeld(void) {
    return !buttons[BTN_IDX_SELECT].currentState;
}

uint32_t buttons_getSelectHoldTime(void) {
    if (!buttons[BTN_IDX_SELECT].currentState && buttons[BTN_IDX_SELECT].pressStartTime > 0) {
        return millis() - buttons[BTN_IDX_SELECT].pressStartTime;
    }
    return 0;
}

// Track UP+DOWN combo for BLE pairing
static uint32_t upDownComboStartTime = 0;
static bool upDownComboTriggered = false;

bool buttons_checkPairingCombo(void) {
    bool upPressed = !buttons[BTN_IDX_UP].currentState;
    bool downPressed = !buttons[BTN_IDX_DOWN].currentState;
    
    if (upPressed && downPressed) {
        // Both buttons held
        if (upDownComboStartTime == 0) {
            upDownComboStartTime = millis();
        }
        
        if (!upDownComboTriggered && 
            (millis() - upDownComboStartTime) >= BLE_PAIRING_COMBO_MS) {
            upDownComboTriggered = true;
            return true;  // Combo triggered
        }
    } else {
        // Reset when either button released
        upDownComboStartTime = 0;
        upDownComboTriggered = false;
    }
    
    return false;
}

void buttons_resetPairingCombo(void) {
    upDownComboStartTime = 0;
    upDownComboTriggered = false;
}
