/**
 * @file agsys_crypto.cpp
 * @brief AgSys LoRa Packet Encryption Implementation
 * 
 * Uses rweather/Crypto library for AES-GCM implementation.
 */

#include "agsys_crypto.h"
#include <string.h>

// Crypto library includes
#include <AES.h>
#include <GCM.h>
#include <SHA256.h>

// Secret salt for key derivation
static const uint8_t AGSYS_SALT[16] = AGSYS_SECRET_SALT;

// Encryption key (derived at init)
static uint8_t s_key[AGSYS_CRYPTO_KEY_SIZE];

// Device UID (stored for reference)
static uint8_t s_deviceUid[AGSYS_DEVICE_UID_SIZE];

// Packet counter / nonce (must never repeat with same key)
static uint32_t s_nonce = 0;

// Initialization flag
static bool s_initialized = false;

// GCM cipher instance
static GCM<AES128> s_gcm;

void agsys_crypto_deriveKey(const uint8_t* deviceUid, uint8_t* keyOut) {
    uint8_t hashInput[16 + AGSYS_DEVICE_UID_SIZE];  // salt + device UID
    uint8_t hash[32];
    
    // Concatenate salt + device UID
    memcpy(hashInput, AGSYS_SALT, 16);
    memcpy(hashInput + 16, deviceUid, AGSYS_DEVICE_UID_SIZE);
    
    // SHA-256 and take first 16 bytes for AES-128 key
    SHA256 sha;
    sha.reset();
    sha.update(hashInput, sizeof(hashInput));
    sha.finalize(hash, 32);
    
    memcpy(keyOut, hash, AGSYS_CRYPTO_KEY_SIZE);
    
    // Clear sensitive data
    memset(hashInput, 0, sizeof(hashInput));
    memset(hash, 0, sizeof(hash));
}

void agsys_crypto_init(const uint8_t* deviceUid) {
    if (s_initialized) {
        return;
    }
    
    // Store device UID
    memcpy(s_deviceUid, deviceUid, AGSYS_DEVICE_UID_SIZE);
    
    // Derive key from salt + device UID
    agsys_crypto_deriveKey(deviceUid, s_key);
    
    // Initialize nonce from a persistent source if available
    // For now, start at 0 - in production, load from NVRAM
    s_nonce = 0;
    
    s_initialized = true;
}

void agsys_crypto_setKey(const uint8_t* key) {
    memcpy(s_key, key, AGSYS_CRYPTO_KEY_SIZE);
    s_initialized = true;
}

bool agsys_crypto_encrypt(const uint8_t* plaintext, size_t plaintextLen,
                          uint8_t* packetOut, size_t* packetLen) {
    if (!s_initialized || plaintext == NULL || packetOut == NULL || packetLen == NULL) {
        return false;
    }
    
    if (plaintextLen > AGSYS_MAX_PLAINTEXT) {
        return false;
    }
    
    // Build full 12-byte nonce for GCM (standard size)
    // Format: [0x00 x 8][nonce:4] - padding + counter
    uint8_t fullNonce[12];
    memset(fullNonce, 0, 8);
    fullNonce[8]  = (s_nonce >> 24) & 0xFF;
    fullNonce[9]  = (s_nonce >> 16) & 0xFF;
    fullNonce[10] = (s_nonce >> 8) & 0xFF;
    fullNonce[11] = s_nonce & 0xFF;
    
    // Setup GCM
    s_gcm.setKey(s_key, AGSYS_CRYPTO_KEY_SIZE);
    s_gcm.setIV(fullNonce, 12);
    
    // Add device UID as associated data (authenticated but not encrypted)
    s_gcm.addAuthData(s_deviceUid, AGSYS_DEVICE_UID_SIZE);
    
    // Write truncated nonce to output
    packetOut[0] = fullNonce[8];
    packetOut[1] = fullNonce[9];
    packetOut[2] = fullNonce[10];
    packetOut[3] = fullNonce[11];
    
    // Encrypt plaintext
    s_gcm.encrypt(packetOut + AGSYS_CRYPTO_NONCE_SIZE, plaintext, plaintextLen);
    
    // Get auth tag (full 16 bytes, then truncate)
    uint8_t fullTag[16];
    s_gcm.computeTag(fullTag, 16);
    
    // Append truncated tag
    memcpy(packetOut + AGSYS_CRYPTO_NONCE_SIZE + plaintextLen, fullTag, AGSYS_CRYPTO_TAG_SIZE);
    
    *packetLen = AGSYS_CRYPTO_NONCE_SIZE + plaintextLen + AGSYS_CRYPTO_TAG_SIZE;
    
    // Increment nonce for next packet
    s_nonce++;
    
    // Clear sensitive data
    s_gcm.clear();
    memset(fullTag, 0, sizeof(fullTag));
    
    return true;
}

bool agsys_crypto_decrypt(const uint8_t* packet, size_t packetLen,
                          uint8_t* plaintextOut, size_t* plaintextLen) {
    return agsys_crypto_decryptWithKey(s_key, packet, packetLen, plaintextOut, plaintextLen);
}

bool agsys_crypto_decryptWithKey(const uint8_t* key,
                                  const uint8_t* packet, size_t packetLen,
                                  uint8_t* plaintextOut, size_t* plaintextLen) {
    if (key == NULL || packet == NULL || plaintextOut == NULL || plaintextLen == NULL) {
        return false;
    }
    
    // Minimum packet size: nonce + tag (no payload)
    if (packetLen < AGSYS_CRYPTO_OVERHEAD) {
        return false;
    }
    
    size_t ciphertextLen = packetLen - AGSYS_CRYPTO_OVERHEAD;
    
    // Extract nonce from packet
    uint8_t fullNonce[12];
    memset(fullNonce, 0, 8);
    fullNonce[8]  = packet[0];
    fullNonce[9]  = packet[1];
    fullNonce[10] = packet[2];
    fullNonce[11] = packet[3];
    
    // Setup GCM for decryption
    GCM<AES128> gcm;
    gcm.setKey(key, AGSYS_CRYPTO_KEY_SIZE);
    gcm.setIV(fullNonce, 12);
    
    // For property controller decryption, we need to extract device UID from
    // the decrypted header. But we can't do that until we decrypt...
    // Solution: Don't use AAD for property controller, or pass device UID separately.
    // For now, skip AAD in decryptWithKey (property controller case)
    
    // Decrypt ciphertext
    gcm.decrypt(plaintextOut, packet + AGSYS_CRYPTO_NONCE_SIZE, ciphertextLen);
    
    // Verify auth tag
    uint8_t computedTag[16];
    gcm.computeTag(computedTag, 16);
    
    // Compare truncated tags (constant-time comparison)
    const uint8_t* receivedTag = packet + AGSYS_CRYPTO_NONCE_SIZE + ciphertextLen;
    uint8_t diff = 0;
    for (int i = 0; i < AGSYS_CRYPTO_TAG_SIZE; i++) {
        diff |= computedTag[i] ^ receivedTag[i];
    }
    
    // Clear sensitive data
    gcm.clear();
    memset(computedTag, 0, sizeof(computedTag));
    
    if (diff != 0) {
        // Auth failed - clear output
        memset(plaintextOut, 0, ciphertextLen);
        *plaintextLen = 0;
        return false;
    }
    
    *plaintextLen = ciphertextLen;
    return true;
}

bool agsys_crypto_encryptWithKey(const uint8_t* key, uint32_t nonce,
                                  const uint8_t* plaintext, size_t plaintextLen,
                                  uint8_t* packetOut, size_t* packetLen) {
    if (key == NULL || plaintext == NULL || packetOut == NULL || packetLen == NULL) {
        return false;
    }
    
    if (plaintextLen > AGSYS_MAX_PLAINTEXT) {
        return false;
    }
    
    // Build full 12-byte nonce for GCM
    uint8_t fullNonce[12];
    memset(fullNonce, 0, 8);
    fullNonce[8]  = (nonce >> 24) & 0xFF;
    fullNonce[9]  = (nonce >> 16) & 0xFF;
    fullNonce[10] = (nonce >> 8) & 0xFF;
    fullNonce[11] = nonce & 0xFF;
    
    // Setup GCM
    GCM<AES128> gcm;
    gcm.setKey(key, AGSYS_CRYPTO_KEY_SIZE);
    gcm.setIV(fullNonce, 12);
    
    // Write truncated nonce to output
    packetOut[0] = fullNonce[8];
    packetOut[1] = fullNonce[9];
    packetOut[2] = fullNonce[10];
    packetOut[3] = fullNonce[11];
    
    // Encrypt plaintext
    gcm.encrypt(packetOut + AGSYS_CRYPTO_NONCE_SIZE, plaintext, plaintextLen);
    
    // Get auth tag
    uint8_t fullTag[16];
    gcm.computeTag(fullTag, 16);
    
    // Append truncated tag
    memcpy(packetOut + AGSYS_CRYPTO_NONCE_SIZE + plaintextLen, fullTag, AGSYS_CRYPTO_TAG_SIZE);
    
    *packetLen = AGSYS_CRYPTO_NONCE_SIZE + plaintextLen + AGSYS_CRYPTO_TAG_SIZE;
    
    // Clear sensitive data
    gcm.clear();
    memset(fullTag, 0, sizeof(fullTag));
    
    return true;
}

uint32_t agsys_crypto_getNonce(void) {
    return s_nonce;
}

void agsys_crypto_setNonce(uint32_t nonce) {
    s_nonce = nonce;
}

uint32_t agsys_crypto_nextNonce(void) {
    // In production, this should persist to NVRAM before returning
    return ++s_nonce;
}
