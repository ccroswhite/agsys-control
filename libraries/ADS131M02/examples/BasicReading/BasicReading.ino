/*!
 * @file BasicReading.ino
 * 
 * Basic example for the ADS131M02 library
 * Reads both channels and prints values to Serial
 * 
 * Wiring:
 *   ADS131M02    Arduino
 *   ---------    -------
 *   VDD          3.3V
 *   GND          GND
 *   CS           Pin 10
 *   SCLK         Pin 13 (SCK)
 *   DIN          Pin 11 (MOSI)
 *   DOUT         Pin 12 (MISO)
 *   DRDY         Pin 9
 *   SYNC/RST     Pin 8 (optional)
 */

#include <SPI.h>
#include <ADS131M02.h>

// Pin definitions
#define PIN_CS    10
#define PIN_DRDY  9
#define PIN_RESET 8   // Optional, set to -1 if not used

// Create ADC instance
ADS131M02 adc;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("ADS131M02 Basic Reading Example");
    Serial.println("================================");
    
    // Initialize the ADC
    if (!adc.begin(PIN_CS, PIN_DRDY, PIN_RESET)) {
        Serial.println("ERROR: ADS131M02 not found!");
        Serial.println("Check wiring and try again.");
        while (1) delay(100);
    }
    
    // Print device info
    Serial.print("Device ID: 0x");
    Serial.println(adc.readID(), HEX);
    
    // Configure for our application
    adc.setGain(0, ADS131M02_GAIN_1);   // Channel 0: Gain = 1
    adc.setGain(1, ADS131M02_GAIN_1);   // Channel 1: Gain = 1
    adc.setOSR(ADS131M02_OSR_4096);     // 1 kSPS data rate
    
    Serial.println("ADC initialized successfully!");
    Serial.println();
}

void loop() {
    ads131m02_data_t data;
    
    // Wait for data ready
    if (adc.dataReady()) {
        // Read both channels
        if (adc.readData(&data)) {
            // Convert to millivolts
            float mv_ch0 = ADS131M02::toMillivolts(data.ch0, adc.getGain(0));
            float mv_ch1 = ADS131M02::toMillivolts(data.ch1, adc.getGain(1));
            
            // Print results
            Serial.print("CH0: ");
            Serial.print(mv_ch0, 3);
            Serial.print(" mV (raw: ");
            Serial.print(data.ch0);
            Serial.print(")  CH1: ");
            Serial.print(mv_ch1, 3);
            Serial.print(" mV (raw: ");
            Serial.print(data.ch1);
            Serial.println(")");
        }
    }
    
    delay(100);  // ~10 readings per second
}
