/**
 * @file ota_lora.cpp
 * @brief LoRa-based Over-The-Air Firmware Update Implementation
 */

#include <Arduino.h>
#include <LoRa.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include "ota_lora.h"
#include "config.h"
#include "nvram.h"
#include "nvram_layout.h"
#include "config_manager.h"
#include "protocol.h"

// Global instance
OtaLoRa otaLora;

// External references
extern NVRAM nvram;
extern Protocol protocol;
extern uint8_t deviceUUID[16];

// CRC32 lookup table
static const uint32_t crc32Table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

/**
 * Calculate CRC32
 */
static uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc = 0xFFFFFFFF) {
    for (size_t i = 0; i < len; i++) {
        crc = crc32Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/**
 * Calculate CRC16 (CCITT)
 */
static uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void OtaLoRa::init(const uint8_t* deviceUuid) {
    _deviceUuid = deviceUuid;
    _state = OTA_STATE_IDLE;
    _error = OTA_ERROR_NONE;
    _announceId = 0;
    _staggerDelay = 0;
    _lastActivityTime = 0;
    _nextExpectedChunk = 0;
    _retryCount = 0;
    
    // Try to load existing OTA header from FRAM
    if (loadHeader()) {
        // Resume interrupted update if valid
        if (_header.magic == OTA_HEADER_MAGIC && 
            _header.state == OTA_STATE_RECEIVING) {
            _state = OTA_STATE_RECEIVING;
            _announceId = _header.announceId;
            _nextExpectedChunk = _header.chunksReceived;
            DEBUG_PRINT("OTA: Resuming update, chunks: ");
            DEBUG_PRINT(_header.chunksReceived);
            DEBUG_PRINT("/");
            DEBUG_PRINTLN(_header.totalChunks);
        } else if (_header.magic == OTA_HEADER_MAGIC &&
                   _header.state == OTA_STATE_READY_TO_APPLY) {
            _state = OTA_STATE_READY_TO_APPLY;
            DEBUG_PRINTLN("OTA: Firmware ready to apply");
        }
    }
}

bool OtaLoRa::processMessage(uint8_t msgType, const uint8_t* payload, uint8_t payloadLen) {
    switch (msgType) {
        case MSG_TYPE_OTA_ANNOUNCE:
            if (payloadLen >= sizeof(OtaAnnounce)) {
                handleAnnounce((const OtaAnnounce*)payload);
                return true;
            }
            break;
            
        case MSG_TYPE_OTA_CHUNK:
            if (payloadLen >= sizeof(OtaChunk) - OTA_CHUNK_SIZE) {
                handleChunk((const OtaChunk*)payload);
                return true;
            }
            break;
            
        case MSG_TYPE_OTA_ABORT:
            DEBUG_PRINTLN("OTA: Received abort");
            abort();
            return true;
            
        default:
            break;
    }
    return false;
}

void OtaLoRa::update() {
    uint32_t now = millis();
    
    switch (_state) {
        case OTA_STATE_ANNOUNCED:
            // Wait for stagger delay, then send request
            if (now - _lastActivityTime >= _staggerDelay) {
                sendRequest();
                _state = OTA_STATE_REQUESTING;
                _lastActivityTime = now;
            }
            break;
            
        case OTA_STATE_REQUESTING:
        case OTA_STATE_RECEIVING:
            // Check for timeout
            if (now - _lastActivityTime >= OTA_CHUNK_TIMEOUT_MS) {
                _retryCount++;
                if (_retryCount >= OTA_MAX_RETRIES) {
                    DEBUG_PRINTLN("OTA: Max retries exceeded");
                    _error = OTA_ERROR_TIMEOUT;
                    _state = OTA_STATE_ERROR;
                    _header.state = OTA_STATE_ERROR;
                    _header.errorCode = OTA_ERROR_TIMEOUT;
                    saveHeader();
                } else {
                    DEBUG_PRINT("OTA: Timeout, retry ");
                    DEBUG_PRINTLN(_retryCount);
                    // Request the missing chunk
                    sendChunkAck(_nextExpectedChunk, 1);  // NACK to request resend
                    _lastActivityTime = now;
                }
            }
            break;
            
        case OTA_STATE_VERIFYING:
            // Verify CRC of complete firmware
            {
                uint32_t calculatedCrc = calculateFirmwareCrc();
                if (calculatedCrc == _header.firmwareCrc) {
                    DEBUG_PRINTLN("OTA: CRC verified, ready to apply");
                    _state = OTA_STATE_READY_TO_APPLY;
                    _header.state = OTA_STATE_READY_TO_APPLY;
                    saveHeader();
                    sendComplete();
                } else {
                    DEBUG_PRINTLN("OTA: CRC mismatch!");
                    _error = OTA_ERROR_CRC_FIRMWARE;
                    _state = OTA_STATE_ERROR;
                    _header.state = OTA_STATE_ERROR;
                    _header.errorCode = OTA_ERROR_CRC_FIRMWARE;
                    saveHeader();
                }
            }
            break;
            
        default:
            break;
    }
}

bool OtaLoRa::needsUpdate(const OtaAnnounce* announce) const {
    // Check if this announcement is for our device type
    if (announce->targetDeviceType != 0xFF && 
        announce->targetDeviceType != DEVICE_TYPE) {
        return false;
    }
    
    // Compare versions
    uint32_t currentVersion = (FIRMWARE_VERSION_MAJOR << 16) | 
                              (FIRMWARE_VERSION_MINOR << 8) | 
                              FIRMWARE_VERSION_PATCH;
    uint32_t newVersion = (announce->versionMajor << 16) | 
                          (announce->versionMinor << 8) | 
                          announce->versionPatch;
    
    return newVersion > currentVersion;
}

uint32_t OtaLoRa::calculateStaggerDelay() const {
    // Use UUID to generate deterministic but distributed delay
    uint32_t hash = 0;
    for (int i = 0; i < 16; i++) {
        hash = hash * 31 + _deviceUuid[i];
    }
    return hash % OTA_STAGGER_MAX_MS;
}

uint8_t OtaLoRa::getProgress() const {
    if (_header.totalChunks == 0) return 0;
    return (uint8_t)((_header.chunksReceived * 100) / _header.totalChunks);
}

void OtaLoRa::abort() {
    _state = OTA_STATE_IDLE;
    _error = OTA_ERROR_ABORTED;
    _announceId = 0;
    
    // Clear header
    memset(&_header, 0, sizeof(_header));
    saveHeader();
    
    DEBUG_PRINTLN("OTA: Aborted");
}

void OtaLoRa::handleAnnounce(const OtaAnnounce* announce) {
    DEBUG_PRINT("OTA: Announce v");
    DEBUG_PRINT(announce->versionMajor);
    DEBUG_PRINT(".");
    DEBUG_PRINT(announce->versionMinor);
    DEBUG_PRINT(".");
    DEBUG_PRINTLN(announce->versionPatch);
    
    // Check if we need this update
    if (!needsUpdate(announce)) {
        DEBUG_PRINTLN("OTA: Already up to date");
        return;
    }
    
    // Check firmware size
    if (announce->firmwareSize > OTA_MAX_FIRMWARE_SIZE) {
        DEBUG_PRINTLN("OTA: Firmware too large");
        return;
    }
    
    // If already receiving same update, ignore
    if (_state == OTA_STATE_RECEIVING && _announceId == announce->announceId) {
        return;
    }
    
    // Initialize new update
    _announceId = announce->announceId;
    _staggerDelay = calculateStaggerDelay();
    _lastActivityTime = millis();
    _nextExpectedChunk = 0;
    _retryCount = 0;
    _state = OTA_STATE_ANNOUNCED;
    _error = OTA_ERROR_NONE;
    
    // Initialize header
    _header.magic = OTA_HEADER_MAGIC;
    _header.announceId = announce->announceId;
    _header.firmwareSize = announce->firmwareSize;
    _header.firmwareCrc = announce->firmwareCrc;
    _header.totalChunks = announce->totalChunks;
    _header.chunksReceived = 0;
    _header.versionMajor = announce->versionMajor;
    _header.versionMinor = announce->versionMinor;
    _header.versionPatch = announce->versionPatch;
    _header.state = OTA_STATE_ANNOUNCED;
    _header.errorCode = OTA_ERROR_NONE;
    
    saveHeader();
    
    // Clear chunk bitmap
    uint8_t zeros[128] = {0};
    nvram.write(OTA_FRAM_BITMAP_ADDR, zeros, sizeof(zeros));
    
    DEBUG_PRINT("OTA: Will request in ");
    DEBUG_PRINT(_staggerDelay / 1000);
    DEBUG_PRINTLN(" seconds");
}

void OtaLoRa::handleChunk(const OtaChunk* chunk) {
    // Verify this is for our current update
    if (chunk->announceId != _announceId) {
        return;
    }
    
    // Verify chunk CRC
    uint16_t calculatedCrc = crc16(chunk->data, chunk->chunkSize);
    if (calculatedCrc != chunk->chunkCrc) {
        DEBUG_PRINT("OTA: Chunk ");
        DEBUG_PRINT(chunk->chunkIndex);
        DEBUG_PRINTLN(" CRC error");
        sendChunkAck(chunk->chunkIndex, 1);  // CRC error
        return;
    }
    
    // Store chunk in FRAM
    if (!saveChunk(chunk->chunkIndex, chunk->data, chunk->chunkSize)) {
        DEBUG_PRINTLN("OTA: Storage error");
        sendChunkAck(chunk->chunkIndex, 2);  // Storage error
        return;
    }
    
    // Mark chunk as received
    markChunkReceived(chunk->chunkIndex);
    _header.chunksReceived++;
    _lastActivityTime = millis();
    _retryCount = 0;
    
    // ACK the chunk
    sendChunkAck(chunk->chunkIndex, 0);  // OK
    
    DEBUG_PRINT("OTA: Chunk ");
    DEBUG_PRINT(chunk->chunkIndex + 1);
    DEBUG_PRINT("/");
    DEBUG_PRINT(_header.totalChunks);
    DEBUG_PRINT(" (");
    DEBUG_PRINT(getProgress());
    DEBUG_PRINTLN("%)");
    
    // Update state
    _state = OTA_STATE_RECEIVING;
    _header.state = OTA_STATE_RECEIVING;
    
    // Check if all chunks received
    if (_header.chunksReceived >= _header.totalChunks) {
        DEBUG_PRINTLN("OTA: All chunks received, verifying...");
        _state = OTA_STATE_VERIFYING;
        _header.state = OTA_STATE_VERIFYING;
    }
    
    saveHeader();
    
    // Expect next chunk
    _nextExpectedChunk = chunk->chunkIndex + 1;
}

void OtaLoRa::sendRequest() {
    uint8_t buffer[64];
    PacketHeader* header = (PacketHeader*)buffer;
    OtaRequest* request = (OtaRequest*)(buffer + sizeof(PacketHeader));
    
    // Build header
    header->magic[0] = PROTOCOL_MAGIC_BYTE1;
    header->magic[1] = PROTOCOL_MAGIC_BYTE2;
    header->version = 1;
    header->msgType = MSG_TYPE_OTA_REQUEST;
    header->deviceType = DEVICE_TYPE;
    memcpy(header->uuid, _deviceUuid, 16);
    header->sequence = protocol.nextSequence();
    header->payloadLen = sizeof(OtaRequest);
    
    // Build request
    request->announceId = _announceId;
    request->currentVersionMajor = FIRMWARE_VERSION_MAJOR;
    request->currentVersionMinor = FIRMWARE_VERSION_MINOR;
    request->currentVersionPatch = FIRMWARE_VERSION_PATCH;
    request->lastChunkReceived = (_header.chunksReceived > 0) ? 
                                  _header.chunksReceived - 1 : 0xFFFF;
    
    // Send
    LoRa.beginPacket();
    LoRa.write(buffer, sizeof(PacketHeader) + sizeof(OtaRequest));
    LoRa.endPacket();
    
    DEBUG_PRINTLN("OTA: Sent request");
}

void OtaLoRa::sendChunkAck(uint16_t chunkIndex, uint8_t status) {
    uint8_t buffer[64];
    PacketHeader* header = (PacketHeader*)buffer;
    OtaChunkAck* ack = (OtaChunkAck*)(buffer + sizeof(PacketHeader));
    
    // Build header
    header->magic[0] = PROTOCOL_MAGIC_BYTE1;
    header->magic[1] = PROTOCOL_MAGIC_BYTE2;
    header->version = 1;
    header->msgType = (status == 0) ? MSG_TYPE_OTA_CHUNK_ACK : MSG_TYPE_OTA_CHUNK_NACK;
    header->deviceType = DEVICE_TYPE;
    memcpy(header->uuid, _deviceUuid, 16);
    header->sequence = protocol.nextSequence();
    header->payloadLen = sizeof(OtaChunkAck);
    
    // Build ACK
    ack->announceId = _announceId;
    ack->chunkIndex = chunkIndex;
    ack->status = status;
    
    // Send
    LoRa.beginPacket();
    LoRa.write(buffer, sizeof(PacketHeader) + sizeof(OtaChunkAck));
    LoRa.endPacket();
}

void OtaLoRa::sendComplete() {
    uint8_t buffer[64];
    PacketHeader* header = (PacketHeader*)buffer;
    OtaComplete* complete = (OtaComplete*)(buffer + sizeof(PacketHeader));
    
    // Build header
    header->magic[0] = PROTOCOL_MAGIC_BYTE1;
    header->magic[1] = PROTOCOL_MAGIC_BYTE2;
    header->version = 1;
    header->msgType = MSG_TYPE_OTA_COMPLETE;
    header->deviceType = DEVICE_TYPE;
    memcpy(header->uuid, _deviceUuid, 16);
    header->sequence = protocol.nextSequence();
    header->payloadLen = sizeof(OtaComplete);
    
    // Build complete
    complete->announceId = _announceId;
    complete->calculatedCrc = calculateFirmwareCrc();
    complete->status = (complete->calculatedCrc == _header.firmwareCrc) ? 0 : 1;
    
    // Send
    LoRa.beginPacket();
    LoRa.write(buffer, sizeof(PacketHeader) + sizeof(OtaComplete));
    LoRa.endPacket();
    
    DEBUG_PRINTLN("OTA: Sent complete");
}

void OtaLoRa::sendStatus() {
    uint8_t buffer[64];
    PacketHeader* header = (PacketHeader*)buffer;
    OtaStatus* status = (OtaStatus*)(buffer + sizeof(PacketHeader));
    
    // Build header
    header->magic[0] = PROTOCOL_MAGIC_BYTE1;
    header->magic[1] = PROTOCOL_MAGIC_BYTE2;
    header->version = 1;
    header->msgType = MSG_TYPE_OTA_STATUS;
    header->deviceType = DEVICE_TYPE;
    memcpy(header->uuid, _deviceUuid, 16);
    header->sequence = protocol.nextSequence();
    header->payloadLen = sizeof(OtaStatus);
    
    // Build status
    status->announceId = _announceId;
    status->chunksReceived = _header.chunksReceived;
    status->totalChunks = _header.totalChunks;
    status->state = _state;
    status->errorCode = _error;
    
    // Send
    LoRa.beginPacket();
    LoRa.write(buffer, sizeof(PacketHeader) + sizeof(OtaStatus));
    LoRa.endPacket();
}

bool OtaLoRa::saveHeader() {
    return nvram.write(OTA_FRAM_HEADER_ADDR, (uint8_t*)&_header, sizeof(_header));
}

bool OtaLoRa::loadHeader() {
    return nvram.read(OTA_FRAM_HEADER_ADDR, (uint8_t*)&_header, sizeof(_header));
}

bool OtaLoRa::saveChunk(uint16_t index, const uint8_t* data, uint16_t size) {
    uint32_t addr = OTA_FRAM_DATA_ADDR + (index * OTA_CHUNK_SIZE);
    return nvram.write(addr, data, size);
}

bool OtaLoRa::isChunkReceived(uint16_t index) {
    uint8_t byteIndex = index / 8;
    uint8_t bitIndex = index % 8;
    uint8_t bitmap;
    
    nvram.read(OTA_FRAM_BITMAP_ADDR + byteIndex, &bitmap, 1);
    return (bitmap & (1 << bitIndex)) != 0;
}

void OtaLoRa::markChunkReceived(uint16_t index) {
    uint8_t byteIndex = index / 8;
    uint8_t bitIndex = index % 8;
    uint8_t bitmap;
    
    nvram.read(OTA_FRAM_BITMAP_ADDR + byteIndex, &bitmap, 1);
    bitmap |= (1 << bitIndex);
    nvram.write(OTA_FRAM_BITMAP_ADDR + byteIndex, &bitmap, 1);
}

uint32_t OtaLoRa::calculateFirmwareCrc() {
    uint32_t crcValue = 0xFFFFFFFF;
    uint8_t buffer[OTA_CHUNK_SIZE];
    
    for (uint16_t i = 0; i < _header.totalChunks; i++) {
        uint32_t addr = OTA_FRAM_DATA_ADDR + (i * OTA_CHUNK_SIZE);
        uint16_t chunkSize = OTA_CHUNK_SIZE;
        
        // Last chunk may be smaller
        if (i == _header.totalChunks - 1) {
            chunkSize = _header.firmwareSize - (i * OTA_CHUNK_SIZE);
        }
        
        nvram.read(addr, buffer, chunkSize);
        crcValue = crc32(buffer, chunkSize, crcValue ^ 0xFFFFFFFF) ^ 0xFFFFFFFF;
    }
    
    return crcValue ^ 0xFFFFFFFF;
}

uint16_t OtaLoRa::calculateChunkCrc(const uint8_t* data, uint16_t size) {
    return crc16(data, size);
}

void OtaLoRa::applyUpdate() {
    if (_state != OTA_STATE_READY_TO_APPLY) {
        DEBUG_PRINTLN("OTA: Not ready to apply");
        return;
    }
    
    DEBUG_PRINTLN("OTA: Applying update...");
    
    // On nRF52, we use the internal DFU bootloader
    // Write a flag to indicate new firmware is available
    // Then trigger a reset to bootloader
    
    // Mark update as pending for bootloader
    _header.state = OTA_STATE_IDLE;  // Clear after apply
    saveHeader();
    
    // The nRF52 bootloader checks for DFU flag in GPREGRET register
    // Setting bit 0 triggers DFU mode on reset
    NRF_POWER->GPREGRET = 0xB1;  // DFU magic value for Adafruit bootloader
    
    // Reset the device
    NVIC_SystemReset();
    
    // Does not return
}
