/**
 * @file lora_crypto.cpp
 * @brief LoRa packet encryption using AES-128-GCM
 * 
 * Uses rweather/Crypto library for AES-GCM implementation.
 */

#include "lora_crypto.h"
#include "security.h"
#include <string.h>

// Crypto library includes
#include <AES.h>
#include <GCM.h>
#include <SHA256.h>

// Secret salt for key derivation (same concept as firmware_crypto)
// TODO: Change this for production!
static const uint8_t LORA_SECRET_SALT[16] = {
    0x4C, 0x6F, 0x52, 0x61, 0x43, 0x72, 0x79, 0x70,  // "LoRaCryp"
    0x74, 0x6F, 0x53, 0x61, 0x6C, 0x74, 0x21, 0x00   // "toSalt!"
};

// Encryption key (derived at init)
static uint8_t s_key[LORA_CRYPTO_KEY_SIZE];

// Packet counter / nonce (must never repeat with same key)
static uint32_t s_nonce = 0;

// Initialization flag
static bool s_initialized = false;

// GCM cipher instance
static GCM<AES128> s_gcm;

/**
 * @brief Derive key from device ID and salt
 */
static void deriveKey(void) {
    uint8_t deviceId[8];
    uint8_t hashInput[16 + 8];  // salt + device ID
    uint8_t hash[32];
    
    // Get device ID from FICR
    security_getDeviceIdBytes(deviceId);
    
    // Concatenate salt + device ID
    memcpy(hashInput, LORA_SECRET_SALT, 16);
    memcpy(hashInput + 16, deviceId, 8);
    
    // SHA-256 and take first 16 bytes for AES-128 key
    SHA256 sha;
    sha.reset();
    sha.update(hashInput, sizeof(hashInput));
    sha.finalize(hash, 32);
    
    memcpy(s_key, hash, LORA_CRYPTO_KEY_SIZE);
    
    // Clear sensitive data
    memset(hashInput, 0, sizeof(hashInput));
    memset(hash, 0, sizeof(hash));
    memset(deviceId, 0, sizeof(deviceId));
}

void lora_crypto_init(void) {
    if (s_initialized) {
        return;
    }
    
    deriveKey();
    
    // Initialize nonce from a persistent source if available
    // For now, start at 0 - in production, load from NVRAM
    s_nonce = 0;
    
    s_initialized = true;
}

void lora_crypto_setKey(const uint8_t* key) {
    memcpy(s_key, key, LORA_CRYPTO_KEY_SIZE);
    s_initialized = true;
}

bool lora_crypto_encrypt(const uint8_t* plaintext, size_t plaintext_len,
                         uint8_t* packet_out, size_t* packet_len) {
    if (!s_initialized || plaintext == NULL || packet_out == NULL || packet_len == NULL) {
        return false;
    }
    
    if (plaintext_len > LORA_MAX_PLAINTEXT) {
        return false;
    }
    
    // Build full 12-byte nonce for GCM (standard size)
    // Format: [0x00 x 8][nonce:4] - padding + counter
    uint8_t full_nonce[12];
    memset(full_nonce, 0, 8);
    full_nonce[8]  = (s_nonce >> 24) & 0xFF;
    full_nonce[9]  = (s_nonce >> 16) & 0xFF;
    full_nonce[10] = (s_nonce >> 8) & 0xFF;
    full_nonce[11] = s_nonce & 0xFF;
    
    // Get device ID for associated data (binds packet to this device)
    uint8_t deviceId[8];
    security_getDeviceIdBytes(deviceId);
    
    // Setup GCM
    s_gcm.setKey(s_key, LORA_CRYPTO_KEY_SIZE);
    s_gcm.setIV(full_nonce, 12);
    
    // Add device ID as associated data (authenticated but not encrypted)
    s_gcm.addAuthData(deviceId, 8);
    
    // Write truncated nonce to output
    packet_out[0] = full_nonce[8];
    packet_out[1] = full_nonce[9];
    packet_out[2] = full_nonce[10];
    packet_out[3] = full_nonce[11];
    
    // Encrypt plaintext
    s_gcm.encrypt(packet_out + LORA_CRYPTO_NONCE_SIZE, plaintext, plaintext_len);
    
    // Get auth tag (full 16 bytes, then truncate)
    uint8_t full_tag[16];
    s_gcm.computeTag(full_tag, 16);
    
    // Append truncated tag
    memcpy(packet_out + LORA_CRYPTO_NONCE_SIZE + plaintext_len, full_tag, LORA_CRYPTO_TAG_SIZE);
    
    *packet_len = LORA_CRYPTO_NONCE_SIZE + plaintext_len + LORA_CRYPTO_TAG_SIZE;
    
    // Increment nonce for next packet
    s_nonce++;
    
    // Clear sensitive data
    s_gcm.clear();
    memset(full_tag, 0, sizeof(full_tag));
    memset(deviceId, 0, sizeof(deviceId));
    
    return true;
}

bool lora_crypto_decrypt(const uint8_t* packet, size_t packet_len,
                         uint8_t* plaintext_out, size_t* plaintext_len) {
    if (!s_initialized || packet == NULL || plaintext_out == NULL || plaintext_len == NULL) {
        return false;
    }
    
    // Minimum packet size: nonce + tag (no payload)
    if (packet_len < LORA_CRYPTO_OVERHEAD) {
        return false;
    }
    
    size_t ciphertext_len = packet_len - LORA_CRYPTO_OVERHEAD;
    
    // Extract nonce from packet
    uint8_t full_nonce[12];
    memset(full_nonce, 0, 8);
    full_nonce[8]  = packet[0];
    full_nonce[9]  = packet[1];
    full_nonce[10] = packet[2];
    full_nonce[11] = packet[3];
    
    // Get device ID for associated data verification
    uint8_t deviceId[8];
    security_getDeviceIdBytes(deviceId);
    
    // Setup GCM for decryption
    s_gcm.setKey(s_key, LORA_CRYPTO_KEY_SIZE);
    s_gcm.setIV(full_nonce, 12);
    s_gcm.addAuthData(deviceId, 8);
    
    // Decrypt ciphertext
    s_gcm.decrypt(plaintext_out, packet + LORA_CRYPTO_NONCE_SIZE, ciphertext_len);
    
    // Verify auth tag
    uint8_t computed_tag[16];
    s_gcm.computeTag(computed_tag, 16);
    
    // Compare truncated tags (constant-time comparison)
    const uint8_t* received_tag = packet + LORA_CRYPTO_NONCE_SIZE + ciphertext_len;
    uint8_t diff = 0;
    for (int i = 0; i < LORA_CRYPTO_TAG_SIZE; i++) {
        diff |= computed_tag[i] ^ received_tag[i];
    }
    
    // Clear sensitive data
    s_gcm.clear();
    memset(computed_tag, 0, sizeof(computed_tag));
    memset(deviceId, 0, sizeof(deviceId));
    
    if (diff != 0) {
        // Auth failed - clear output
        memset(plaintext_out, 0, ciphertext_len);
        *plaintext_len = 0;
        return false;
    }
    
    *plaintext_len = ciphertext_len;
    return true;
}

uint32_t lora_crypto_getNonce(void) {
    return s_nonce;
}

void lora_crypto_setNonce(uint32_t nonce) {
    s_nonce = nonce;
}
