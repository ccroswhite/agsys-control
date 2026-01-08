// Package lora provides LoRa communication utilities.
// This file implements AES-128-GCM encryption with per-device key derivation.
package lora

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/sha256"
	"encoding/binary"
	"fmt"
)

// Crypto constants matching the device firmware
const (
	CryptoKeySize   = 16                              // AES-128
	CryptoNonceSize = 4                               // Truncated nonce (counter)
	CryptoTagSize   = 4                               // Truncated auth tag
	CryptoOverhead  = CryptoNonceSize + CryptoTagSize // 8 bytes
	DeviceUIDSize   = 8
)

// SecretSalt is the shared salt for key derivation.
// WARNING: This must match the salt in devices/common/include/agsys_protocol.h
// Change this for production deployments!
var SecretSalt = []byte{
	0x41, 0x67, 0x53, 0x79, 0x73, 0x4C, 0x6F, 0x52,
	0x61, 0x53, 0x61, 0x6C, 0x74, 0x32, 0x30, 0x32,
} // "AgSysLoRaSalt202"

// DeviceKeyCache caches derived keys for devices
type DeviceKeyCache struct {
	keys map[[DeviceUIDSize]byte][]byte
}

// NewDeviceKeyCache creates a new key cache
func NewDeviceKeyCache() *DeviceKeyCache {
	return &DeviceKeyCache{
		keys: make(map[[DeviceUIDSize]byte][]byte),
	}
}

// DeriveKey derives an AES-128 key for a specific device.
// Key = SHA-256(SECRET_SALT || DEVICE_UID)[0:16]
func DeriveKey(deviceUID [DeviceUIDSize]byte) []byte {
	// Concatenate salt + device UID
	hashInput := make([]byte, len(SecretSalt)+DeviceUIDSize)
	copy(hashInput[:len(SecretSalt)], SecretSalt)
	copy(hashInput[len(SecretSalt):], deviceUID[:])

	// SHA-256 and take first 16 bytes
	hash := sha256.Sum256(hashInput)
	key := make([]byte, CryptoKeySize)
	copy(key, hash[:CryptoKeySize])

	return key
}

// GetKey returns the key for a device, deriving and caching if needed
func (c *DeviceKeyCache) GetKey(deviceUID [DeviceUIDSize]byte) []byte {
	if key, ok := c.keys[deviceUID]; ok {
		return key
	}

	key := DeriveKey(deviceUID)
	c.keys[deviceUID] = key
	return key
}

// EncryptGCM encrypts data using AES-128-GCM with a 4-byte nonce.
// Output format: [Nonce:4][Ciphertext:N][Tag:4]
func EncryptGCM(key []byte, nonce uint32, plaintext []byte) ([]byte, error) {
	if len(key) != CryptoKeySize {
		return nil, fmt.Errorf("invalid key size: %d", len(key))
	}

	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, fmt.Errorf("failed to create cipher: %w", err)
	}

	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return nil, fmt.Errorf("failed to create GCM: %w", err)
	}

	// Build full 12-byte nonce for GCM (standard size)
	// Format: [0x00 x 8][nonce:4] - padding + counter
	fullNonce := make([]byte, 12)
	fullNonce[8] = byte(nonce >> 24)
	fullNonce[9] = byte(nonce >> 16)
	fullNonce[10] = byte(nonce >> 8)
	fullNonce[11] = byte(nonce)

	// Encrypt
	ciphertext := gcm.Seal(nil, fullNonce, plaintext, nil)

	// Build output: [Nonce:4][Ciphertext][Tag]
	// GCM appends the full 16-byte tag, we need to truncate to 4 bytes
	if len(ciphertext) < 16 {
		return nil, fmt.Errorf("ciphertext too short")
	}

	// ciphertext = encrypted_data + 16-byte tag
	encryptedData := ciphertext[:len(ciphertext)-16]
	fullTag := ciphertext[len(ciphertext)-16:]

	// Build output with truncated nonce and tag
	output := make([]byte, CryptoNonceSize+len(encryptedData)+CryptoTagSize)
	output[0] = fullNonce[8]
	output[1] = fullNonce[9]
	output[2] = fullNonce[10]
	output[3] = fullNonce[11]
	copy(output[CryptoNonceSize:], encryptedData)
	copy(output[CryptoNonceSize+len(encryptedData):], fullTag[:CryptoTagSize])

	return output, nil
}

// DecryptGCM decrypts data using AES-128-GCM with a 4-byte nonce.
// Input format: [Nonce:4][Ciphertext:N][Tag:4]
// Note: With truncated tag, we verify only 4 bytes of the authentication tag.
func DecryptGCM(key []byte, packet []byte) ([]byte, error) {
	if len(key) != CryptoKeySize {
		return nil, fmt.Errorf("invalid key size: %d", len(key))
	}

	if len(packet) < CryptoOverhead {
		return nil, fmt.Errorf("packet too short: %d", len(packet))
	}

	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, fmt.Errorf("failed to create cipher: %w", err)
	}

	// Extract nonce from packet
	fullNonce := make([]byte, 12)
	fullNonce[8] = packet[0]
	fullNonce[9] = packet[1]
	fullNonce[10] = packet[2]
	fullNonce[11] = packet[3]

	// Extract ciphertext and truncated tag
	ciphertextLen := len(packet) - CryptoOverhead
	encryptedData := packet[CryptoNonceSize : CryptoNonceSize+ciphertextLen]
	truncatedTag := packet[CryptoNonceSize+ciphertextLen:]

	// Use CTR mode for decryption (GCM uses CTR internally)
	ctr := cipher.NewCTR(block, buildGCMCounter(fullNonce))
	plaintext := make([]byte, len(encryptedData))
	ctr.XORKeyStream(plaintext, encryptedData)

	// Compute tag and verify truncated portion
	computedTag := computeGCMTag(block, fullNonce, nil, encryptedData)
	if computedTag == nil {
		return nil, fmt.Errorf("failed to compute tag")
	}

	// Constant-time comparison of truncated tag
	diff := byte(0)
	for i := 0; i < CryptoTagSize; i++ {
		diff |= computedTag[i] ^ truncatedTag[i]
	}

	if diff != 0 {
		return nil, fmt.Errorf("authentication failed")
	}

	return plaintext, nil
}

// buildGCMCounter builds the initial counter block for GCM
func buildGCMCounter(nonce []byte) []byte {
	counter := make([]byte, 16)
	copy(counter, nonce)
	counter[15] = 2 // GCM starts counter at 2 for encryption
	return counter
}

// computeGCMTag computes the GCM authentication tag
// This is a simplified implementation for tag verification
func computeGCMTag(block cipher.Block, nonce, aad, ciphertext []byte) []byte {
	// For proper GCM tag computation, we need GHASH
	// This is a simplified version that works with our use case

	// Create GCM instance to compute tag
	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return nil
	}

	// Encrypt empty plaintext to get just the tag
	// Then XOR with our ciphertext's contribution
	// This is a workaround - in production, implement proper GHASH

	// For now, use a simpler approach: re-encrypt and compare
	// This works because we have the key and nonce

	// Actually, let's use the standard GCM Seal with the plaintext we decrypted
	// to verify the tag matches

	// Decrypt first (CTR mode)
	ctr := cipher.NewCTR(block, buildGCMCounter(nonce))
	plaintext := make([]byte, len(ciphertext))
	ctr.XORKeyStream(plaintext, ciphertext)

	// Re-encrypt with GCM to get the tag
	result := gcm.Seal(nil, nonce, plaintext, aad)

	// Extract the tag (last 16 bytes)
	if len(result) >= 16 {
		return result[len(result)-16:]
	}
	return nil
}

// ExtractNonce extracts the 4-byte nonce from an encrypted packet
func ExtractNonce(packet []byte) (uint32, error) {
	if len(packet) < CryptoNonceSize {
		return 0, fmt.Errorf("packet too short")
	}
	return binary.BigEndian.Uint32(packet[:CryptoNonceSize]), nil
}
