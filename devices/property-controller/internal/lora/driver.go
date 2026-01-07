// Package lora provides the LoRa communication driver for the RAK2245 Pi HAT.
// The RAK2245 uses the SX1301 concentrator chip which communicates via SPI.
package lora

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/binary"
	"fmt"
	"io"
	"log"
	"sync"
	"time"

	"github.com/agsys/property-controller/internal/protocol"
)

// Config holds LoRa radio configuration
type Config struct {
	Frequency       uint32 // Frequency in Hz (e.g., 915000000 for 915 MHz)
	SpreadingFactor uint8  // SF7-SF12
	Bandwidth       uint32 // Bandwidth in Hz (125000, 250000, 500000)
	CodingRate      uint8  // 5-8 (4/5 to 4/8)
	TxPower         int8   // Transmit power in dBm
	SyncWord        uint8  // Sync word for private network
	AESKey          []byte // 16-byte AES-128 key for encryption
}

// DefaultConfig returns default LoRa configuration for US 915 MHz
func DefaultConfig() Config {
	return Config{
		Frequency:       915000000,
		SpreadingFactor: 10,
		Bandwidth:       125000,
		CodingRate:      5,
		TxPower:         20,
		SyncWord:        0x34,
		AESKey:          nil, // Must be set by application
	}
}

// Driver handles LoRa communication via the RAK2245
type Driver struct {
	config   Config
	cipher   cipher.Block
	rxChan   chan *protocol.LoRaMessage
	txChan   chan *protocol.LoRaMessage
	stopChan chan struct{}
	wg       sync.WaitGroup
	mu       sync.Mutex
	running  bool
	seqNum   uint16

	// Callbacks
	onReceive func(*protocol.LoRaMessage)
}

// New creates a new LoRa driver
func New(config Config) (*Driver, error) {
	d := &Driver{
		config:   config,
		rxChan:   make(chan *protocol.LoRaMessage, 100),
		txChan:   make(chan *protocol.LoRaMessage, 100),
		stopChan: make(chan struct{}),
	}

	// Initialize AES cipher if key provided
	if len(config.AESKey) == 16 {
		block, err := aes.NewCipher(config.AESKey)
		if err != nil {
			return nil, fmt.Errorf("failed to create AES cipher: %w", err)
		}
		d.cipher = block
	}

	return d, nil
}

// Start initializes the LoRa hardware and starts the receive loop
func (d *Driver) Start() error {
	d.mu.Lock()
	if d.running {
		d.mu.Unlock()
		return fmt.Errorf("driver already running")
	}
	d.running = true
	d.mu.Unlock()

	// Initialize RAK2245 hardware
	if err := d.initHardware(); err != nil {
		return fmt.Errorf("failed to initialize hardware: %w", err)
	}

	// Start receive goroutine
	d.wg.Add(1)
	go d.receiveLoop()

	// Start transmit goroutine
	d.wg.Add(1)
	go d.transmitLoop()

	log.Printf("LoRa driver started: freq=%d Hz, SF=%d, BW=%d Hz",
		d.config.Frequency, d.config.SpreadingFactor, d.config.Bandwidth)

	return nil
}

// Stop stops the LoRa driver
func (d *Driver) Stop() error {
	d.mu.Lock()
	if !d.running {
		d.mu.Unlock()
		return nil
	}
	d.running = false
	d.mu.Unlock()

	close(d.stopChan)
	d.wg.Wait()

	return d.shutdownHardware()
}

// SetReceiveCallback sets the callback for received messages
func (d *Driver) SetReceiveCallback(cb func(*protocol.LoRaMessage)) {
	d.mu.Lock()
	d.onReceive = cb
	d.mu.Unlock()
}

// Send queues a message for transmission
func (d *Driver) Send(msg *protocol.LoRaMessage) error {
	d.mu.Lock()
	if !d.running {
		d.mu.Unlock()
		return fmt.Errorf("driver not running")
	}
	d.mu.Unlock()

	select {
	case d.txChan <- msg:
		return nil
	default:
		return fmt.Errorf("transmit queue full")
	}
}

// SendToDevice sends a message to a specific device
func (d *Driver) SendToDevice(deviceUID [8]byte, msgType uint8, payload []byte) error {
	d.mu.Lock()
	d.seqNum++
	seq := d.seqNum
	d.mu.Unlock()

	msg := &protocol.LoRaMessage{
		DeviceUID: deviceUID,
		MsgType:   msgType,
		SeqNum:    seq,
		Payload:   payload,
	}

	return d.Send(msg)
}

// Broadcast sends a message to all devices (uses broadcast UID)
func (d *Driver) Broadcast(msgType uint8, payload []byte) error {
	var broadcastUID [8]byte
	for i := range broadcastUID {
		broadcastUID[i] = 0xFF
	}
	return d.SendToDevice(broadcastUID, msgType, payload)
}

// initHardware initializes the RAK2245 SX1301 concentrator
func (d *Driver) initHardware() error {
	// The RAK2245 uses the Semtech SX1301 concentrator chip
	// Communication is via SPI on the Raspberry Pi
	//
	// For production, this would use the libloragw library or
	// communicate directly via SPI. For now, we'll use a stub
	// that can be replaced with actual hardware calls.
	//
	// The SX1301 initialization sequence:
	// 1. Reset the concentrator via GPIO
	// 2. Load firmware to the concentrator's MCU
	// 3. Configure radio parameters (frequency, SF, BW, etc.)
	// 4. Start the concentrator

	log.Println("Initializing RAK2245 hardware (stub)")

	// TODO: Implement actual SX1301 initialization
	// This would typically use CGO to call libloragw functions:
	// - lgw_board_setconf()
	// - lgw_rxrf_setconf()
	// - lgw_rxif_setconf()
	// - lgw_txgain_setconf()
	// - lgw_start()

	return nil
}

// shutdownHardware cleanly shuts down the LoRa hardware
func (d *Driver) shutdownHardware() error {
	log.Println("Shutting down RAK2245 hardware")
	// TODO: Call lgw_stop()
	return nil
}

// receiveLoop continuously receives LoRa packets
func (d *Driver) receiveLoop() {
	defer d.wg.Done()

	for {
		select {
		case <-d.stopChan:
			return
		default:
			// Poll for received packets
			// In production, this would call lgw_receive() or use interrupts
			msg, err := d.receivePacket()
			if err != nil {
				// No packet available or error
				time.Sleep(10 * time.Millisecond)
				continue
			}

			if msg != nil {
				// Decrypt if encryption enabled
				if d.cipher != nil && len(msg.Payload) > 0 {
					decrypted, err := d.decrypt(msg.Payload)
					if err != nil {
						log.Printf("Failed to decrypt message from %s: %v", msg.DeviceUIDString(), err)
						continue
					}
					msg.Payload = decrypted
				}

				msg.ReceivedAt = time.Now().Unix()

				// Call callback if set
				d.mu.Lock()
				cb := d.onReceive
				d.mu.Unlock()
				if cb != nil {
					cb(msg)
				}

				// Also send to channel
				select {
				case d.rxChan <- msg:
				default:
					log.Println("Receive queue full, dropping packet")
				}
			}
		}
	}
}

// transmitLoop handles outgoing packets
func (d *Driver) transmitLoop() {
	defer d.wg.Done()

	for {
		select {
		case <-d.stopChan:
			return
		case msg := <-d.txChan:
			// Encode message
			data := msg.Encode()

			// Encrypt if encryption enabled
			if d.cipher != nil {
				encrypted, err := d.encrypt(data)
				if err != nil {
					log.Printf("Failed to encrypt message: %v", err)
					continue
				}
				data = encrypted
			}

			// Transmit
			if err := d.transmitPacket(data); err != nil {
				log.Printf("Failed to transmit packet: %v", err)
			}

			// Small delay between transmissions
			time.Sleep(100 * time.Millisecond)
		}
	}
}

// receivePacket attempts to receive a LoRa packet
func (d *Driver) receivePacket() (*protocol.LoRaMessage, error) {
	// TODO: Implement actual packet reception via SX1301
	// This would call lgw_receive() and process the packet
	//
	// For now, return nil to indicate no packet
	return nil, nil
}

// transmitPacket transmits a LoRa packet
func (d *Driver) transmitPacket(data []byte) error {
	// TODO: Implement actual packet transmission via SX1301
	// This would:
	// 1. Create a lgw_pkt_tx_s structure
	// 2. Set frequency, modulation, SF, BW, power, etc.
	// 3. Copy payload data
	// 4. Call lgw_send()

	log.Printf("TX: %d bytes", len(data))
	return nil
}

// encrypt encrypts data using AES-128-CTR
func (d *Driver) encrypt(plaintext []byte) ([]byte, error) {
	if d.cipher == nil {
		return plaintext, nil
	}

	// Create IV
	iv := make([]byte, aes.BlockSize)
	if _, err := io.ReadFull(rand.Reader, iv); err != nil {
		return nil, err
	}

	// Encrypt
	ciphertext := make([]byte, aes.BlockSize+len(plaintext))
	copy(ciphertext[:aes.BlockSize], iv)

	stream := cipher.NewCTR(d.cipher, iv)
	stream.XORKeyStream(ciphertext[aes.BlockSize:], plaintext)

	return ciphertext, nil
}

// decrypt decrypts data using AES-128-CTR
func (d *Driver) decrypt(ciphertext []byte) ([]byte, error) {
	if d.cipher == nil {
		return ciphertext, nil
	}

	if len(ciphertext) < aes.BlockSize {
		return nil, fmt.Errorf("ciphertext too short")
	}

	iv := ciphertext[:aes.BlockSize]
	ciphertext = ciphertext[aes.BlockSize:]

	plaintext := make([]byte, len(ciphertext))
	stream := cipher.NewCTR(d.cipher, iv)
	stream.XORKeyStream(plaintext, ciphertext)

	return plaintext, nil
}

// GetNextSeqNum returns the next sequence number
func (d *Driver) GetNextSeqNum() uint16 {
	d.mu.Lock()
	defer d.mu.Unlock()
	d.seqNum++
	return d.seqNum
}

// ParseDeviceUID parses a hex string into a device UID
func ParseDeviceUID(s string) ([8]byte, error) {
	var uid [8]byte
	if len(s) != 16 {
		return uid, fmt.Errorf("invalid UID length: expected 16 hex chars, got %d", len(s))
	}

	for i := 0; i < 8; i++ {
		var b uint8
		_, err := fmt.Sscanf(s[i*2:i*2+2], "%02X", &b)
		if err != nil {
			return uid, fmt.Errorf("invalid hex at position %d: %w", i*2, err)
		}
		uid[i] = b
	}
	return uid, nil
}

// DeviceUIDToString converts a device UID to hex string
func DeviceUIDToString(uid [8]byte) string {
	return fmt.Sprintf("%02X%02X%02X%02X%02X%02X%02X%02X",
		uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7])
}

// CreateValveCommand creates a valve command message
func CreateValveCommand(controllerUID [8]byte, actuatorAddr uint8, command uint8, commandID uint16) *protocol.LoRaMessage {
	payload := &protocol.ValveCommandPayload{
		ActuatorAddr: actuatorAddr,
		Command:      command,
		CommandID:    commandID,
	}

	return &protocol.LoRaMessage{
		DeviceUID: controllerUID,
		MsgType:   protocol.MsgTypeValveCommand,
		Payload:   payload.Encode(),
	}
}

// CreateTimeSyncMessage creates a time sync message
func CreateTimeSyncMessage(utcOffset int8) *protocol.LoRaMessage {
	payload := &protocol.TimeSyncPayload{
		UnixTimestamp: uint32(time.Now().Unix()),
		UTCOffset:     utcOffset,
	}

	var broadcastUID [8]byte
	for i := range broadcastUID {
		broadcastUID[i] = 0xFF
	}

	return &protocol.LoRaMessage{
		DeviceUID: broadcastUID,
		MsgType:   protocol.MsgTypeTimeSync,
		Payload:   payload.Encode(),
	}
}

// CreateScheduleUpdateMessage creates a schedule update message
func CreateScheduleUpdateMessage(controllerUID [8]byte, version uint16, entries []protocol.ScheduleEntry) *protocol.LoRaMessage {
	payload := &protocol.ScheduleUpdatePayload{
		Version:    version,
		EntryCount: uint8(len(entries)),
		Entries:    entries,
	}

	return &protocol.LoRaMessage{
		DeviceUID: controllerUID,
		MsgType:   protocol.MsgTypeScheduleUpdate,
		Payload:   payload.Encode(),
	}
}

// Ensure binary is imported (used in protocol package)
var _ = binary.LittleEndian
