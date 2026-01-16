/**
 * @file bl_ed25519.h
 * @brief Ed25519 Signature Verification for Bootloader
 * 
 * Minimal Ed25519 verify-only implementation for firmware signature checking.
 * Based on the ref10 reference implementation, optimized for embedded use.
 * 
 * Features:
 * - Verify-only (no signing capability in bootloader)
 * - ~4KB code size
 * - No dynamic memory allocation
 * - Constant-time operations where security-critical
 */

#ifndef BL_ED25519_H
#define BL_ED25519_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BL_ED25519_PUBLIC_KEY_SIZE  32
#define BL_ED25519_SIGNATURE_SIZE   64

/**
 * @brief Verify an Ed25519 signature
 * 
 * @param signature 64-byte signature
 * @param message Message that was signed
 * @param message_len Length of message in bytes
 * @param public_key 32-byte public key
 * @return true if signature is valid, false otherwise
 */
bool bl_ed25519_verify(
    const uint8_t signature[BL_ED25519_SIGNATURE_SIZE],
    const uint8_t *message,
    size_t message_len,
    const uint8_t public_key[BL_ED25519_PUBLIC_KEY_SIZE]
);

/**
 * @brief Verify firmware signature
 * 
 * Convenience function that verifies a firmware binary against its signature
 * using the embedded public key.
 * 
 * @param firmware Pointer to firmware data
 * @param firmware_size Size of firmware in bytes
 * @param signature 64-byte Ed25519 signature
 * @return true if signature is valid, false otherwise
 */
bool bl_verify_firmware_signature(
    const uint8_t *firmware,
    size_t firmware_size,
    const uint8_t signature[BL_ED25519_SIGNATURE_SIZE]
);

#ifdef __cplusplus
}
#endif

#endif /* BL_ED25519_H */
