// Package lora provides LoRa communication via ChirpStack Concentratord.
// This driver communicates with Concentratord over ZeroMQ sockets.
package lora

import (
	"context"
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/binary"
	"fmt"
	"io"
	"log"
	"sync"
	"time"

	"github.com/agsys/property-controller/internal/lora/gw"
	"github.com/agsys/property-controller/internal/protocol"
	"github.com/go-zeromq/zmq4"
)

// ConcentratordConfig holds configuration for the Concentratord connection
type ConcentratordConfig struct {
	EventURL        string // SUB socket for receiving events
	CommandURL      string // REQ socket for sending commands
	Frequency       uint32 // Frequency in Hz
	SpreadingFactor uint32 // SF7-SF12
	Bandwidth       uint32 // Bandwidth in Hz
	CodingRate      string // "4/5", "4/6", "4/7", "4/8"
	TxPower         int32  // Transmit power in dBm
	AESKey          []byte // 16-byte AES-128 key
}

// DefaultConcentratordConfig returns default configuration
func DefaultConcentratordConfig() ConcentratordConfig {
	return ConcentratordConfig{
		EventURL:        "ipc:///tmp/concentratord_event",
		CommandURL:      "ipc:///tmp/concentratord_command",
		Frequency:       915000000,
		SpreadingFactor: 10,
		Bandwidth:       125000,
		CodingRate:      "4/5",
		TxPower:         20,
	}
}

// ConcentratordDriver handles LoRa communication via ChirpStack Concentratord
type ConcentratordDriver struct {
	config     ConcentratordConfig
	cipher     cipher.Block
	keyCache   *DeviceKeyCache
	txNonce    uint32
	eventSock  zmq4.Socket
	cmdSock    zmq4.Socket
	ctx        context.Context
	cancel     context.CancelFunc
	wg         sync.WaitGroup
	mu         sync.Mutex
	running    bool
	seqNum     uint16
	gatewayID  string
	downlinkID uint32
	onReceive  func(*protocol.LoRaMessage)
}

// NewConcentratordDriver creates a new Concentratord driver
func NewConcentratordDriver(config ConcentratordConfig) (*ConcentratordDriver, error) {
	ctx, cancel := context.WithCancel(context.Background())

	d := &ConcentratordDriver{
		config:   config,
		ctx:      ctx,
		cancel:   cancel,
		keyCache: NewDeviceKeyCache(),
	}

	// Legacy: support single shared key if provided (for backward compatibility)
	if len(config.AESKey) == 16 {
		block, err := aes.NewCipher(config.AESKey)
		if err != nil {
			cancel()
			return nil, fmt.Errorf("failed to create AES cipher: %w", err)
		}
		d.cipher = block
	}

	return d, nil
}

// Start connects to Concentratord and starts the event loop
func (d *ConcentratordDriver) Start() error {
	d.mu.Lock()
	if d.running {
		d.mu.Unlock()
		return fmt.Errorf("driver already running")
	}
	d.running = true
	d.mu.Unlock()

	d.eventSock = zmq4.NewSub(d.ctx)
	if err := d.eventSock.Dial(d.config.EventURL); err != nil {
		return fmt.Errorf("failed to connect event socket: %w", err)
	}
	if err := d.eventSock.SetOption(zmq4.OptionSubscribe, ""); err != nil {
		return fmt.Errorf("failed to subscribe: %w", err)
	}

	d.cmdSock = zmq4.NewReq(d.ctx)
	if err := d.cmdSock.Dial(d.config.CommandURL); err != nil {
		d.eventSock.Close()
		return fmt.Errorf("failed to connect command socket: %w", err)
	}

	if err := d.fetchGatewayID(); err != nil {
		log.Printf("Warning: failed to get gateway ID: %v", err)
	}

	d.wg.Add(1)
	go d.eventLoop()

	log.Printf("Concentratord driver started: event=%s, cmd=%s, gateway=%s",
		d.config.EventURL, d.config.CommandURL, d.gatewayID)

	return nil
}

// Stop stops the driver and closes connections
func (d *ConcentratordDriver) Stop() error {
	d.mu.Lock()
	if !d.running {
		d.mu.Unlock()
		return nil
	}
	d.running = false
	d.mu.Unlock()

	d.cancel()
	d.wg.Wait()

	if d.eventSock != nil {
		d.eventSock.Close()
	}
	if d.cmdSock != nil {
		d.cmdSock.Close()
	}

	log.Println("Concentratord driver stopped")
	return nil
}

// SetReceiveCallback sets the callback for received messages
func (d *ConcentratordDriver) SetReceiveCallback(cb func(*protocol.LoRaMessage)) {
	d.mu.Lock()
	d.onReceive = cb
	d.mu.Unlock()
}

// Send transmits a LoRa message
func (d *ConcentratordDriver) Send(msg *protocol.LoRaMessage) error {
	d.mu.Lock()
	if !d.running {
		d.mu.Unlock()
		return fmt.Errorf("driver not running")
	}
	d.mu.Unlock()

	data := msg.Encode()

	if d.cipher != nil {
		encrypted, err := d.encrypt(data)
		if err != nil {
			return fmt.Errorf("encryption failed: %w", err)
		}
		data = encrypted
	}

	return d.sendDownlink(data)
}

// SendToDevice sends a message to a specific device
func (d *ConcentratordDriver) SendToDevice(deviceUID [8]byte, msgType uint8, payload []byte) error {
	d.mu.Lock()
	d.seqNum++
	seq := d.seqNum
	d.mu.Unlock()

	msg := &protocol.LoRaMessage{
		Header: protocol.Header{
			Magic:      [2]byte{protocol.MagicByte1, protocol.MagicByte2},
			Version:    protocol.ProtocolVersion,
			MsgType:    msgType,
			DeviceType: 0, // Controller doesn't have a device type
			DeviceUID:  deviceUID,
			Sequence:   seq,
		},
		Payload: payload,
	}

	return d.Send(msg)
}

// Broadcast sends a message to all devices
func (d *ConcentratordDriver) Broadcast(msgType uint8, payload []byte) error {
	var broadcastUID [8]byte
	for i := range broadcastUID {
		broadcastUID[i] = 0xFF
	}
	return d.SendToDevice(broadcastUID, msgType, payload)
}

// GetNextSeqNum returns the next sequence number
func (d *ConcentratordDriver) GetNextSeqNum() uint16 {
	d.mu.Lock()
	defer d.mu.Unlock()
	d.seqNum++
	return d.seqNum
}

// fetchGatewayID retrieves the gateway ID from Concentratord
func (d *ConcentratordDriver) fetchGatewayID() error {
	msg := zmq4.NewMsgFrom([]byte("gateway_id"), []byte{})
	if err := d.cmdSock.Send(msg); err != nil {
		return fmt.Errorf("failed to send command: %w", err)
	}

	resp, err := d.cmdSock.Recv()
	if err != nil {
		return fmt.Errorf("failed to receive response: %w", err)
	}

	if len(resp.Frames) > 0 && len(resp.Frames[0]) >= 8 {
		gwResp, err := gw.UnmarshalGetGatewayIdResponse(resp.Frames[0])
		if err != nil {
			return err
		}
		d.gatewayID = gwResp.GatewayId
	}

	return nil
}

// sendDownlink sends a downlink frame via Concentratord
func (d *ConcentratordDriver) sendDownlink(payload []byte) error {
	d.mu.Lock()
	d.downlinkID++
	dlID := d.downlinkID
	d.mu.Unlock()

	codeRate := gw.CodeRate_CR_4_5
	switch d.config.CodingRate {
	case "4/6":
		codeRate = gw.CodeRate_CR_4_6
	case "4/7":
		codeRate = gw.CodeRate_CR_4_7
	case "4/8":
		codeRate = gw.CodeRate_CR_4_8
	}

	downlink := &gw.DownlinkFrame{
		DownlinkId: dlID,
		GatewayId:  d.gatewayID,
		Items: []*gw.DownlinkFrameItem{
			{
				PhyPayload: payload,
				TxInfo: &gw.DownlinkTxInfo{
					Frequency: d.config.Frequency,
					Power:     d.config.TxPower,
					Modulation: &gw.Modulation{
						Lora: &gw.LoraModulationInfo{
							Bandwidth:             d.config.Bandwidth,
							SpreadingFactor:       d.config.SpreadingFactor,
							CodeRate:              codeRate,
							PolarizationInversion: true,
						},
					},
					Timing: &gw.Timing{
						Immediately: &gw.ImmediatelyTimingInfo{},
					},
				},
			},
		},
	}

	dlData, err := gw.MarshalDownlinkFrame(downlink)
	if err != nil {
		return fmt.Errorf("failed to marshal downlink: %w", err)
	}

	msg := zmq4.NewMsgFrom([]byte("down"), dlData)
	d.mu.Lock()
	err = d.cmdSock.Send(msg)
	d.mu.Unlock()
	if err != nil {
		return fmt.Errorf("failed to send downlink: %w", err)
	}

	d.mu.Lock()
	resp, err := d.cmdSock.Recv()
	d.mu.Unlock()
	if err != nil {
		return fmt.Errorf("failed to receive TX ack: %w", err)
	}

	if len(resp.Frames) > 0 {
		txAck, err := gw.UnmarshalDownlinkTxAck(resp.Frames[0])
		if err != nil {
			return fmt.Errorf("failed to unmarshal TX ack: %w", err)
		}

		if len(txAck.Items) > 0 && txAck.Items[0].Status != gw.TxAckStatus_OK {
			return fmt.Errorf("TX failed: %s", txAck.Items[0].Status.String())
		}
	}

	log.Printf("TX: %d bytes, freq=%d, SF=%d", len(payload), d.config.Frequency, d.config.SpreadingFactor)
	return nil
}

// eventLoop receives events from Concentratord
func (d *ConcentratordDriver) eventLoop() {
	defer d.wg.Done()

	for {
		select {
		case <-d.ctx.Done():
			return
		default:
		}

		msg, err := d.eventSock.Recv()
		if err != nil {
			if d.ctx.Err() != nil {
				return
			}
			continue
		}

		if len(msg.Frames) < 2 {
			continue
		}

		eventType := string(msg.Frames[0])
		eventData := msg.Frames[1]

		event, err := gw.UnmarshalEvent(eventType, eventData)
		if err != nil {
			log.Printf("Failed to unmarshal event: %v", err)
			continue
		}

		if event.UplinkFrame != nil {
			d.handleUplink(event.UplinkFrame)
		} else if event.GatewayStats != nil {
			d.handleStats(event.GatewayStats)
		}
	}
}

// handleUplink processes an uplink frame from Concentratord
func (d *ConcentratordDriver) handleUplink(uplink *gw.UplinkFrame) {
	if uplink == nil || len(uplink.PhyPayload) == 0 {
		return
	}

	payload := uplink.PhyPayload

	if d.cipher != nil {
		decrypted, err := d.decrypt(payload)
		if err != nil {
			log.Printf("Failed to decrypt uplink: %v", err)
			return
		}
		payload = decrypted
	}

	msg, err := protocol.Decode(payload)
	if err != nil {
		log.Printf("Failed to decode message: %v", err)
		return
	}

	if uplink.RxInfo != nil {
		msg.RSSI = int16(uplink.RxInfo.Rssi)
		msg.SNR = uplink.RxInfo.Snr
	}
	msg.ReceivedAt = time.Now().Unix()

	log.Printf("RX: %d bytes from %s, RSSI=%d, SNR=%.1f",
		len(payload), msg.DeviceUIDString(), msg.RSSI, msg.SNR)

	d.mu.Lock()
	cb := d.onReceive
	d.mu.Unlock()
	if cb != nil {
		cb(msg)
	}
}

// handleStats processes gateway statistics
func (d *ConcentratordDriver) handleStats(stats *gw.GatewayStats) {
	if stats == nil {
		return
	}
	log.Printf("Gateway stats: RX=%d, TX=%d", stats.RxPacketsReceivedOk, stats.TxPacketsEmitted)
}

// encryptForDevice encrypts data using AES-128-GCM with per-device key
func (d *ConcentratordDriver) encryptForDevice(deviceUID [8]byte, plaintext []byte) ([]byte, error) {
	key := d.keyCache.GetKey(deviceUID)

	d.mu.Lock()
	d.txNonce++
	nonce := d.txNonce
	d.mu.Unlock()

	return EncryptGCM(key, nonce, plaintext)
}

// decryptFromDevice decrypts data using AES-128-GCM with per-device key
func (d *ConcentratordDriver) decryptFromDevice(deviceUID [8]byte, ciphertext []byte) ([]byte, error) {
	key := d.keyCache.GetKey(deviceUID)
	return DecryptGCM(key, ciphertext)
}

// encrypt encrypts data using legacy AES-128-CTR (for backward compatibility)
func (d *ConcentratordDriver) encrypt(plaintext []byte) ([]byte, error) {
	if d.cipher == nil {
		return plaintext, nil
	}

	iv := make([]byte, aes.BlockSize)
	if _, err := io.ReadFull(rand.Reader, iv); err != nil {
		return nil, err
	}

	ciphertext := make([]byte, aes.BlockSize+len(plaintext))
	copy(ciphertext[:aes.BlockSize], iv)

	stream := cipher.NewCTR(d.cipher, iv)
	stream.XORKeyStream(ciphertext[aes.BlockSize:], plaintext)

	return ciphertext, nil
}

// decrypt decrypts data using legacy AES-128-CTR (for backward compatibility)
func (d *ConcentratordDriver) decrypt(ciphertext []byte) ([]byte, error) {
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

// Ensure binary is imported
var _ = binary.LittleEndian
