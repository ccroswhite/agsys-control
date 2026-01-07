/**
 * @file config.h
 * @brief Configuration for Valve Actuator
 * 
 * Hardware: Nordic nRF52810 + MCP2515 CAN + Discrete H-Bridge
 * 
 * Each actuator controls a single motorized ball valve via H-bridge,
 * communicates with the valve controller via CAN bus.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/* ==========================================================================
 * DEVICE IDENTIFICATION
 * ========================================================================== */
#define DEVICE_TYPE                 0x03        // Valve Actuator
#define FIRMWARE_VERSION_MAJOR      1
#define FIRMWARE_VERSION_MINOR      0
#define FIRMWARE_VERSION_PATCH      0

/* ==========================================================================
 * PIN ASSIGNMENTS - nRF52810
 * ========================================================================== */

// SPI Bus (for MCP2515)
#define PIN_SPI_SCK                 (14)        // P0.14
#define PIN_SPI_MISO                (13)        // P0.13
#define PIN_SPI_MOSI                (12)        // P0.12

// CAN Bus (MCP2515)
#define PIN_CAN_CS                  (11)        // P0.11
#define PIN_CAN_INT                 (8)         // P0.08 - Interrupt

// H-Bridge Control
#define PIN_HBRIDGE_A               (3)         // P0.03 - High-side A (open direction)
#define PIN_HBRIDGE_B               (4)         // P0.04 - High-side B (close direction)
#define PIN_HBRIDGE_EN_A            (5)         // P0.05 - Low-side A enable
#define PIN_HBRIDGE_EN_B            (6)         // P0.06 - Low-side B enable

// Current Sensing (ADC)
#define PIN_CURRENT_SENSE           (2)         // P0.02/AIN0 - Voltage across shunt

// Valve Position Limit Switches (active LOW)
#define PIN_LIMIT_OPEN              (9)         // P0.09 - Valve fully open
#define PIN_LIMIT_CLOSED            (10)        // P0.10 - Valve fully closed

// DIP Switch Bank (10-position: 1-6 address, 10 termination)
// Switches 7-9 reserved for future use
#define PIN_DIP_1                   (15)        // P0.15 - Address bit 0
#define PIN_DIP_2                   (16)        // P0.16 - Address bit 1
#define PIN_DIP_3                   (17)        // P0.17 - Address bit 2
#define PIN_DIP_4                   (18)        // P0.18 - Address bit 3
#define PIN_DIP_5                   (19)        // P0.19 - Address bit 4
#define PIN_DIP_6                   (20)        // P0.20 - Address bit 5
#define PIN_DIP_7                   (21)        // P0.21 - Reserved
#define PIN_DIP_8                   (22)        // P0.22 - Reserved
#define PIN_DIP_9                   (23)        // P0.23 - Reserved
#define PIN_DIP_10                  (24)        // P0.24 - CAN Termination enable

// Status LEDs
#define PIN_LED_3V3                 (25)        // P0.25 - Green (power)
#define PIN_LED_24V                 (26)        // P0.26 - Yellow (24V present)
#define PIN_LED_STATUS              (27)        // P0.27 - Red (error/status)
#define PIN_LED_VALVE_OPEN          (28)        // P0.28 - Blue (valve open)

// 24V Sense (for LED, optional ADC)
#define PIN_24V_SENSE               (29)        // P0.29/AIN5 - Voltage divider from 24V

/* ==========================================================================
 * CAN BUS CONFIGURATION
 * ========================================================================== */
#define CAN_SPEED                   CAN_1000KBPS // 1 Mbps (must match controller)
#define CAN_CLOCK                   MCP_16MHZ   // MCP2515 crystal

// CAN Message IDs (must match valve controller)
#define CAN_ID_VALVE_OPEN           0x100       // Controller -> Actuator: Open valve
#define CAN_ID_VALVE_CLOSE          0x101       // Controller -> Actuator: Close valve
#define CAN_ID_VALVE_STOP           0x102       // Controller -> Actuator: Stop motor
#define CAN_ID_VALVE_QUERY          0x103       // Controller -> Actuator: Query status
#define CAN_ID_EMERGENCY_CLOSE      0x1FF       // Controller -> All: Emergency close
#define CAN_ID_STATUS_BASE          0x200       // Actuator -> Controller: Status

/* ==========================================================================
 * H-BRIDGE CONFIGURATION
 * ========================================================================== */
#define MOTOR_PWM_FREQUENCY         1000        // 1 kHz PWM
#define MOTOR_STARTUP_DUTY          255         // Full power on startup
#define MOTOR_RUN_DUTY              200         // Reduced power after moving

// Current sensing
#define CURRENT_SENSE_RESISTOR      0.1f        // 0.1 ohm shunt
#define CURRENT_OVERCURRENT_MA      3000        // 3A overcurrent threshold
#define CURRENT_STALL_MA            2500        // Stall detection threshold
#define CURRENT_SAMPLE_INTERVAL_MS  10          // Sample every 10ms

// Timing
#define VALVE_OPERATION_TIMEOUT_MS  30000       // 30 seconds max to open/close
#define VALVE_DEBOUNCE_MS           50          // Limit switch debounce

/* ==========================================================================
 * STATUS FLAGS (sent in CAN status response)
 * ========================================================================== */
#define STATUS_FLAG_ONLINE          0x01        // Device is operational
#define STATUS_FLAG_OPEN            0x02        // Valve is fully open
#define STATUS_FLAG_CLOSED          0x04        // Valve is fully closed
#define STATUS_FLAG_MOVING          0x08        // Valve is in motion
#define STATUS_FLAG_FAULT           0x10        // General fault
#define STATUS_FLAG_OVERCURRENT     0x20        // Overcurrent detected
#define STATUS_FLAG_TIMEOUT         0x40        // Operation timed out
#define STATUS_FLAG_STALL           0x80        // Motor stall detected

/* ==========================================================================
 * DEBUG CONFIGURATION
 * ========================================================================== */
#if defined(RELEASE_BUILD)
    #define DEBUG_MODE              0
#else
    #define DEBUG_MODE              1
#endif

#if DEBUG_MODE
    #define DEBUG_PRINT(x)          Serial.print(x)
    #define DEBUG_PRINTLN(x)        Serial.println(x)
    #define DEBUG_PRINTF(...)       Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif

#endif // CONFIG_H
