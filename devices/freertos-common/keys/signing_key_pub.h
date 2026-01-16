/**
 * AgSys Firmware Signing Public Key
 * 
 * Generated: 2026-01-16 11:33:57
 * Algorithm: Ed25519
 * 
 * WARNING: Do not modify this file manually!
 * Regenerate with: python3 generate_signing_key.py
 */

#ifndef AGSYS_SIGNING_KEY_PUB_H
#define AGSYS_SIGNING_KEY_PUB_H

#include <stdint.h>

#define AGSYS_ED25519_PUBLIC_KEY_SIZE 32
#define AGSYS_ED25519_SIGNATURE_SIZE  64

/**
 * Ed25519 public key for firmware signature verification.
 * This key is embedded in the bootloader (read-only).
 */
static const uint8_t agsys_signing_public_key[AGSYS_ED25519_PUBLIC_KEY_SIZE] = {
    0x24, 0x93, 0x90, 0xcf, 0xe8, 0xc2, 0xa7, 0x27, 0xf6, 0x68, 0x76, 0x05, 0x76, 0xa1, 0x52, 0xf3,
    0x3b, 0x83, 0x26, 0x70, 0x98, 0x31, 0xb2, 0x52, 0x36, 0x35, 0x04, 0xf1, 0x9a, 0x3b, 0xeb, 0x2a
};

#endif /* AGSYS_SIGNING_KEY_PUB_H */
