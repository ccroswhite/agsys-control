/**
 * @file main.cpp
 * @brief Electromagnetic Flow Meter (Mag Meter) Main Application
 * 
 * Measures water flow using electromagnetic induction with capacitively-coupled
 * electrodes on PVC pipe. Reports readings via LoRa and displays on TFT LCD.
 */

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_FRAM_SPI.h>
#include "magmeter_config.h"
#include "ADS131M02.h"
#include "agsys_protocol.h"
#include "agsys_lora.h"
#include "agsys_crypto.h"

// Module includes
#include "display.h"
#include "signal_processing.h"
#include "coil_driver.h"
#include "buttons.h"
#include "settings.h"
#include "calibration.h"
#include "ble_service.h"

/* ==========================================================================
 * GLOBAL STATE
 * ========================================================================== */

// Hardware instances
Adafruit_FRAM_SPI fram = Adafruit_FRAM_SPI(PIN_FRAM_CS);
ADS131M02 adc;

// Tier configuration (detected at startup)
uint8_t currentTier = TIER_MM_S;
MagmeterTier_t tierConfig;

// Flow measurement
float currentFlowRate_LPM = 0.0f;      // Liters per minute
float totalVolume_L = 0.0f;            // Total liters
float currentVelocity_MPS = 0.0f;      // Meters per second

// Timing
uint32_t lastReportTime = 0;
uint32_t lastDisplayUpdate = 0;
uint32_t lastCalibrationSave = 0;

// Device state
uint8_t deviceUid[8];
uint8_t statusFlags = 0;

// Last ADC reading (for calibration capture)
static int32_t lastElectrodeReading = 0;

// Trend and average tracking
static float trendVolume_L = 0.0f;
static float avgVolume_L = 0.0f;
static float volumeAtTrendStart = 0.0f;
static uint32_t trendStartTime = 0;
static float volumeHistory[120];  // 1 sample per minute, 2 hours max
static uint8_t volumeHistoryIndex = 0;
static uint8_t volumeHistoryCount = 0;

/* ==========================================================================
 * FUNCTION PROTOTYPES
 * ========================================================================== */

void initPins(void);
void initSPI(void);
void initADC(void);
void initLoRa(void);
void initFRAM(void);
void initDisplay(void);

void detectTier(void);
void updateTrendAndAvg(void);
void handleButtons(void);

// Hardware-synced ADC callbacks
void onAdcTrigger(bool polarity);
void onPolarityChange(bool polarity);
void calculateFlow(void);

void sendReport(void);
void processLoRa(void);
void updateDisplay(void);

void getDeviceUid(uint8_t* uid);

/* ==========================================================================
 * TIER CONFIGURATIONS
 * ========================================================================== */

const MagmeterTier_t TIER_CONFIGS[] = {
    // TIER_MM_S: 1.5" - 2" pipe
    {
        .voltage_mv = 24000,
        .frequency_hz = 2000,
        .current_ma = 500,
        .pipe_diameter_mm = 40.9f,  // 1.5" Schedule 40 ID
        .k_factor = 1.0f
    },
    // TIER_MM_M: 2.5" - 3" pipe
    {
        .voltage_mv = 48000,
        .frequency_hz = 1000,
        .current_ma = 1000,
        .pipe_diameter_mm = 62.7f,  // 2.5" Schedule 40 ID
        .k_factor = 1.0f
    },
    // TIER_MM_L: 4" pipe
    {
        .voltage_mv = 60000,
        .frequency_hz = 500,
        .current_ma = 2000,
        .pipe_diameter_mm = 102.3f, // 4" Schedule 40 ID
        .k_factor = 1.0f
    }
};

/* ==========================================================================
 * SETUP
 * ========================================================================== */

void setup() {
    #if DEBUG_MODE
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    DEBUG_PRINTLN("Mag Meter Starting...");
    #endif

    initPins();
    initSPI();
    initFRAM();
    
    // Detect which power board tier is connected
    detectTier();
    DEBUG_PRINTF("Detected tier: %d\n", currentTier);
    
    // Initialize settings and calibration from FRAM
    settings_init();
    calibration_init();
    
    // Set default max flow based on tier if not configured
    UserSettings_t* settings = settings_get();
    if (settings->maxFlowLPM < 10.0f) {
        settings->maxFlowLPM = settings_getDefaultMaxFlow(currentTier);
        settings_save();
    }
    
    // Initialize ADC
    initADC();
    
    // Initialize buttons
    buttons_init();
    
    // Initialize display
    initDisplay();
    display_setSettings(settings);
    display_showSplash();
    
    // Get device UID and initialize LoRa
    getDeviceUid(deviceUid);
    initLoRa();
    
    // Initialize AgSys LoRa layer
    if (!agsys_lora_init(deviceUid, DEVICE_TYPE)) {
        DEBUG_PRINTLN("ERROR: Failed to initialize AgSys LoRa");
    }
    
    // Load crypto nonce from FRAM
    uint32_t savedNonce = 0;
    fram.read(FRAM_ADDR_NONCE, (uint8_t*)&savedNonce, sizeof(savedNonce));
    agsys_crypto_setNonce(savedNonce);
    
    // Initialize signal processing
    signal_init();
    
    // Setup coil driver with ADC trigger callback
    coil_init(tierConfig.frequency_hz);
    coil_setPolarityCallback(onPolarityChange);
    coil_setAdcTriggerCallback(onAdcTrigger);
    coil_start();
    
    // Initialize BLE
    ble_init();
    ble_startAdvertising();
    
    DEBUG_PRINTLN("Mag Meter Ready");
    
    // Initial display update
    display_showMain();
    
    // Turn on backlight based on settings
    digitalWrite(PIN_DISP_BL_EN, settings->backlightOn ? HIGH : LOW);
    
    // Initialize trend tracking
    trendStartTime = millis();
    volumeAtTrendStart = totalVolume_L;
    
    lastReportTime = millis();
    lastDisplayUpdate = millis();
}

/* ==========================================================================
 * MAIN LOOP
 * ========================================================================== */

void loop() {
    uint32_t now = millis();
    
    // ADC samples are now collected via hardware-synced callback (onAdcTrigger)
    // No polling needed - coil timer triggers ADC reads at optimal times
    
    // Calculate flow rate periodically (processes accumulated samples)
    static uint32_t lastFlowCalc = 0;
    if (now - lastFlowCalc >= 1000) {
        calculateFlow();
        lastFlowCalc = now;
    }
    
    // Update trend and average every minute
    static uint32_t lastTrendUpdate = 0;
    if (now - lastTrendUpdate >= 60000) {
        updateTrendAndAvg();
        lastTrendUpdate = now;
    }
    
    // Handle button input
    handleButtons();
    
    // Update display
    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_MS) {
        updateDisplay();
        lastDisplayUpdate = now;
    }
    
    // Process incoming LoRa messages
    processLoRa();
    
    // Send periodic report
    if (now - lastReportTime >= REPORT_INTERVAL_MS) {
        sendReport();
        lastReportTime = now;
    }
    
    // Process BLE and send live data updates
    ble_process();
    static uint32_t lastBleUpdate = 0;
    if (ble_isConnected() && (now - lastBleUpdate >= 1000)) {
        bool reverseFlow = (currentFlowRate_LPM < 0);
        ble_updateLiveData(currentFlowRate_LPM, totalVolume_L, trendVolume_L, avgVolume_L, reverseFlow);
        ble_updateDiagnostics(lastElectrodeReading, 0, tierConfig.frequency_hz, tierConfig.current_ma);
        lastBleUpdate = now;
    }
    
    // LVGL tick
    lv_timer_handler();
}

/* ==========================================================================
 * INITIALIZATION FUNCTIONS
 * ========================================================================== */

void initPins(void) {
    // Status LED
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);
    
    // Backlight (off initially)
    pinMode(PIN_DISP_BL_EN, OUTPUT);
    digitalWrite(PIN_DISP_BL_EN, LOW);
    
    // SPI chip selects - all high (inactive)
    pinMode(PIN_ADC_CS, OUTPUT);
    pinMode(PIN_DISP_CS, OUTPUT);
    pinMode(PIN_LORA_CS, OUTPUT);
    pinMode(PIN_FRAM_CS, OUTPUT);
    
    digitalWrite(PIN_ADC_CS, HIGH);
    digitalWrite(PIN_DISP_CS, HIGH);
    digitalWrite(PIN_LORA_CS, HIGH);
    digitalWrite(PIN_FRAM_CS, HIGH);
    
    // ADC control pins
    pinMode(PIN_ADC_DRDY, INPUT);
    pinMode(PIN_ADC_SYNC_RST, OUTPUT);
    digitalWrite(PIN_ADC_SYNC_RST, HIGH);
    
    // Tier ID analog input
    pinMode(PIN_TIER_ID, INPUT);
}

void initSPI(void) {
    SPI.begin();
}

void initADC(void) {
    DEBUG_PRINTLN("Initializing ADC...");
    
    if (!adc.begin(PIN_ADC_CS, PIN_ADC_DRDY, PIN_ADC_SYNC_RST)) {
        DEBUG_PRINTLN("ERROR: ADS131M02 not found!");
        while (1) {
            digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));
            delay(100);
        }
    }
    
    // Configure ADC
    adc.setGain(ADC_CH_ELECTRODE, (ads131m02_gain_t)ADC_GAIN_ELECTRODE);
    adc.setGain(ADC_CH_CURRENT, (ads131m02_gain_t)ADC_GAIN_CURRENT);
    adc.setOSR(ADS131M02_OSR_4096);  // 1 kSPS
    
    // Load calibration into ADC
    adc.setOffsetCal(0, calibration.offset_ch0);
    adc.setOffsetCal(1, calibration.offset_ch1);
    adc.setGainCal(0, calibration.gain_ch0);
    adc.setGainCal(1, calibration.gain_ch1);
    
    DEBUG_PRINTLN("ADC initialized");
}

void initLoRa(void) {
    DEBUG_PRINTLN("Initializing LoRa...");
    
    LoRa.setPins(PIN_LORA_CS, PIN_LORA_RST, PIN_LORA_DIO0);
    
    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("ERROR: LoRa init failed!");
        display_showError("LoRa Failed");
        while (1) {
            digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));
            delay(100);
        }
    }
    
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    LoRa.setSignalBandwidth(LORA_BANDWIDTH);
    LoRa.setCodingRate4(LORA_CODING_RATE);
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    
    DEBUG_PRINTLN("LoRa initialized");
}

void initFRAM(void) {
    DEBUG_PRINTLN("Initializing FRAM...");
    if (!fram.begin()) {
        DEBUG_PRINTLN("WARNING: FRAM init failed, using defaults");
    }
    DEBUG_PRINTLN("FRAM initialized");
}

void detectTier(void) {
    // Read tier ID voltage divider
    int adcValue = analogRead(PIN_TIER_ID);
    
    DEBUG_PRINTF("Tier ID ADC: %d\n", adcValue);
    
    if (adcValue < TIER_ID_THRESHOLD_SM) {
        currentTier = TIER_MM_S;
    } else if (adcValue < TIER_ID_THRESHOLD_ML) {
        currentTier = TIER_MM_M;
    } else {
        currentTier = TIER_MM_L;
    }
    
    tierConfig = TIER_CONFIGS[currentTier];
}

void loadCalibration(void) {
    fram.read(FRAM_ADDR_CALIBRATION, (uint8_t*)&calibration, sizeof(calibration));
    
    // Verify checksum
    uint32_t checksum = 0;
    uint8_t* data = (uint8_t*)&calibration;
    for (size_t i = 0; i < sizeof(calibration) - sizeof(uint32_t); i++) {
        checksum += data[i];
    }
    
    if (checksum != calibration.checksum) {
        DEBUG_PRINTLN("Calibration checksum invalid, using defaults");
        calibration.offset_ch0 = CAL_OFFSET_DEFAULT;
        calibration.offset_ch1 = CAL_OFFSET_DEFAULT;
        calibration.gain_ch0 = CAL_GAIN_DEFAULT;
        calibration.gain_ch1 = CAL_GAIN_DEFAULT;
        calibration.k_factor = CAL_K_FACTOR_DEFAULT;
    }
    
    DEBUG_PRINTF("Calibration loaded: k_factor=%.4f\n", calibration.k_factor);
}

void saveCalibration(void) {
    // Calculate checksum
    calibration.checksum = 0;
    uint8_t* data = (uint8_t*)&calibration;
    for (size_t i = 0; i < sizeof(calibration) - sizeof(uint32_t); i++) {
        calibration.checksum += data[i];
    }
    
    fram.write(FRAM_ADDR_CALIBRATION, (uint8_t*)&calibration, sizeof(calibration));
    
    // Also save crypto nonce
    uint32_t nonce = agsys_crypto_getNonce();
    fram.write(FRAM_ADDR_NONCE, (uint8_t*)&nonce, sizeof(nonce));
    
    DEBUG_PRINTLN("Calibration saved");
}

void getDeviceUid(uint8_t* uid) {
    uint32_t deviceId0 = NRF_FICR->DEVICEID[0];
    uint32_t deviceId1 = NRF_FICR->DEVICEID[1];
    
    uid[0] = (deviceId0 >> 0) & 0xFF;
    uid[1] = (deviceId0 >> 8) & 0xFF;
    uid[2] = (deviceId0 >> 16) & 0xFF;
    uid[3] = (deviceId0 >> 24) & 0xFF;
    uid[4] = (deviceId1 >> 0) & 0xFF;
    uid[5] = (deviceId1 >> 8) & 0xFF;
    uid[6] = (deviceId1 >> 16) & 0xFF;
    uid[7] = (deviceId1 >> 24) & 0xFF;
}

/* ==========================================================================
 * ADC AND SIGNAL PROCESSING (Hardware-Synced)
 * ========================================================================== */

// ADC trigger callback - called by coil driver at optimal sample times
void onAdcTrigger(bool polarity) {
    ads131m02_data_t data;
    
    if (adc.readData(&data)) {
        // Store last reading for calibration/diagnostics
        lastElectrodeReading = data.ch0;
        
        // Add sample to signal processing with known polarity
        signal_addSample(data.ch0, data.ch1, polarity);
    }
}

// Polarity change callback - for diagnostics/debugging
void onPolarityChange(bool polarity) {
    // Could add diagnostics here if needed
    (void)polarity;
}

void calculateFlow(void) {
    // Compute flow signal from accumulated samples (synchronous detection)
    float signalAmplitude = signal_computeFlowSignal();
    float coilCurrentRaw = signal_computeCoilCurrent();
    
    // Convert ADC counts to voltage (microvolts)
    float signal_uV = ADS131M02::toMicrovolts((int32_t)signalAmplitude, 
                                               (ads131m02_gain_t)ADC_GAIN_ELECTRODE);
    
    // Convert coil current ADC counts to milliamps
    // Current sense: shunt voltage / shunt resistance
    // Assuming 0.1Ω shunt, ADC gain = 1
    float coilCurrent_uV = ADS131M02::toMicrovolts((int32_t)coilCurrentRaw,
                                                    (ads131m02_gain_t)ADC_GAIN_CURRENT);
    float coilCurrent_mA = (coilCurrent_uV / 1000.0f) / CURRENT_SENSE_SHUNT_OHMS;
    
    // Get calibration data
    CalibrationData_t* cal = calibration_get();
    
    // Normalize by coil current for ratiometric measurement
    // This compensates for temperature drift and supply variations
    // v = E / (B * D), where B ∝ I_coil
    float currentNormFactor = 1.0f;
    float expectedCurrent_mA = (float)tierConfig.current_ma;
    
    if (coilCurrent_mA > (expectedCurrent_mA * 0.5f)) {
        // Only normalize if current is reasonable (>50% of expected)
        currentNormFactor = expectedCurrent_mA / coilCurrent_mA;
        
        // Clamp normalization to ±20% to avoid wild swings from noise
        if (currentNormFactor < 0.8f) currentNormFactor = 0.8f;
        if (currentNormFactor > 1.2f) currentNormFactor = 1.2f;
    } else if (coilCurrent_mA < (expectedCurrent_mA * 0.1f)) {
        // Coil current too low - possible fault
        statusFlags |= 0x01;  // Set coil fault flag
        DEBUG_PRINTLN("WARNING: Coil current too low!");
    }
    
    // Calculate flow velocity using Faraday's law:
    // E = B * v * D
    // v = E / (B * D)
    // 
    // k_factor incorporates B, geometry, and field calibration
    // currentNormFactor compensates for actual vs expected field strength
    float velocity_mps = (signal_uV * 1e-6f) * cal->kFactor * currentNormFactor;
    
    // Clamp to valid range
    if (fabs(velocity_mps) < FLOW_MIN_VELOCITY_MPS) {
        velocity_mps = 0.0f;
    }
    if (fabs(velocity_mps) > FLOW_MAX_VELOCITY_MPS) {
        velocity_mps = (velocity_mps > 0) ? FLOW_MAX_VELOCITY_MPS : -FLOW_MAX_VELOCITY_MPS;
    }
    
    currentVelocity_MPS = velocity_mps;
    
    // Calculate volumetric flow rate
    // Q = v * A = v * π * (D/2)²
    float diameter_m = tierConfig.pipe_diameter_mm / 1000.0f;
    float area_m2 = 3.14159f * (diameter_m / 2.0f) * (diameter_m / 2.0f);
    float flowRate_m3ps = velocity_mps * area_m2;
    
    // Convert to liters per minute
    currentFlowRate_LPM = flowRate_m3ps * 1000.0f * 60.0f;
    
    // Accumulate total volume (1 second worth)
    totalVolume_L += currentFlowRate_LPM / 60.0f;
    
    // Reset signal processing for next averaging window
    signal_reset();
    
    DEBUG_PRINTF("Signal: %.2f uV, Coil: %.0f mA (norm=%.3f), Vel: %.3f m/s, Flow: %.2f L/min\n",
                 signal_uV, coilCurrent_mA, currentNormFactor, velocity_mps, currentFlowRate_LPM);
}

/* ==========================================================================
 * COMMUNICATION
 * ========================================================================== */

void sendReport(void) {
    DEBUG_PRINTLN("Sending mag meter report...");
    
    // Build payload (reuse water meter report structure for now)
    AgsysWaterMeterReport report;
    report.timestamp = millis() / 1000;
    report.totalPulses = 0;  // Not used for mag meter
    report.totalLiters = (uint32_t)totalVolume_L;
    report.flowRateLPM = (uint16_t)(currentFlowRate_LPM * 10);  // Fixed point
    report.batteryMv = 0;  // Mains powered
    report.flags = statusFlags;
    
    if (agsys_lora_send(AGSYS_MSG_WATER_METER_REPORT, (uint8_t*)&report, sizeof(report))) {
        DEBUG_PRINTLN("Report sent");
        digitalWrite(PIN_LED_STATUS, HIGH);
        delay(50);
        digitalWrite(PIN_LED_STATUS, LOW);
    } else {
        DEBUG_PRINTLN("ERROR: Failed to send report");
    }
}

void processLoRa(void) {
    AgsysHeader header;
    uint8_t payload[64];
    size_t payloadLen = sizeof(payload);
    int16_t rssi;
    
    if (agsys_lora_receive(&header, payload, &payloadLen, &rssi)) {
        DEBUG_PRINTF("Received message type 0x%02X, RSSI=%d\n", header.msgType, rssi);
        
        switch (header.msgType) {
            case AGSYS_MSG_TIME_SYNC:
                // Handle time sync if needed
                break;
                
            case AGSYS_MSG_CONFIG_UPDATE:
                // Handle configuration updates
                break;
                
            case AGSYS_MSG_ACK:
                // Handle acknowledgments
                break;
                
            default:
                DEBUG_PRINTF("Unknown message type: 0x%02X\n", header.msgType);
                break;
        }
    }
}

/* ==========================================================================
 * DISPLAY
 * ========================================================================== */

void updateDisplay(void) {
    bool reverseFlow = (currentFlowRate_LPM < 0);
    display_updateMain(currentFlowRate_LPM, totalVolume_L, trendVolume_L, avgVolume_L, reverseFlow);
}

/* ==========================================================================
 * BUTTON HANDLING
 * ========================================================================== */

void handleButtons(void) {
    ButtonEvent_t event = buttons_poll();
    if (event != BTN_NONE) {
        display_handleButton(event);
    }
}

/* ==========================================================================
 * TREND AND AVERAGE TRACKING
 * ========================================================================== */

void updateTrendAndAvg(void) {
    UserSettings_t* settings = settings_get();
    uint32_t now = millis();
    
    // Update trend (volume change over trend period)
    uint32_t trendPeriodMs = (uint32_t)settings->trendPeriodMin * 60000UL;
    if (now - trendStartTime >= trendPeriodMs) {
        trendVolume_L = totalVolume_L - volumeAtTrendStart;
        volumeAtTrendStart = totalVolume_L;
        trendStartTime = now;
    }
    
    // Store volume sample for averaging
    volumeHistory[volumeHistoryIndex] = totalVolume_L;
    volumeHistoryIndex = (volumeHistoryIndex + 1) % 120;
    if (volumeHistoryCount < 120) volumeHistoryCount++;
    
    // Calculate average over avg period
    uint8_t samplesToAvg = settings->avgPeriodMin;
    if (samplesToAvg > volumeHistoryCount) samplesToAvg = volumeHistoryCount;
    
    if (samplesToAvg > 1) {
        // Get oldest sample in the window
        uint8_t oldestIdx = (volumeHistoryIndex + 120 - samplesToAvg) % 120;
        float volumeChange = totalVolume_L - volumeHistory[oldestIdx];
        avgVolume_L = volumeChange / samplesToAvg;  // Volume per minute average
    } else {
        avgVolume_L = 0;
    }
}

/* ==========================================================================
 * CALIBRATION INTERFACE (called from display)
 * ========================================================================== */

// Provide last electrode reading for calibration
int32_t adc_getLastElectrodeReading(void) {
    return lastElectrodeReading;
}
