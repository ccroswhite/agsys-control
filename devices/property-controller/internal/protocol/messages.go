// Package protocol defines the LoRa message formats and types for communication
// between the property controller and field devices.
//
// This package imports the canonical protocol definitions from agsys-api/pkg/lora
// and provides payload encoding/decoding functions specific to the property controller.
package protocol

import (
	"encoding/binary"
	"fmt"

	"github.com/ccroswhite/agsys-api/pkg/lora"
)

// Re-export protocol constants from shared package for backward compatibility
const (
	ProtocolVersion = lora.ProtocolVersion
	MagicByte1      = lora.MagicByte1
	MagicByte2      = lora.MagicByte2
	HeaderSize      = lora.HeaderSize
	DeviceUIDSize   = lora.DeviceUIDSize
)

// Re-export message types from shared package
const (
	MsgTypeHeartbeat         = lora.MsgTypeHeartbeat
	MsgTypeLogBatch          = lora.MsgTypeLogBatch
	MsgTypeConfigRequest     = lora.MsgTypeConfigRequest
	MsgTypeAck               = lora.MsgTypeAck
	MsgTypeNack              = lora.MsgTypeNack
	MsgTypeConfigUpdate      = lora.MsgTypeConfigUpdate
	MsgTypeTimeSync          = lora.MsgTypeTimeSync
	MsgTypeSoilReport        = lora.MsgTypeSoilReport
	MsgTypeSoilCalibrateReq  = lora.MsgTypeSoilCalibrateReq
	MsgTypeMeterReport       = lora.MsgTypeMeterReport
	MsgTypeMeterAlarm        = lora.MsgTypeMeterAlarm
	MsgTypeMeterCalibrateReq = lora.MsgTypeMeterCalibrateReq
	MsgTypeMeterResetTotal   = lora.MsgTypeMeterResetTotal
	MsgTypeValveStatus       = lora.MsgTypeValveStatus
	MsgTypeValveAck          = lora.MsgTypeValveAck
	MsgTypeValveScheduleReq  = lora.MsgTypeValveScheduleReq
	MsgTypeValveCommand      = lora.MsgTypeValveCommand
	MsgTypeValveSchedule     = lora.MsgTypeValveSchedule
	MsgTypeOTAAnnounce       = lora.MsgTypeOTAAnnounce
	MsgTypeOTAChunk          = lora.MsgTypeOTAChunk
	MsgTypeOTAStatus         = lora.MsgTypeOTAStatus

	// Legacy aliases
	MsgTypeSensorReport     = lora.MsgTypeSensorReport
	MsgTypeWaterMeterReport = lora.MsgTypeWaterMeterReport
	MsgTypeScheduleRequest  = lora.MsgTypeScheduleRequest
	MsgTypeScheduleUpdate   = lora.MsgTypeScheduleUpdate
)

// Re-export device types from shared package
const (
	DeviceTypeSoilMoisture    = lora.DeviceTypeSoilMoisture
	DeviceTypeValveController = lora.DeviceTypeValveController
	DeviceTypeWaterMeter      = lora.DeviceTypeWaterMeter
	DeviceTypeValveActuator   = lora.DeviceTypeValveActuator
)

// Re-export valve states from shared package
const (
	ValveStateClosed  = lora.ValveStateClosed
	ValveStateOpen    = lora.ValveStateOpen
	ValveStateOpening = lora.ValveStateOpening
	ValveStateClosing = lora.ValveStateClosing
	ValveStateError   = lora.ValveStateError
)

// Re-export valve commands from shared package
const (
	ValveCmdClose = lora.ValveCmdClose
	ValveCmdOpen  = lora.ValveCmdOpen
	ValveCmdStop  = lora.ValveCmdStop
	ValveCmdQuery = lora.ValveCmdQuery
)

// Type aliases for backward compatibility
type Header = lora.Header

// LoRaMessage represents a decoded LoRa message (extends lora.Message with controller-specific fields)
type LoRaMessage struct {
	Header     Header  // Protocol header
	Payload    []byte  // Message-specific payload
	RSSI       int16   // Received signal strength (set by receiver)
	SNR        float32 // Signal-to-noise ratio (set by receiver)
	ReceivedAt int64   // Unix timestamp when received
}

// NewHeader creates a new header with magic bytes and version set
func NewHeader(msgType uint8, deviceType uint8, deviceUID [8]byte, sequence uint16) *Header {
	return lora.NewHeader(msgType, deviceType, deviceUID, sequence)
}

// DecodeHeader parses a header from raw bytes
func DecodeHeader(data []byte) (*Header, error) {
	return lora.DecodeHeader(data)
}

// Encode serializes the full message for transmission
func (m *LoRaMessage) Encode() []byte {
	headerBytes := m.Header.Encode()
	buf := make([]byte, HeaderSize+len(m.Payload))
	copy(buf[0:HeaderSize], headerBytes)
	copy(buf[HeaderSize:], m.Payload)
	return buf
}

// Decode parses a raw message into the LoRaMessage structure
func Decode(data []byte) (*LoRaMessage, error) {
	if len(data) < HeaderSize {
		return nil, fmt.Errorf("message too short: %d bytes", len(data))
	}

	header, err := DecodeHeader(data)
	if err != nil {
		return nil, err
	}

	if !header.IsValid() {
		return nil, fmt.Errorf("invalid header: magic=%02X%02X version=%d",
			header.Magic[0], header.Magic[1], header.Version)
	}

	msg := &LoRaMessage{
		Header: *header,
	}

	if len(data) > HeaderSize {
		msg.Payload = make([]byte, len(data)-HeaderSize)
		copy(msg.Payload, data[HeaderSize:])
	}

	return msg, nil
}

// DeviceUIDString returns the device UID as a hex string
func (m *LoRaMessage) DeviceUIDString() string {
	return m.Header.DeviceUIDString()
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

// MeterAlarmPayload represents a water meter alarm
type MeterAlarmPayload struct {
	Timestamp   uint32 // Device uptime in seconds
	AlarmType   uint8  // Type of alarm (see MeterAlarm* constants)
	FlowRateLPM uint16 // Current flow rate in liters/min * 10
	DurationSec uint32 // Duration of alarm condition in seconds
	TotalLiters uint32 // Total liters at alarm time
	Flags       uint8  // Additional flags
}

// Meter alarm types
const (
	MeterAlarmCleared  uint8 = 0x00 // Alarm condition cleared
	MeterAlarmLeak     uint8 = 0x01 // Continuous flow exceeds threshold
	MeterAlarmReverse  uint8 = 0x02 // Reverse flow detected
	MeterAlarmTamper   uint8 = 0x03 // Tamper detected
	MeterAlarmHighFlow uint8 = 0x04 // Flow rate exceeds maximum
)

// DecodeMeterAlarm parses meter alarm data from payload
func DecodeMeterAlarm(data []byte) (*MeterAlarmPayload, error) {
	if len(data) < 16 {
		return nil, fmt.Errorf("meter alarm data too short: %d bytes", len(data))
	}
	return &MeterAlarmPayload{
		Timestamp:   binary.LittleEndian.Uint32(data[0:4]),
		AlarmType:   data[4],
		FlowRateLPM: binary.LittleEndian.Uint16(data[5:7]),
		DurationSec: binary.LittleEndian.Uint32(data[7:11]),
		TotalLiters: binary.LittleEndian.Uint32(data[11:15]),
		Flags:       data[15],
	}, nil
}

// MeterAlarmTypeString returns a human-readable alarm type
func MeterAlarmTypeString(alarmType uint8) string {
	switch alarmType {
	case MeterAlarmCleared:
		return "CLEARED"
	case MeterAlarmLeak:
		return "LEAK"
	case MeterAlarmReverse:
		return "REVERSE_FLOW"
	case MeterAlarmTamper:
		return "TAMPER"
	case MeterAlarmHighFlow:
		return "HIGH_FLOW"
	default:
		return fmt.Sprintf("UNKNOWN(%d)", alarmType)
	}
}

// MeterConfigPayload represents water meter configuration
type MeterConfigPayload struct {
	ConfigVersion     uint16 // Configuration version
	ReportIntervalSec uint16 // Report interval in seconds
	PulsesPerLiter    uint16 // Calibration: pulses per liter * 100
	LeakThresholdMin  uint16 // Minutes of continuous flow = leak
	MaxFlowRateLPM    uint16 // Max expected flow rate * 10
	Flags             uint8  // Configuration flags
}

// Meter config flags
const (
	MeterCfgLeakDetectEn  uint8 = 1 << 0 // Enable leak detection
	MeterCfgReverseDetect uint8 = 1 << 1 // Enable reverse flow detection
	MeterCfgTamperDetect  uint8 = 1 << 2 // Enable tamper detection
)

// Encode serializes meter config payload
func (p *MeterConfigPayload) Encode() []byte {
	buf := make([]byte, 11)
	binary.LittleEndian.PutUint16(buf[0:2], p.ConfigVersion)
	binary.LittleEndian.PutUint16(buf[2:4], p.ReportIntervalSec)
	binary.LittleEndian.PutUint16(buf[4:6], p.PulsesPerLiter)
	binary.LittleEndian.PutUint16(buf[6:8], p.LeakThresholdMin)
	binary.LittleEndian.PutUint16(buf[8:10], p.MaxFlowRateLPM)
	buf[10] = p.Flags
	return buf
}

// MeterResetTotalPayload represents a meter reset command
type MeterResetTotalPayload struct {
	CommandID      uint16 // Command ID for acknowledgment
	ResetType      uint8  // 0 = reset to zero, 1 = set to value
	NewTotalLiters uint32 // New total (only used if ResetType == 1)
}

// Encode serializes meter reset payload
func (p *MeterResetTotalPayload) Encode() []byte {
	buf := make([]byte, 7)
	binary.LittleEndian.PutUint16(buf[0:2], p.CommandID)
	buf[2] = p.ResetType
	binary.LittleEndian.PutUint32(buf[3:7], p.NewTotalLiters)
	return buf
}

// MeterResetAckPayload represents the response to a meter reset
type MeterResetAckPayload struct {
	AckedSequence  uint16 // Sequence number being acknowledged
	Status         uint8  // 0 = OK, non-zero = error
	OldTotalLiters uint32 // Previous total before reset
	NewTotalLiters uint32 // New total after reset
}

// DecodeMeterResetAck parses meter reset ack from payload
func DecodeMeterResetAck(data []byte) (*MeterResetAckPayload, error) {
	if len(data) < 11 {
		return nil, fmt.Errorf("meter reset ack too short: %d bytes", len(data))
	}
	return &MeterResetAckPayload{
		AckedSequence:  binary.LittleEndian.Uint16(data[0:2]),
		Status:         data[2],
		OldTotalLiters: binary.LittleEndian.Uint32(data[3:7]),
		NewTotalLiters: binary.LittleEndian.Uint32(data[7:11]),
	}, nil
}

// AckPayload represents a generic acknowledgment
type AckPayload struct {
	AckedSequence uint16 // Sequence number being acknowledged
	Status        uint8  // 0 = OK, non-zero = error code
	Flags         uint8  // Response flags
}

// ACK flags
const (
	AckFlagSendLogs       uint8 = 1 << 0 // Request pending logs
	AckFlagConfigAvail    uint8 = 1 << 1 // New config available
	AckFlagTimeSync       uint8 = 1 << 2 // Time sync follows
	AckFlagScheduleUpdate uint8 = 1 << 3 // Schedule update follows
)

// Encode serializes ack payload
func (p *AckPayload) Encode() []byte {
	buf := make([]byte, 4)
	binary.LittleEndian.PutUint16(buf[0:2], p.AckedSequence)
	buf[2] = p.Status
	buf[3] = p.Flags
	return buf
}

// DecodeAck parses ack from payload
func DecodeAck(data []byte) (*AckPayload, error) {
	if len(data) < 4 {
		return nil, fmt.Errorf("ack too short: %d bytes", len(data))
	}
	return &AckPayload{
		AckedSequence: binary.LittleEndian.Uint16(data[0:2]),
		Status:        data[2],
		Flags:         data[3],
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
