/**
 * @file firmware_crypto.cpp
 * @brief Firmware encryption implementation using AES-256-CTR
 * 
 * Key derivation: SHA-256(SECRET_SALT || FICR_DEVICE_ID)
 * Encryption: AES-256-CTR with offset-based IV
 */

#include "firmware_crypto.h"
#include "security.h"
#include "config.h"
#include <string.h>

// Use rweather/Crypto library for AES and SHA256
#include <AES.h>    // Must be before CTR.h
#include <CTR.h>
#include <SHA256.h>

/**
 * SECRET SALT - Change this for your production builds!
 * 
 * This is combined with the FICR device ID to derive the encryption key.
 * Keep this value secret and different from any public examples.
 * 
 * Recommended: Generate with: openssl rand -hex 32
 */
static const uint8_t SECRET_SALT[32] = {
    0x41, 0x67, 0x53, 0x79, 0x73, 0x43, 0x74, 0x72,  // "AgSysCtr"
    0x6C, 0x46, 0x57, 0x42, 0x61, 0x63, 0x6B, 0x75,  // "lFWBacku"
    0x70, 0x53, 0x61, 0x6C, 0x74, 0x32, 0x30, 0x32,  // "pSalt202"
    0x35, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // "5" + padding
    // TODO: Replace with random bytes for production!
};

// Derived encryption key (computed once at init)
static uint8_t s_encryptionKey[FW_CRYPTO_KEY_SIZE];
static bool s_initialized = false;

/**
 * @brief Derive encryption key from FICR + secret salt
 * 
 * Key = SHA-256(SECRET_SALT || DEVICE_ID)
 */
static void deriveKey(void) {
    uint8_t deviceId[8];
    uint8_t hashInput[32 + 8];  // salt + device ID
    
    // Get device ID from FICR
    security_getDeviceIdBytes(deviceId);
    
    // Concatenate salt + device ID
    memcpy(hashInput, SECRET_SALT, 32);
    memcpy(hashInput + 32, deviceId, 8);
    
    // SHA-256 to derive 32-byte key
    SHA256 sha;
    sha.reset();
    sha.update(hashInput, sizeof(hashInput));
    sha.finalize(s_encryptionKey, 32);
    
    // Clear sensitive data from stack
    memset(hashInput, 0, sizeof(hashInput));
    memset(deviceId, 0, sizeof(deviceId));
}

/**
 * @brief Generate IV from block offset
 * 
 * IV = SHA-256(key || offset)[0:16]
 * This allows random-access encryption/decryption.
 */
static void generateIV(uint32_t offset, uint8_t* iv) {
    uint8_t hashInput[FW_CRYPTO_KEY_SIZE + 4];
    uint8_t hash[32];
    
    // Concatenate key + offset
    memcpy(hashInput, s_encryptionKey, FW_CRYPTO_KEY_SIZE);
    hashInput[FW_CRYPTO_KEY_SIZE + 0] = (offset >> 24) & 0xFF;
    hashInput[FW_CRYPTO_KEY_SIZE + 1] = (offset >> 16) & 0xFF;
    hashInput[FW_CRYPTO_KEY_SIZE + 2] = (offset >> 8) & 0xFF;
    hashInput[FW_CRYPTO_KEY_SIZE + 3] = offset & 0xFF;
    
    SHA256 sha;
    sha.reset();
    sha.update(hashInput, sizeof(hashInput));
    sha.finalize(hash, 32);
    
    // Use first 16 bytes as IV
    memcpy(iv, hash, FW_CRYPTO_IV_SIZE);
    
    // Clear sensitive data
    memset(hashInput, 0, sizeof(hashInput));
    memset(hash, 0, sizeof(hash));
}

void fw_crypto_init(void) {
    if (s_initialized) {
        return;
    }
    
    deriveKey();
    s_initialized = true;
}

bool fw_crypto_encrypt(uint8_t* data, size_t length, uint32_t offset) {
    if (!s_initialized || data == NULL || (length % FW_CRYPTO_BLOCK_SIZE) != 0) {
        return false;
    }
    
    uint8_t iv[FW_CRYPTO_IV_SIZE];
    generateIV(offset, iv);
    
    // Software AES-256-CTR using Crypto library
    CTR<AES256> ctr;
    ctr.setKey(s_encryptionKey, FW_CRYPTO_KEY_SIZE);
    ctr.setIV(iv, FW_CRYPTO_IV_SIZE);
    ctr.setCounterSize(4);
    ctr.encrypt(data, data, length);
    
    memset(iv, 0, sizeof(iv));
    return true;
}

bool fw_crypto_decrypt(uint8_t* data, size_t length, uint32_t offset) {
    // CTR mode: encryption and decryption are identical
    return fw_crypto_encrypt(data, length, offset);
}

// Forward declarations for flash operations (implement in spi_flash.cpp)
extern bool spi_flash_write(uint32_t addr, const uint8_t* data, size_t length);
extern bool spi_flash_read(uint32_t addr, uint8_t* data, size_t length);

bool fw_crypto_write_chunk(uint32_t flash_addr, const uint8_t* data, size_t length) {
    if (!s_initialized || data == NULL || length == 0) {
        return false;
    }
    
    // Work buffer for encryption (process in 256-byte chunks to save RAM)
    uint8_t buffer[256];
    size_t remaining = length;
    uint32_t offset = 0;
    
    while (remaining > 0) {
        size_t chunk = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;
        
        // Pad to block size if needed
        size_t padded = ((chunk + FW_CRYPTO_BLOCK_SIZE - 1) / FW_CRYPTO_BLOCK_SIZE) 
                        * FW_CRYPTO_BLOCK_SIZE;
        
        // Copy and pad with zeros
        memcpy(buffer, data + offset, chunk);
        if (padded > chunk) {
            memset(buffer + chunk, 0, padded - chunk);
        }
        
        // Encrypt in place
        if (!fw_crypto_encrypt(buffer, padded, flash_addr + offset)) {
            return false;
        }
        
        // Write to flash
        if (!spi_flash_write(flash_addr + offset, buffer, padded)) {
            return false;
        }
        
        offset += chunk;
        remaining -= chunk;
    }
    
    memset(buffer, 0, sizeof(buffer));
    return true;
}

bool fw_crypto_read_chunk(uint32_t flash_addr, uint8_t* data, size_t length) {
    if (!s_initialized || data == NULL || length == 0) {
        return false;
    }
    
    // Work buffer for decryption
    uint8_t buffer[256];
    size_t remaining = length;
    uint32_t offset = 0;
    
    while (remaining > 0) {
        size_t chunk = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;
        size_t padded = ((chunk + FW_CRYPTO_BLOCK_SIZE - 1) / FW_CRYPTO_BLOCK_SIZE) 
                        * FW_CRYPTO_BLOCK_SIZE;
        
        // Read from flash
        if (!spi_flash_read(flash_addr + offset, buffer, padded)) {
            return false;
        }
        
        // Decrypt in place
        if (!fw_crypto_decrypt(buffer, padded, flash_addr + offset)) {
            return false;
        }
        
        // Copy to output
        memcpy(data + offset, buffer, chunk);
        
        offset += chunk;
        remaining -= chunk;
    }
    
    memset(buffer, 0, sizeof(buffer));
    return true;
}
