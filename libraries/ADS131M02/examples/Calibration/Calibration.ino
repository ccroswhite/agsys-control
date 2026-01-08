/*!
 * @file Calibration.ino
 * 
 * Calibration example for the ADS131M02 library
 * Demonstrates offset and gain calibration
 * 
 * To calibrate:
 * 1. Short the inputs for offset calibration
 * 2. Apply a known voltage for gain calibration
 */

#include <SPI.h>
#include <ADS131M02.h>

#define PIN_CS    10
#define PIN_DRDY  9
#define PIN_RESET 8

ADS131M02 adc;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("ADS131M02 Calibration Example");
    Serial.println("==============================");
    
    if (!adc.begin(PIN_CS, PIN_DRDY, PIN_RESET)) {
        Serial.println("ERROR: ADS131M02 not found!");
        while (1) delay(100);
    }
    
    // Set gain for calibration
    adc.setGain(0, ADS131M02_GAIN_1);
    adc.setGain(1, ADS131M02_GAIN_1);
    
    Serial.println("ADC initialized.");
    Serial.println();
    Serial.println("Commands:");
    Serial.println("  o - Calibrate offset (short inputs first)");
    Serial.println("  g - Calibrate gain (apply known voltage first)");
    Serial.println("  r - Read current values");
    Serial.println("  s - Show calibration values");
    Serial.println();
}

void loop() {
    if (Serial.available()) {
        char cmd = Serial.read();
        
        switch (cmd) {
            case 'o':
            case 'O':
                calibrateOffset();
                break;
            case 'g':
            case 'G':
                calibrateGain();
                break;
            case 'r':
            case 'R':
                readValues();
                break;
            case 's':
            case 'S':
                showCalibration();
                break;
        }
    }
    
    delay(10);
}

void calibrateOffset() {
    Serial.println("Calibrating offset...");
    Serial.println("Make sure inputs are shorted!");
    delay(1000);
    
    // Average multiple readings
    int32_t sum_ch0 = 0;
    int32_t sum_ch1 = 0;
    int count = 0;
    
    for (int i = 0; i < 100; i++) {
        while (!adc.dataReady()) delay(1);
        
        ads131m02_data_t data;
        if (adc.readData(&data)) {
            sum_ch0 += data.ch0;
            sum_ch1 += data.ch1;
            count++;
        }
    }
    
    if (count > 0) {
        int32_t offset_ch0 = sum_ch0 / count;
        int32_t offset_ch1 = sum_ch1 / count;
        
        // Set offset calibration (negative to cancel)
        adc.setOffsetCal(0, -offset_ch0);
        adc.setOffsetCal(1, -offset_ch1);
        
        Serial.print("CH0 offset: ");
        Serial.println(offset_ch0);
        Serial.print("CH1 offset: ");
        Serial.println(offset_ch1);
        Serial.println("Offset calibration applied!");
    }
}

void calibrateGain() {
    Serial.println("Calibrating gain...");
    Serial.println("Apply a known voltage (e.g., 100mV)");
    Serial.println("Enter the applied voltage in mV:");
    
    // Wait for user input
    while (!Serial.available()) delay(10);
    float known_mv = Serial.parseFloat();
    
    Serial.print("Using reference: ");
    Serial.print(known_mv);
    Serial.println(" mV");
    
    delay(500);
    
    // Average multiple readings
    int32_t sum_ch0 = 0;
    int count = 0;
    
    for (int i = 0; i < 100; i++) {
        while (!adc.dataReady()) delay(1);
        
        ads131m02_data_t data;
        if (adc.readData(&data)) {
            sum_ch0 += data.ch0;
            count++;
        }
    }
    
    if (count > 0) {
        int32_t measured_raw = sum_ch0 / count;
        float measured_mv = ADS131M02::toMillivolts(measured_raw, adc.getGain(0));
        
        // Calculate gain correction
        // gain_cal = (expected / measured) * 0x800000
        float correction = known_mv / measured_mv;
        uint32_t gain_cal = (uint32_t)(correction * 0x800000);
        
        adc.setGainCal(0, gain_cal);
        
        Serial.print("Measured: ");
        Serial.print(measured_mv, 3);
        Serial.println(" mV");
        Serial.print("Correction factor: ");
        Serial.println(correction, 6);
        Serial.print("Gain cal value: 0x");
        Serial.println(gain_cal, HEX);
        Serial.println("Gain calibration applied to CH0!");
    }
}

void readValues() {
    Serial.println("Reading values...");
    
    for (int i = 0; i < 10; i++) {
        while (!adc.dataReady()) delay(1);
        
        ads131m02_data_t data;
        if (adc.readData(&data)) {
            float mv_ch0 = ADS131M02::toMillivolts(data.ch0, adc.getGain(0));
            float mv_ch1 = ADS131M02::toMillivolts(data.ch1, adc.getGain(1));
            
            Serial.print("CH0: ");
            Serial.print(mv_ch0, 4);
            Serial.print(" mV  CH1: ");
            Serial.print(mv_ch1, 4);
            Serial.println(" mV");
        }
        delay(100);
    }
}

void showCalibration() {
    Serial.println("Current calibration values:");
    
    Serial.print("CH0 Offset: ");
    Serial.println(adc.getOffsetCal(0));
    Serial.print("CH0 Gain: 0x");
    Serial.println(adc.getGainCal(0), HEX);
    
    Serial.print("CH1 Offset: ");
    Serial.println(adc.getOffsetCal(1));
    Serial.print("CH1 Gain: 0x");
    Serial.println(adc.getGainCal(1), HEX);
}
