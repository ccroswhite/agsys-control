// Package protocol defines the LoRa message formats and types for communication
// between the property controller and field devices.
package protocol

import (
	"encoding/binary"
	"fmt"
)

// Message types for LoRa communication
const (
	// Device -> Controller messages
	MsgTypeSensorData      uint8 = 0x01 // Soil moisture sensor reading
	MsgTypeWaterMeterData  uint8 = 0x02 // Water meter reading
	MsgTypeValveStatus     uint8 = 0x03 // Valve controller status report
	MsgTypeValveAck        uint8 = 0x04 // Valve command acknowledgment
	MsgTypeScheduleRequest uint8 = 0x05 // Valve controller requesting schedule
	MsgTypeHeartbeat       uint8 = 0x06 // Device heartbeat/keepalive

	// Controller -> Device messages
	MsgTypeValveCommand   uint8 = 0x10 // Immediate valve open/close command
	MsgTypeScheduleUpdate uint8 = 0x11 // Schedule data for valve controller
	MsgTypeConfigUpdate   uint8 = 0x12 // Configuration update
	MsgTypeTimeSync       uint8 = 0x13 // Time synchronization
	MsgTypeOTAAnnounce    uint8 = 0x20 // OTA firmware announcement
	MsgTypeOTAChunk       uint8 = 0x21 // OTA firmware chunk

	// Bidirectional
	MsgTypeAck  uint8 = 0xF0 // Generic acknowledgment
	MsgTypeNack uint8 = 0xF1 // Negative acknowledgment
)

// Device types
const (
	DeviceTypeSoilMoisture    uint8 = 0x01
	DeviceTypeWaterMeter      uint8 = 0x02
	DeviceTypeValveController uint8 = 0x03
	DeviceTypeValveActuator   uint8 = 0x04
)

// Valve states
const (
	ValveStateClosed  uint8 = 0x00
	ValveStateOpen    uint8 = 0x01
	ValveStateOpening uint8 = 0x02
	ValveStateClosing uint8 = 0x03
	ValveStateError   uint8 = 0xFF
)

// Valve commands
const (
	ValveCmdClose uint8 = 0x00
	ValveCmdOpen  uint8 = 0x01
	ValveCmdStop  uint8 = 0x02
	ValveCmdQuery uint8 = 0x03
)

// LoRaMessage represents a decoded LoRa message
type LoRaMessage struct {
	DeviceUID  [8]byte // Unique device identifier (MCU UID)
	MsgType    uint8   // Message type
	SeqNum     uint16  // Sequence number for tracking
	Payload    []byte  // Message-specific payload
	RSSI       int16   // Received signal strength (set by receiver)
	SNR        float32 // Signal-to-noise ratio (set by receiver)
	ReceivedAt int64   // Unix timestamp when received
}

// Header size: 8 (UID) + 1 (type) + 2 (seq) = 11 bytes
const HeaderSize = 11

// Encode serializes the message for transmission
func (m *LoRaMessage) Encode() []byte {
	buf := make([]byte, HeaderSize+len(m.Payload))
	copy(buf[0:8], m.DeviceUID[:])
	buf[8] = m.MsgType
	binary.LittleEndian.PutUint16(buf[9:11], m.SeqNum)
	copy(buf[HeaderSize:], m.Payload)
	return buf
}

// Decode parses a raw message into the LoRaMessage structure
func Decode(data []byte) (*LoRaMessage, error) {
	if len(data) < HeaderSize {
		return nil, fmt.Errorf("message too short: %d bytes", len(data))
	}

	msg := &LoRaMessage{
		MsgType: data[8],
		SeqNum:  binary.LittleEndian.Uint16(data[9:11]),
	}
	copy(msg.DeviceUID[:], data[0:8])

	if len(data) > HeaderSize {
		msg.Payload = make([]byte, len(data)-HeaderSize)
		copy(msg.Payload, data[HeaderSize:])
	}

	return msg, nil
}

// DeviceUIDString returns the device UID as a hex string
func (m *LoRaMessage) DeviceUIDString() string {
	return fmt.Sprintf("%02X%02X%02X%02X%02X%02X%02X%02X",
		m.DeviceUID[0], m.DeviceUID[1], m.DeviceUID[2], m.DeviceUID[3],
		m.DeviceUID[4], m.DeviceUID[5], m.DeviceUID[6], m.DeviceUID[7])
}

// SensorDataPayload represents soil moisture sensor data
type SensorDataPayload struct {
	ProbeID         uint8  // Probe index 0-3
	MoistureRaw     uint16 // Raw ADC value
	MoisturePercent uint8  // Calculated moisture percentage
	Temperature     int16  // Temperature in 0.1°C units
	BatteryMV       uint16 // Battery voltage in mV
}

// Encode serializes sensor data payload
func (p *SensorDataPayload) Encode() []byte {
	buf := make([]byte, 8)
	buf[0] = p.ProbeID
	binary.LittleEndian.PutUint16(buf[1:3], p.MoistureRaw)
	buf[3] = p.MoisturePercent
	binary.LittleEndian.PutUint16(buf[4:6], uint16(p.Temperature))
	binary.LittleEndian.PutUint16(buf[6:8], p.BatteryMV)
	return buf
}

// DecodeSensorData parses sensor data from payload
func DecodeSensorData(data []byte) (*SensorDataPayload, error) {
	if len(data) < 8 {
		return nil, fmt.Errorf("sensor data too short: %d bytes", len(data))
	}
	return &SensorDataPayload{
		ProbeID:         data[0],
		MoistureRaw:     binary.LittleEndian.Uint16(data[1:3]),
		MoisturePercent: data[3],
		Temperature:     int16(binary.LittleEndian.Uint16(data[4:6])),
		BatteryMV:       binary.LittleEndian.Uint16(data[6:8]),
	}, nil
}

// WaterMeterPayload represents water meter data
type WaterMeterPayload struct {
	TotalLiters uint32 // Total liters since installation
	FlowRateLPM uint16 // Current flow rate in liters per minute * 10
	BatteryMV   uint16 // Battery voltage in mV
}

// Encode serializes water meter payload
func (p *WaterMeterPayload) Encode() []byte {
	buf := make([]byte, 8)
	binary.LittleEndian.PutUint32(buf[0:4], p.TotalLiters)
	binary.LittleEndian.PutUint16(buf[4:6], p.FlowRateLPM)
	binary.LittleEndian.PutUint16(buf[6:8], p.BatteryMV)
	return buf
}

// DecodeWaterMeter parses water meter data from payload
func DecodeWaterMeter(data []byte) (*WaterMeterPayload, error) {
	if len(data) < 8 {
		return nil, fmt.Errorf("water meter data too short: %d bytes", len(data))
	}
	return &WaterMeterPayload{
		TotalLiters: binary.LittleEndian.Uint32(data[0:4]),
		FlowRateLPM: binary.LittleEndian.Uint16(data[4:6]),
		BatteryMV:   binary.LittleEndian.Uint16(data[6:8]),
	}, nil
}

// ValveStatusPayload represents valve controller status
type ValveStatusPayload struct {
	ActuatorAddr uint8  // Actuator address (0-63)
	State        uint8  // Current valve state
	CurrentMA    uint16 // Motor current in mA (during operation)
	Flags        uint8  // Status flags (bit 0: power fail, bit 1: overcurrent, etc.)
}

// Encode serializes valve status payload
func (p *ValveStatusPayload) Encode() []byte {
	buf := make([]byte, 5)
	buf[0] = p.ActuatorAddr
	buf[1] = p.State
	binary.LittleEndian.PutUint16(buf[2:4], p.CurrentMA)
	buf[4] = p.Flags
	return buf
}

// DecodeValveStatus parses valve status from payload
func DecodeValveStatus(data []byte) (*ValveStatusPayload, error) {
	if len(data) < 5 {
		return nil, fmt.Errorf("valve status too short: %d bytes", len(data))
	}
	return &ValveStatusPayload{
		ActuatorAddr: data[0],
		State:        data[1],
		CurrentMA:    binary.LittleEndian.Uint16(data[2:4]),
		Flags:        data[4],
	}, nil
}

// ValveCommandPayload represents a valve command
type ValveCommandPayload struct {
	ActuatorAddr uint8  // Target actuator address (0-63, 0xFF for all)
	Command      uint8  // Command (open/close/stop/query)
	CommandID    uint16 // Unique command ID for tracking acknowledgment
}

// Encode serializes valve command payload
func (p *ValveCommandPayload) Encode() []byte {
	buf := make([]byte, 4)
	buf[0] = p.ActuatorAddr
	buf[1] = p.Command
	binary.LittleEndian.PutUint16(buf[2:4], p.CommandID)
	return buf
}

// DecodeValveCommand parses valve command from payload
func DecodeValveCommand(data []byte) (*ValveCommandPayload, error) {
	if len(data) < 4 {
		return nil, fmt.Errorf("valve command too short: %d bytes", len(data))
	}
	return &ValveCommandPayload{
		ActuatorAddr: data[0],
		Command:      data[1],
		CommandID:    binary.LittleEndian.Uint16(data[2:4]),
	}, nil
}

// ValveAckPayload represents acknowledgment of a valve command
type ValveAckPayload struct {
	ActuatorAddr uint8  // Actuator that executed the command
	CommandID    uint16 // Command ID being acknowledged
	ResultState  uint8  // Resulting valve state
	Success      bool   // Whether command succeeded
}

// Encode serializes valve ack payload
func (p *ValveAckPayload) Encode() []byte {
	buf := make([]byte, 5)
	buf[0] = p.ActuatorAddr
	binary.LittleEndian.PutUint16(buf[1:3], p.CommandID)
	buf[3] = p.ResultState
	if p.Success {
		buf[4] = 1
	} else {
		buf[4] = 0
	}
	return buf
}

// DecodeValveAck parses valve ack from payload
func DecodeValveAck(data []byte) (*ValveAckPayload, error) {
	if len(data) < 5 {
		return nil, fmt.Errorf("valve ack too short: %d bytes", len(data))
	}
	return &ValveAckPayload{
		ActuatorAddr: data[0],
		CommandID:    binary.LittleEndian.Uint16(data[1:3]),
		ResultState:  data[3],
		Success:      data[4] != 0,
	}, nil
}

// ScheduleEntry represents a single schedule entry
type ScheduleEntry struct {
	DayMask      uint8  // Bit mask for days (bit 0 = Sunday, bit 6 = Saturday)
	StartHour    uint8  // Start hour (0-23)
	StartMinute  uint8  // Start minute (0-59)
	DurationMins uint16 // Duration in minutes
	ActuatorMask uint64 // Bit mask for which actuators (up to 64)
}

// ScheduleUpdatePayload represents schedule data sent to valve controller
type ScheduleUpdatePayload struct {
	Version    uint16          // Schedule version number
	EntryCount uint8           // Number of entries
	Entries    []ScheduleEntry // Schedule entries
}

// Encode serializes schedule update payload
func (p *ScheduleUpdatePayload) Encode() []byte {
	buf := make([]byte, 3+len(p.Entries)*13)
	binary.LittleEndian.PutUint16(buf[0:2], p.Version)
	buf[2] = p.EntryCount

	offset := 3
	for _, entry := range p.Entries {
		buf[offset] = entry.DayMask
		buf[offset+1] = entry.StartHour
		buf[offset+2] = entry.StartMinute
		binary.LittleEndian.PutUint16(buf[offset+3:offset+5], entry.DurationMins)
		binary.LittleEndian.PutUint64(buf[offset+5:offset+13], entry.ActuatorMask)
		offset += 13
	}
	return buf[:offset]
}

// TimeSyncPayload represents time synchronization data
type TimeSyncPayload struct {
	UnixTimestamp uint32 // Current Unix timestamp
	UTCOffset     int8   // UTC offset in hours
}

// Encode serializes time sync payload
func (p *TimeSyncPayload) Encode() []byte {
	buf := make([]byte, 5)
	binary.LittleEndian.PutUint32(buf[0:4], p.UnixTimestamp)
	buf[4] = byte(p.UTCOffset)
	return buf
}
