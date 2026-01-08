/**
 * @file agsys_lora.cpp
 * @brief AgSys LoRa Communication Layer Implementation
 */

#include "agsys_lora.h"
#include "agsys_crypto.h"
#include <LoRa.h>
#include <string.h>

// Device state
static uint8_t s_deviceUid[AGSYS_DEVICE_UID_SIZE];
static uint8_t s_deviceType = 0;
static uint16_t s_sequence = 0;
static bool s_initialized = false;

// Last received signal info
static int16_t s_lastRssi = 0;
static int8_t s_lastSnr = 0;

// Buffers
static uint8_t s_txBuffer[AGSYS_MAX_PACKET];
static uint8_t s_rxBuffer[AGSYS_MAX_PACKET];

bool agsys_lora_init(const uint8_t* deviceUid, uint8_t deviceType) {
    if (deviceUid == NULL) {
        return false;
    }
    
    memcpy(s_deviceUid, deviceUid, AGSYS_DEVICE_UID_SIZE);
    s_deviceType = deviceType;
    s_sequence = 0;
    
    // Initialize crypto with device UID
    agsys_crypto_init(deviceUid);
    
    s_initialized = true;
    return true;
}

void agsys_lora_buildHeader(AgsysHeader* header, uint8_t msgType) {
    if (header == NULL) {
        return;
    }
    
    header->magic[0] = AGSYS_MAGIC_BYTE1;
    header->magic[1] = AGSYS_MAGIC_BYTE2;
    header->version = AGSYS_PROTOCOL_VERSION;
    header->msgType = msgType;
    header->deviceType = s_deviceType;
    memcpy(header->deviceUid, s_deviceUid, AGSYS_DEVICE_UID_SIZE);
    header->sequence = s_sequence++;
}

bool agsys_lora_send(uint8_t msgType, const uint8_t* payload, size_t payloadLen) {
    if (!s_initialized) {
        return false;
    }
    
    // Check payload size
    if (payloadLen > AGSYS_MAX_PLAINTEXT - AGSYS_HEADER_SIZE) {
        return false;
    }
    
    // Build plaintext: header + payload
    uint8_t plaintext[AGSYS_MAX_PLAINTEXT];
    AgsysHeader* header = (AgsysHeader*)plaintext;
    agsys_lora_buildHeader(header, msgType);
    
    size_t plaintextLen = AGSYS_HEADER_SIZE;
    if (payload != NULL && payloadLen > 0) {
        memcpy(plaintext + AGSYS_HEADER_SIZE, payload, payloadLen);
        plaintextLen += payloadLen;
    }
    
    // Encrypt
    size_t packetLen = 0;
    if (!agsys_crypto_encrypt(plaintext, plaintextLen, s_txBuffer, &packetLen)) {
        return false;
    }
    
    // Transmit via LoRa
    if (!LoRa.beginPacket()) {
        return false;
    }
    
    LoRa.write(s_txBuffer, packetLen);
    
    if (!LoRa.endPacket()) {
        return false;
    }
    
    return true;
}

bool agsys_lora_available(void) {
    return LoRa.parsePacket() > 0;
}

bool agsys_lora_receive(AgsysHeader* header, uint8_t* payload, size_t* payloadLen, int16_t* rssi) {
    if (!s_initialized || header == NULL || payload == NULL || payloadLen == NULL) {
        return false;
    }
    
    int packetSize = LoRa.parsePacket();
    if (packetSize == 0) {
        return false;
    }
    
    // Read encrypted packet
    if (packetSize > (int)sizeof(s_rxBuffer)) {
        // Packet too large, discard
        while (LoRa.available()) {
            LoRa.read();
        }
        return false;
    }
    
    size_t rxLen = 0;
    while (LoRa.available() && rxLen < sizeof(s_rxBuffer)) {
        s_rxBuffer[rxLen++] = LoRa.read();
    }
    
    // Store signal info
    s_lastRssi = LoRa.packetRssi();
    s_lastSnr = LoRa.packetSnr() * 4;  // Convert to fixed point
    
    if (rssi != NULL) {
        *rssi = s_lastRssi;
    }
    
    // Decrypt
    uint8_t plaintext[AGSYS_MAX_PLAINTEXT];
    size_t plaintextLen = 0;
    
    if (!agsys_crypto_decrypt(s_rxBuffer, rxLen, plaintext, &plaintextLen)) {
        return false;
    }
    
    // Validate minimum size
    if (plaintextLen < AGSYS_HEADER_SIZE) {
        return false;
    }
    
    // Parse header
    memcpy(header, plaintext, AGSYS_HEADER_SIZE);
    
    // Validate magic bytes
    if (!AGSYS_HEADER_VALID(header)) {
        return false;
    }
    
    // Validate protocol version
    if (header->version != AGSYS_PROTOCOL_VERSION) {
        return false;
    }
    
    // Copy payload
    size_t actualPayloadLen = plaintextLen - AGSYS_HEADER_SIZE;
    if (actualPayloadLen > *payloadLen) {
        // Payload buffer too small
        return false;
    }
    
    if (actualPayloadLen > 0) {
        memcpy(payload, plaintext + AGSYS_HEADER_SIZE, actualPayloadLen);
    }
    *payloadLen = actualPayloadLen;
    
    return true;
}

uint16_t agsys_lora_getSequence(void) {
    return s_sequence;
}

void agsys_lora_setRadioParams(uint32_t frequency, uint8_t spreadingFactor, 
                                uint32_t bandwidth, int8_t txPower) {
    LoRa.setFrequency(frequency);
    LoRa.setSpreadingFactor(spreadingFactor);
    LoRa.setSignalBandwidth(bandwidth);
    LoRa.setTxPower(txPower);
}

void agsys_lora_sleep(void) {
    LoRa.sleep();
}

void agsys_lora_wake(void) {
    LoRa.idle();
}

int16_t agsys_lora_getLastRssi(void) {
    return s_lastRssi;
}

int8_t agsys_lora_getLastSnr(void) {
    return s_lastSnr;
}
