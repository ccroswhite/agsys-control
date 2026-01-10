package protocol

import (
	"bytes"
	"testing"
)

// TestMeterAlarmEncodeDecode tests MeterAlarm payload encoding/decoding roundtrip
func TestMeterAlarmEncodeDecode(t *testing.T) {
	tests := []struct {
		name  string
		alarm MeterAlarmPayload
	}{
		{
			name: "leak alarm",
			alarm: MeterAlarmPayload{
				Timestamp:   12345,
				AlarmType:   MeterAlarmLeak,
				FlowRateLPM: 150, // 15.0 L/min
				DurationSec: 3600,
				TotalLiters: 50000,
				Flags:       0x01,
			},
		},
		{
			name: "high flow alarm",
			alarm: MeterAlarmPayload{
				Timestamp:   99999,
				AlarmType:   MeterAlarmHighFlow,
				FlowRateLPM: 1200, // 120.0 L/min
				DurationSec: 60,
				TotalLiters: 100000,
				Flags:       0x00,
			},
		},
		{
			name: "cleared alarm",
			alarm: MeterAlarmPayload{
				Timestamp:   54321,
				AlarmType:   MeterAlarmCleared,
				FlowRateLPM: 0,
				DurationSec: 0,
				TotalLiters: 75000,
				Flags:       0x00,
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Encode (manual since MeterAlarmPayload doesn't have Encode)
			encoded := encodeMeterAlarm(&tt.alarm)

			// Decode
			decoded, err := DecodeMeterAlarm(encoded)
			if err != nil {
				t.Fatalf("DecodeMeterAlarm failed: %v", err)
			}

			// Verify
			if decoded.Timestamp != tt.alarm.Timestamp {
				t.Errorf("Timestamp mismatch: got %d, want %d", decoded.Timestamp, tt.alarm.Timestamp)
			}
			if decoded.AlarmType != tt.alarm.AlarmType {
				t.Errorf("AlarmType mismatch: got %d, want %d", decoded.AlarmType, tt.alarm.AlarmType)
			}
			if decoded.FlowRateLPM != tt.alarm.FlowRateLPM {
				t.Errorf("FlowRateLPM mismatch: got %d, want %d", decoded.FlowRateLPM, tt.alarm.FlowRateLPM)
			}
			if decoded.DurationSec != tt.alarm.DurationSec {
				t.Errorf("DurationSec mismatch: got %d, want %d", decoded.DurationSec, tt.alarm.DurationSec)
			}
			if decoded.TotalLiters != tt.alarm.TotalLiters {
				t.Errorf("TotalLiters mismatch: got %d, want %d", decoded.TotalLiters, tt.alarm.TotalLiters)
			}
			if decoded.Flags != tt.alarm.Flags {
				t.Errorf("Flags mismatch: got %d, want %d", decoded.Flags, tt.alarm.Flags)
			}
		})
	}
}

// Helper to encode MeterAlarmPayload (matches C struct layout)
func encodeMeterAlarm(a *MeterAlarmPayload) []byte {
	buf := make([]byte, 16)
	// timestamp (4 bytes, little-endian)
	buf[0] = byte(a.Timestamp)
	buf[1] = byte(a.Timestamp >> 8)
	buf[2] = byte(a.Timestamp >> 16)
	buf[3] = byte(a.Timestamp >> 24)
	// alarmType (1 byte)
	buf[4] = a.AlarmType
	// flowRateLPM (2 bytes, little-endian)
	buf[5] = byte(a.FlowRateLPM)
	buf[6] = byte(a.FlowRateLPM >> 8)
	// durationSec (4 bytes, little-endian)
	buf[7] = byte(a.DurationSec)
	buf[8] = byte(a.DurationSec >> 8)
	buf[9] = byte(a.DurationSec >> 16)
	buf[10] = byte(a.DurationSec >> 24)
	// totalLiters (4 bytes, little-endian)
	buf[11] = byte(a.TotalLiters)
	buf[12] = byte(a.TotalLiters >> 8)
	buf[13] = byte(a.TotalLiters >> 16)
	buf[14] = byte(a.TotalLiters >> 24)
	// flags (1 byte)
	buf[15] = a.Flags
	return buf
}

// TestMeterConfigEncodeDecode tests MeterConfig payload roundtrip
func TestMeterConfigEncodeDecode(t *testing.T) {
	config := MeterConfigPayload{
		ConfigVersion:     5,
		ReportIntervalSec: 60,
		PulsesPerLiter:    45000, // 450.00 pulses/L
		LeakThresholdMin:  60,
		MaxFlowRateLPM:    1000, // 100.0 L/min
		Flags:             MeterCfgLeakDetectEn | MeterCfgTamperDetect,
	}

	encoded := config.Encode()
	if len(encoded) != 11 {
		t.Fatalf("Encoded length wrong: got %d, want 11", len(encoded))
	}

	// Decode manually to verify
	decoded := MeterConfigPayload{
		ConfigVersion:     uint16(encoded[0]) | uint16(encoded[1])<<8,
		ReportIntervalSec: uint16(encoded[2]) | uint16(encoded[3])<<8,
		PulsesPerLiter:    uint16(encoded[4]) | uint16(encoded[5])<<8,
		LeakThresholdMin:  uint16(encoded[6]) | uint16(encoded[7])<<8,
		MaxFlowRateLPM:    uint16(encoded[8]) | uint16(encoded[9])<<8,
		Flags:             encoded[10],
	}

	if decoded.ConfigVersion != config.ConfigVersion {
		t.Errorf("ConfigVersion mismatch: got %d, want %d", decoded.ConfigVersion, config.ConfigVersion)
	}
	if decoded.ReportIntervalSec != config.ReportIntervalSec {
		t.Errorf("ReportIntervalSec mismatch: got %d, want %d", decoded.ReportIntervalSec, config.ReportIntervalSec)
	}
	if decoded.PulsesPerLiter != config.PulsesPerLiter {
		t.Errorf("PulsesPerLiter mismatch: got %d, want %d", decoded.PulsesPerLiter, config.PulsesPerLiter)
	}
	if decoded.LeakThresholdMin != config.LeakThresholdMin {
		t.Errorf("LeakThresholdMin mismatch: got %d, want %d", decoded.LeakThresholdMin, config.LeakThresholdMin)
	}
	if decoded.MaxFlowRateLPM != config.MaxFlowRateLPM {
		t.Errorf("MaxFlowRateLPM mismatch: got %d, want %d", decoded.MaxFlowRateLPM, config.MaxFlowRateLPM)
	}
	if decoded.Flags != config.Flags {
		t.Errorf("Flags mismatch: got %d, want %d", decoded.Flags, config.Flags)
	}
}

// TestMeterResetEncodeDecode tests MeterResetTotal payload roundtrip
func TestMeterResetEncodeDecode(t *testing.T) {
	tests := []struct {
		name  string
		reset MeterResetTotalPayload
	}{
		{
			name: "reset to zero",
			reset: MeterResetTotalPayload{
				CommandID:      1234,
				ResetType:      0,
				NewTotalLiters: 0,
			},
		},
		{
			name: "set to value",
			reset: MeterResetTotalPayload{
				CommandID:      5678,
				ResetType:      1,
				NewTotalLiters: 100000,
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			encoded := tt.reset.Encode()
			if len(encoded) != 7 {
				t.Fatalf("Encoded length wrong: got %d, want 7", len(encoded))
			}

			// Decode manually
			decoded := MeterResetTotalPayload{
				CommandID:      uint16(encoded[0]) | uint16(encoded[1])<<8,
				ResetType:      encoded[2],
				NewTotalLiters: uint32(encoded[3]) | uint32(encoded[4])<<8 | uint32(encoded[5])<<16 | uint32(encoded[6])<<24,
			}

			if decoded.CommandID != tt.reset.CommandID {
				t.Errorf("CommandID mismatch: got %d, want %d", decoded.CommandID, tt.reset.CommandID)
			}
			if decoded.ResetType != tt.reset.ResetType {
				t.Errorf("ResetType mismatch: got %d, want %d", decoded.ResetType, tt.reset.ResetType)
			}
			if decoded.NewTotalLiters != tt.reset.NewTotalLiters {
				t.Errorf("NewTotalLiters mismatch: got %d, want %d", decoded.NewTotalLiters, tt.reset.NewTotalLiters)
			}
		})
	}
}

// TestAckEncodeDecode tests Ack payload roundtrip
func TestAckEncodeDecode(t *testing.T) {
	ack := AckPayload{
		AckedSequence: 0x1234,
		Status:        0,
		Flags:         AckFlagConfigAvail | AckFlagTimeSync,
	}

	encoded := ack.Encode()
	if len(encoded) != 4 {
		t.Fatalf("Encoded length wrong: got %d, want 4", len(encoded))
	}

	decoded, err := DecodeAck(encoded)
	if err != nil {
		t.Fatalf("DecodeAck failed: %v", err)
	}

	if decoded.AckedSequence != ack.AckedSequence {
		t.Errorf("AckedSequence mismatch: got %d, want %d", decoded.AckedSequence, ack.AckedSequence)
	}
	if decoded.Status != ack.Status {
		t.Errorf("Status mismatch: got %d, want %d", decoded.Status, ack.Status)
	}
	if decoded.Flags != ack.Flags {
		t.Errorf("Flags mismatch: got %d, want %d", decoded.Flags, ack.Flags)
	}
}

// TestMeterAlarmTypeString tests alarm type string conversion
func TestMeterAlarmTypeString(t *testing.T) {
	tests := []struct {
		alarmType uint8
		expected  string
	}{
		{MeterAlarmCleared, "CLEARED"},
		{MeterAlarmLeak, "LEAK"},
		{MeterAlarmReverse, "REVERSE_FLOW"},
		{MeterAlarmTamper, "TAMPER"},
		{MeterAlarmHighFlow, "HIGH_FLOW"},
		{0xFF, "UNKNOWN(255)"},
	}

	for _, tt := range tests {
		result := MeterAlarmTypeString(tt.alarmType)
		if result != tt.expected {
			t.Errorf("MeterAlarmTypeString(%d) = %s, want %s", tt.alarmType, result, tt.expected)
		}
	}
}

// TestMessageTypeConstants verifies message type values match protocol spec
func TestMessageTypeConstants(t *testing.T) {
	// Common messages (0x00 - 0x0F)
	if MsgTypeHeartbeat != 0x01 {
		t.Errorf("MsgTypeHeartbeat = 0x%02X, want 0x01", MsgTypeHeartbeat)
	}
	if MsgTypeAck != 0x0E {
		t.Errorf("MsgTypeAck = 0x%02X, want 0x0E", MsgTypeAck)
	}
	if MsgTypeNack != 0x0F {
		t.Errorf("MsgTypeNack = 0x%02X, want 0x0F", MsgTypeNack)
	}

	// Controller -> device (0x10 - 0x1F)
	if MsgTypeConfigUpdate != 0x10 {
		t.Errorf("MsgTypeConfigUpdate = 0x%02X, want 0x10", MsgTypeConfigUpdate)
	}
	if MsgTypeTimeSync != 0x11 {
		t.Errorf("MsgTypeTimeSync = 0x%02X, want 0x11", MsgTypeTimeSync)
	}

	// Soil moisture (0x20 - 0x2F)
	if MsgTypeSoilReport != 0x20 {
		t.Errorf("MsgTypeSoilReport = 0x%02X, want 0x20", MsgTypeSoilReport)
	}

	// Water meter (0x30 - 0x3F)
	if MsgTypeMeterReport != 0x30 {
		t.Errorf("MsgTypeMeterReport = 0x%02X, want 0x30", MsgTypeMeterReport)
	}
	if MsgTypeMeterAlarm != 0x31 {
		t.Errorf("MsgTypeMeterAlarm = 0x%02X, want 0x31", MsgTypeMeterAlarm)
	}
	if MsgTypeMeterResetTotal != 0x33 {
		t.Errorf("MsgTypeMeterResetTotal = 0x%02X, want 0x33", MsgTypeMeterResetTotal)
	}

	// Valve controller (0x40 - 0x4F)
	if MsgTypeValveStatus != 0x40 {
		t.Errorf("MsgTypeValveStatus = 0x%02X, want 0x40", MsgTypeValveStatus)
	}
	if MsgTypeValveCommand != 0x43 {
		t.Errorf("MsgTypeValveCommand = 0x%02X, want 0x43", MsgTypeValveCommand)
	}
}

// TestHeaderEncodeDecode tests LoRa message header encoding/decoding
func TestHeaderEncodeDecode(t *testing.T) {
	header := Header{
		Magic:      [2]byte{MagicByte1, MagicByte2},
		Version:    ProtocolVersion,
		MsgType:    MsgTypeMeterAlarm,
		DeviceType: DeviceTypeWaterMeter,
		DeviceUID:  [8]byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08},
		Sequence:   0x1234,
	}

	encoded := header.Encode()
	if len(encoded) != HeaderSize {
		t.Fatalf("Header encoded size wrong: got %d, want %d", len(encoded), HeaderSize)
	}

	// Verify magic bytes
	if encoded[0] != MagicByte1 || encoded[1] != MagicByte2 {
		t.Errorf("Magic bytes wrong: got [0x%02X, 0x%02X], want [0x%02X, 0x%02X]",
			encoded[0], encoded[1], MagicByte1, MagicByte2)
	}

	// Decode
	decoded, err := DecodeHeader(encoded)
	if err != nil {
		t.Fatalf("DecodeHeader failed: %v", err)
	}

	if decoded.Magic != header.Magic {
		t.Errorf("Magic mismatch")
	}
	if decoded.Version != header.Version {
		t.Errorf("Version mismatch: got %d, want %d", decoded.Version, header.Version)
	}
	if decoded.MsgType != header.MsgType {
		t.Errorf("MsgType mismatch: got 0x%02X, want 0x%02X", decoded.MsgType, header.MsgType)
	}
	if decoded.DeviceType != header.DeviceType {
		t.Errorf("DeviceType mismatch: got %d, want %d", decoded.DeviceType, header.DeviceType)
	}
	if !bytes.Equal(decoded.DeviceUID[:], header.DeviceUID[:]) {
		t.Errorf("DeviceUID mismatch")
	}
	if decoded.Sequence != header.Sequence {
		t.Errorf("Sequence mismatch: got %d, want %d", decoded.Sequence, header.Sequence)
	}
}

// TestDecodeErrors tests error handling for malformed data
func TestDecodeErrors(t *testing.T) {
	// Too short for MeterAlarm
	_, err := DecodeMeterAlarm(make([]byte, 10))
	if err == nil {
		t.Error("DecodeMeterAlarm should fail with short data")
	}

	// Too short for Ack
	_, err = DecodeAck(make([]byte, 2))
	if err == nil {
		t.Error("DecodeAck should fail with short data")
	}

	// Too short for Header
	_, err = DecodeHeader(make([]byte, 10))
	if err == nil {
		t.Error("DecodeHeader should fail with short data")
	}

	// Invalid magic bytes - DecodeHeader parses but IsValid() returns false
	badMagic := make([]byte, HeaderSize)
	badMagic[0] = 0xFF
	badMagic[1] = 0xFF
	header, err := DecodeHeader(badMagic)
	if err != nil {
		t.Errorf("DecodeHeader should parse even with bad magic: %v", err)
	}
	if header.IsValid() {
		t.Error("Header.IsValid() should return false for invalid magic bytes")
	}
}
