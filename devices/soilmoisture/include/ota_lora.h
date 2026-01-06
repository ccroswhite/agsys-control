/**
 * @file ota_lora.h
 * @brief LoRa-based Over-The-Air Firmware Update System
 * 
 * Enables remote firmware updates via LoRa without requiring physical
 * access to devices. Supports staggered updates across device fleets.
 * 
 * Protocol:
 * 1. Controller broadcasts OTA_ANNOUNCE with firmware version/size
 * 2. Devices needing update respond with OTA_REQUEST (staggered)
 * 3. Controller sends firmware in OTA_CHUNK packets
 * 4. Device ACKs each chunk, stores in FRAM
 * 5. After all chunks received, device verifies CRC
 * 6. Device applies update via bootloader and reboots
 */

#ifndef OTA_LORA_H
#define OTA_LORA_H

#include <Arduino.h>

/* ==========================================================================
 * OTA MESSAGE TYPES
 * ========================================================================== */

// Message types (0x10-0x1F reserved for OTA)
#define MSG_TYPE_OTA_ANNOUNCE       0x10    // Controller → All: New firmware available
#define MSG_TYPE_OTA_REQUEST        0x11    // Device → Controller: Request update
#define MSG_TYPE_OTA_CHUNK          0x12    // Controller → Device: Firmware chunk
#define MSG_TYPE_OTA_CHUNK_ACK      0x13    // Device → Controller: Chunk received OK
#define MSG_TYPE_OTA_CHUNK_NACK     0x14    // Device → Controller: Chunk error, resend
#define MSG_TYPE_OTA_COMPLETE       0x15    // Device → Controller: All chunks received
#define MSG_TYPE_OTA_ABORT          0x16    // Either direction: Abort update
#define MSG_TYPE_OTA_STATUS         0x17    // Device → Controller: Update status

/* ==========================================================================
 * OTA CONFIGURATION
 * ========================================================================== */

#define OTA_CHUNK_SIZE              200     // Bytes per chunk (fits in LoRa packet)
#define OTA_MAX_FIRMWARE_SIZE       (256 * 1024)  // 256KB max firmware
#define OTA_MAX_CHUNKS              (OTA_MAX_FIRMWARE_SIZE / OTA_CHUNK_SIZE)
#define OTA_CHUNK_TIMEOUT_MS        10000   // Timeout waiting for chunk
#define OTA_MAX_RETRIES             5       // Max retries per chunk
#define OTA_STAGGER_MAX_MS          (30 * 60 * 1000)  // 30 min max stagger delay

// FRAM addresses for OTA storage (from nvram_layout.h)
// OTA uses the dedicated OTA staging region to avoid overwriting config
#include "nvram_layout.h"
#define OTA_FRAM_HEADER_ADDR        NVRAM_OTA_HEADER_ADDR   // 0x0400
#define OTA_FRAM_BITMAP_ADDR        (NVRAM_OTA_HEADER_ADDR + 0x18)  // Inside header
#define OTA_FRAM_DATA_ADDR          NVRAM_OTA_DATA_ADDR     // 0x0440

/* ==========================================================================
 * OTA PACKET STRUCTURES
 * ========================================================================== */

/**
 * OTA Announce - Broadcast by controller when new firmware is available
 */
struct OtaAnnounce {
    uint8_t  targetDeviceType;      // Which device type (0xFF = all)
    uint8_t  versionMajor;          // New firmware version
    uint8_t  versionMinor;
    uint8_t  versionPatch;
    uint32_t firmwareSize;          // Total size in bytes
    uint16_t totalChunks;           // Number of chunks
    uint32_t firmwareCrc;           // CRC32 of entire firmware
    uint32_t announceId;            // Unique ID for this update session
} __attribute__((packed));

/**
 * OTA Request - Device requests to receive update
 */
struct OtaRequest {
    uint32_t announceId;            // Which announcement we're responding to
    uint8_t  currentVersionMajor;   // Current firmware version
    uint8_t  currentVersionMinor;
    uint8_t  currentVersionPatch;
    uint16_t lastChunkReceived;     // For resume (0xFFFF = start fresh)
} __attribute__((packed));

/**
 * OTA Chunk - Single chunk of firmware data
 */
struct OtaChunk {
    uint32_t announceId;            // Update session ID
    uint16_t chunkIndex;            // Which chunk (0-based)
    uint16_t chunkSize;             // Actual bytes in this chunk
    uint16_t chunkCrc;              // CRC16 of chunk data
    uint8_t  data[OTA_CHUNK_SIZE];  // Chunk data
} __attribute__((packed));

/**
 * OTA Chunk ACK - Acknowledge receipt of chunk
 */
struct OtaChunkAck {
    uint32_t announceId;            // Update session ID
    uint16_t chunkIndex;            // Which chunk we received
    uint8_t  status;                // 0 = OK, 1 = CRC error, 2 = storage error
} __attribute__((packed));

/**
 * OTA Complete - All chunks received, ready to apply
 */
struct OtaComplete {
    uint32_t announceId;            // Update session ID
    uint32_t calculatedCrc;         // CRC we calculated from received data
    uint8_t  status;                // 0 = CRC match, 1 = CRC mismatch
} __attribute__((packed));

/**
 * OTA Status - Progress report
 */
struct OtaStatus {
    uint32_t announceId;            // Update session ID
    uint16_t chunksReceived;        // How many chunks we have
    uint16_t totalChunks;           // Total expected
    uint8_t  state;                 // Current OTA state
    uint8_t  errorCode;             // Last error (0 = none)
} __attribute__((packed));

/* ==========================================================================
 * OTA STATE MACHINE
 * ========================================================================== */

enum OtaState {
    OTA_STATE_IDLE = 0,             // No update in progress
    OTA_STATE_ANNOUNCED,            // Received announce, waiting to request
    OTA_STATE_REQUESTING,           // Sent request, waiting for first chunk
    OTA_STATE_RECEIVING,            // Receiving chunks
    OTA_STATE_VERIFYING,            // All chunks received, verifying CRC
    OTA_STATE_READY_TO_APPLY,       // Verified, ready to apply on next boot
    OTA_STATE_ERROR                 // Error occurred
};

enum OtaError {
    OTA_ERROR_NONE = 0,
    OTA_ERROR_TIMEOUT,              // Chunk timeout
    OTA_ERROR_CRC_CHUNK,            // Chunk CRC mismatch
    OTA_ERROR_CRC_FIRMWARE,         // Firmware CRC mismatch
    OTA_ERROR_STORAGE,              // FRAM write error
    OTA_ERROR_SIZE,                 // Firmware too large
    OTA_ERROR_ABORTED               // Update aborted
};

/* ==========================================================================
 * OTA HEADER (stored in FRAM)
 * ========================================================================== */

struct OtaHeader {
    uint32_t magic;                 // 0x4F544148 ("OTAH")
    uint32_t announceId;            // Current update session
    uint32_t firmwareSize;          // Expected size
    uint32_t firmwareCrc;           // Expected CRC
    uint16_t totalChunks;           // Total chunks expected
    uint16_t chunksReceived;        // Chunks received so far
    uint8_t  versionMajor;          // Target version
    uint8_t  versionMinor;
    uint8_t  versionPatch;
    uint8_t  state;                 // OtaState
    uint8_t  errorCode;             // OtaError
    uint8_t  reserved[7];           // Padding to 32 bytes
} __attribute__((packed));

#define OTA_HEADER_MAGIC            0x4F544148

/* ==========================================================================
 * OTA MANAGER CLASS
 * ========================================================================== */

class OtaLoRa {
public:
    /**
     * Initialize OTA system
     * @param deviceUuid Pointer to 16-byte device UUID
     */
    void init(const uint8_t* deviceUuid);
    
    /**
     * Process received OTA message
     * @param msgType Message type from packet header
     * @param payload Pointer to payload data
     * @param payloadLen Length of payload
     * @return true if message was handled
     */
    bool processMessage(uint8_t msgType, const uint8_t* payload, uint8_t payloadLen);
    
    /**
     * Check for OTA timeouts and handle state machine
     * Call this periodically (e.g., in loop)
     */
    void update();
    
    /**
     * Get current OTA state
     */
    OtaState getState() const { return _state; }
    
    /**
     * Get last error code
     */
    OtaError getError() const { return _error; }
    
    /**
     * Get update progress (0-100%)
     */
    uint8_t getProgress() const;
    
    /**
     * Check if firmware is ready to apply
     */
    bool isReadyToApply() const { return _state == OTA_STATE_READY_TO_APPLY; }
    
    /**
     * Apply the downloaded firmware (triggers bootloader)
     * Does not return on success
     */
    void applyUpdate();
    
    /**
     * Abort current update
     */
    void abort();
    
    /**
     * Check if device needs update based on announce
     */
    bool needsUpdate(const OtaAnnounce* announce) const;
    
    /**
     * Calculate staggered delay for this device
     */
    uint32_t calculateStaggerDelay() const;
    
private:
    // State
    OtaState _state;
    OtaError _error;
    uint32_t _announceId;
    uint32_t _staggerDelay;
    uint32_t _lastActivityTime;
    uint16_t _nextExpectedChunk;
    uint8_t  _retryCount;
    
    // Device info
    const uint8_t* _deviceUuid;
    
    // Cached header
    OtaHeader _header;
    
    // Internal methods
    void handleAnnounce(const OtaAnnounce* announce);
    void handleChunk(const OtaChunk* chunk);
    void sendRequest();
    void sendChunkAck(uint16_t chunkIndex, uint8_t status);
    void sendComplete();
    void sendStatus();
    
    bool saveHeader();
    bool loadHeader();
    bool saveChunk(uint16_t index, const uint8_t* data, uint16_t size);
    bool isChunkReceived(uint16_t index);
    void markChunkReceived(uint16_t index);
    
    uint32_t calculateFirmwareCrc();
    uint16_t calculateChunkCrc(const uint8_t* data, uint16_t size);
};

// Global OTA manager instance
extern OtaLoRa otaLora;

#endif // OTA_LORA_H
