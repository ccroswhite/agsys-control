/**
 * @file agsys_lora.h
 * @brief AgSys LoRa Communication Layer
 * 
 * High-level API for sending and receiving LoRa packets using the AgSys protocol.
 * Handles encryption, header construction, and packet parsing.
 */

#ifndef AGSYS_LORA_H
#define AGSYS_LORA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "agsys_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the LoRa communication layer
 * 
 * @param deviceUid 8-byte device unique identifier
 * @param deviceType Device type (AGSYS_DEVICE_TYPE_*)
 * @return true on success, false on error
 */
bool agsys_lora_init(const uint8_t* deviceUid, uint8_t deviceType);

/**
 * @brief Build a packet header
 * 
 * @param header Pointer to header structure to fill
 * @param msgType Message type (AGSYS_MSG_*)
 */
void agsys_lora_buildHeader(AgsysHeader* header, uint8_t msgType);

/**
 * @brief Send a LoRa packet
 * 
 * Encrypts and transmits a packet with the given payload.
 * 
 * @param msgType Message type (AGSYS_MSG_*)
 * @param payload Payload data (NULL if no payload)
 * @param payloadLen Payload length
 * @return true on success, false on error
 */
bool agsys_lora_send(uint8_t msgType, const uint8_t* payload, size_t payloadLen);

/**
 * @brief Check if a packet is available
 * 
 * @return true if a packet is waiting to be received
 */
bool agsys_lora_available(void);

/**
 * @brief Receive a LoRa packet
 * 
 * Decrypts and parses an incoming packet.
 * 
 * @param header Output: parsed header
 * @param payload Output: payload buffer
 * @param payloadLen Input: max payload size, Output: actual payload length
 * @param rssi Output: received signal strength (optional, can be NULL)
 * @return true on success, false on error or no packet
 */
bool agsys_lora_receive(AgsysHeader* header, uint8_t* payload, size_t* payloadLen, int16_t* rssi);

/**
 * @brief Get the current sequence number
 * 
 * @return Current sequence number
 */
uint16_t agsys_lora_getSequence(void);

/**
 * @brief Set the LoRa radio parameters
 * 
 * @param frequency Frequency in Hz
 * @param spreadingFactor Spreading factor (7-12)
 * @param bandwidth Bandwidth in Hz
 * @param txPower Transmit power in dBm
 */
void agsys_lora_setRadioParams(uint32_t frequency, uint8_t spreadingFactor, 
                                uint32_t bandwidth, int8_t txPower);

/**
 * @brief Put LoRa radio into sleep mode
 */
void agsys_lora_sleep(void);

/**
 * @brief Wake LoRa radio from sleep
 */
void agsys_lora_wake(void);

/**
 * @brief Get last RSSI value
 * 
 * @return RSSI in dBm
 */
int16_t agsys_lora_getLastRssi(void);

/**
 * @brief Get last SNR value
 * 
 * @return SNR in dB * 4
 */
int8_t agsys_lora_getLastSnr(void);

#ifdef __cplusplus
}
#endif

#endif // AGSYS_LORA_H
