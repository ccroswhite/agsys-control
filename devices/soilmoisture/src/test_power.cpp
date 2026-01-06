/**
 * @file test_power.cpp
 * @brief Power profiling test mode
 * 
 * Build with: pio run -e test-power-all
 * 
 * Cycles through three phases for power measurement:
 *   Phase 1: Sensor active (H-bridge, ADC) - 60 seconds
 *   Phase 2: LoRa TX continuous - 60 seconds  
 *   Phase 3: Deep sleep - 60 seconds
 * Repeats forever.
 */

#ifdef TEST_MODE_POWER_ALL

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

#include "config.h"
#include "capacitance.h"
#include "security.h"

#ifndef TEST_PHASE_DURATION_MS
#define TEST_PHASE_DURATION_MS 60000
#endif

// Test phases
enum TestPhase {
    PHASE_SENSOR = 0,
    PHASE_LORA_TX,
    PHASE_SLEEP,
    PHASE_COUNT
};

static const char* PHASE_NAMES[] = {
    "SENSOR (H-bridge + ADC)",
    "LORA TX (continuous)",
    "DEEP SLEEP"
};

static TestPhase currentPhase = PHASE_SENSOR;
static uint32_t phaseStartTime = 0;
static uint32_t cycleCount = 0;

// Forward declarations
void testPowerInit();
void testPowerLoop();
void enterPhase(TestPhase phase);
void runSensorPhase();
void runLoRaTxPhase();
void runSleepPhase();

void testPowerInit() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    
    Serial.println("\n========================================");
    Serial.println("  TEST MODE: Power Profiling");
    Serial.println("========================================");
    Serial.printf("Device ID: %016llX\n", security_getDeviceId());
    Serial.printf("Phase Duration: %d ms (%d seconds)\n", 
                  TEST_PHASE_DURATION_MS, TEST_PHASE_DURATION_MS / 1000);
    Serial.println("========================================");
    Serial.println("Phases:");
    Serial.println("  1. Sensor (H-bridge + ADC active)");
    Serial.println("  2. LoRa TX (continuous transmission)");
    Serial.println("  3. Deep Sleep (minimum power)");
    Serial.println("========================================\n");
    
    // Initialize pins
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_LED_SPI, OUTPUT);
    pinMode(PIN_LED_CONN, OUTPUT);
    digitalWrite(PIN_LED_STATUS, HIGH);  // LEDs off (active low)
    digitalWrite(PIN_LED_SPI, HIGH);
    digitalWrite(PIN_LED_CONN, HIGH);
    
    pinMode(PIN_MOISTURE_POWER, OUTPUT);
    digitalWrite(PIN_MOISTURE_POWER, LOW);
    
    // Initialize ADC
    analogReadResolution(ADC_RESOLUTION_BITS);
    
    // Initialize SPI
    SPI.begin();
    
    // Initialize capacitance measurement
    capacitanceInit();
    
    // Initialize LoRa
    LoRa.setPins(PIN_LORA_CS, PIN_LORA_RST, PIN_LORA_DIO0);
    if (!LoRa.begin(LORA_FREQUENCY)) {
        Serial.println("ERROR: LoRa init failed!");
        while (1) delay(1000);
    }
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    LoRa.setSignalBandwidth(LORA_BANDWIDTH);
    LoRa.setCodingRate4(LORA_CODING_RATE);
    LoRa.setTxPower(LORA_TX_POWER_DBM);
    LoRa.enableCrc();
    
    Serial.println("Initialization complete.\n");
    
    // Start first phase
    enterPhase(PHASE_SENSOR);
}

void testPowerLoop() {
    // Check if phase duration has elapsed
    if (millis() - phaseStartTime >= TEST_PHASE_DURATION_MS) {
        // Move to next phase
        TestPhase nextPhase = (TestPhase)((currentPhase + 1) % PHASE_COUNT);
        
        if (nextPhase == PHASE_SENSOR) {
            cycleCount++;
        }
        
        enterPhase(nextPhase);
    }
    
    // Run current phase
    switch (currentPhase) {
        case PHASE_SENSOR:
            runSensorPhase();
            break;
        case PHASE_LORA_TX:
            runLoRaTxPhase();
            break;
        case PHASE_SLEEP:
            runSleepPhase();
            break;
        default:
            break;
    }
}

void enterPhase(TestPhase phase) {
    currentPhase = phase;
    phaseStartTime = millis();
    
    Serial.println("----------------------------------------");
    Serial.printf("CYCLE %lu - PHASE %d: %s\n", cycleCount + 1, phase + 1, PHASE_NAMES[phase]);
    Serial.printf("Duration: %d seconds\n", TEST_PHASE_DURATION_MS / 1000);
    Serial.println("----------------------------------------");
    
    // LED indication: blink phase number
    for (int i = 0; i <= phase; i++) {
        digitalWrite(PIN_LED_STATUS, LOW);  // On
        delay(100);
        digitalWrite(PIN_LED_STATUS, HIGH); // Off
        delay(100);
    }
    
    // Phase-specific setup
    switch (phase) {
        case PHASE_SENSOR:
            // Power on sensor circuitry
            digitalWrite(PIN_MOISTURE_POWER, HIGH);
            hbridgeStart();
            break;
            
        case PHASE_LORA_TX:
            // Power off sensor, prepare LoRa
            digitalWrite(PIN_MOISTURE_POWER, LOW);
            hbridgeStop();
            break;
            
        case PHASE_SLEEP:
            // Power off everything
            digitalWrite(PIN_MOISTURE_POWER, LOW);
            hbridgeStop();
            LoRa.sleep();
            break;
    }
}

void runSensorPhase() {
    static unsigned long lastReadTime = 0;
    
    // Read sensor every 500ms
    if (millis() - lastReadTime >= 500) {
        lastReadTime = millis();
        
        // Read moisture using capacitance measurement
        uint16_t moistureRaw = readCapacitance();
        
        // Also read battery ADC
        uint16_t batteryRaw = analogRead(PIN_BATTERY_ANALOG);
        
        uint32_t elapsed = (millis() - phaseStartTime) / 1000;
        Serial.printf("[%02lu:%02lu] Moisture: %d, Battery ADC: %d\n",
                      elapsed / 60, elapsed % 60, moistureRaw, batteryRaw);
    }
}

void runLoRaTxPhase() {
    static unsigned long lastTxTime = 0;
    static uint32_t txCount = 0;
    
    // Transmit every 100ms (continuous TX load)
    if (millis() - lastTxTime >= 100) {
        lastTxTime = millis();
        
        // Send a test packet
        uint8_t packet[32];
        memset(packet, 0xAA, sizeof(packet));
        packet[0] = 'T';
        packet[1] = 'E';
        packet[2] = 'S';
        packet[3] = 'T';
        packet[4] = (txCount >> 24) & 0xFF;
        packet[5] = (txCount >> 16) & 0xFF;
        packet[6] = (txCount >> 8) & 0xFF;
        packet[7] = txCount & 0xFF;
        
        LoRa.beginPacket();
        LoRa.write(packet, sizeof(packet));
        LoRa.endPacket();
        
        txCount++;
        
        if (txCount % 50 == 0) {
            uint32_t elapsed = (millis() - phaseStartTime) / 1000;
            Serial.printf("[%02lu:%02lu] TX packets: %lu\n",
                          elapsed / 60, elapsed % 60, txCount);
        }
    }
}

void runSleepPhase() {
    // In this test, we use delay() to simulate sleep
    // Real deep sleep would use sd_power_system_off() or similar
    // but that would stop serial output
    
    static unsigned long lastPrintTime = 0;
    
    if (millis() - lastPrintTime >= 10000) {
        lastPrintTime = millis();
        uint32_t elapsed = (millis() - phaseStartTime) / 1000;
        Serial.printf("[%02lu:%02lu] Sleeping... (use power analyzer to measure)\n",
                      elapsed / 60, elapsed % 60);
    }
    
    // Low power delay
    delay(100);
}

// Arduino entry points
void setup() {
    security_init();
    testPowerInit();
}

void loop() {
    testPowerLoop();
}

#endif // TEST_MODE_POWER_ALL
