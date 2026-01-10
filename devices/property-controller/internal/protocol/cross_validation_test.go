package protocol

import (
	"encoding/hex"
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
)

// TestVectors represents the JSON structure from C test vector generator
type TestVectors struct {
	MeterAlarms  []MeterAlarmVector  `json:"meter_alarms"`
	MeterConfigs []MeterConfigVector `json:"meter_configs"`
	MeterResets  []MeterResetVector  `json:"meter_resets"`
	Acks         []AckVector         `json:"acks"`
	Headers      []HeaderVector      `json:"headers"`
}

type MeterAlarmVector struct {
	Timestamp   uint32 `json:"timestamp"`
	AlarmType   uint8  `json:"alarm_type"`
	FlowRateLPM uint16 `json:"flow_rate_lpm"`
	DurationSec uint32 `json:"duration_sec"`
	TotalLiters uint32 `json:"total_liters"`
	Flags       uint8  `json:"flags"`
	Encoded     string `json:"encoded"`
}

type MeterConfigVector struct {
	ConfigVersion     uint16 `json:"config_version"`
	ReportIntervalSec uint16 `json:"report_interval_sec"`
	PulsesPerLiter    uint16 `json:"pulses_per_liter"`
	LeakThresholdMin  uint16 `json:"leak_threshold_min"`
	MaxFlowRateLPM    uint16 `json:"max_flow_rate_lpm"`
	Flags             uint8  `json:"flags"`
	Encoded           string `json:"encoded"`
}

type MeterResetVector struct {
	CommandID      uint16 `json:"command_id"`
	ResetType      uint8  `json:"reset_type"`
	NewTotalLiters uint32 `json:"new_total_liters"`
	Encoded        string `json:"encoded"`
}

type AckVector struct {
	AckedSequence uint16 `json:"acked_sequence"`
	Status        uint8  `json:"status"`
	Flags         uint8  `json:"flags"`
	Encoded       string `json:"encoded"`
}

type HeaderVector struct {
	Version    uint8  `json:"version"`
	MsgType    uint8  `json:"msg_type"`
	DeviceType uint8  `json:"device_type"`
	Sequence   uint16 `json:"sequence"`
	DeviceUID  string `json:"device_uid"`
	Encoded    string `json:"encoded"`
}

func loadTestVectors(t *testing.T) *TestVectors {
	t.Helper()

	// Try multiple paths to find the test vectors file
	paths := []string{
		"../../../../common/test/test_vectors.json",
		"../../../devices/common/test/test_vectors.json",
		filepath.Join(os.Getenv("HOME"), "src/agsys-control/devices/common/test/test_vectors.json"),
	}

	var data []byte
	var err error
	for _, path := range paths {
		data, err = os.ReadFile(path)
		if err == nil {
			break
		}
	}

	if err != nil {
		t.Skipf("Test vectors file not found (run 'make vectors' in devices/common/test): %v", err)
		return nil
	}

	var vectors TestVectors
	if err := json.Unmarshal(data, &vectors); err != nil {
		t.Fatalf("Failed to parse test vectors: %v", err)
	}

	return &vectors
}

// TestCrossValidateMeterAlarm validates Go decoding of C-encoded MeterAlarm
func TestCrossValidateMeterAlarm(t *testing.T) {
	vectors := loadTestVectors(t)
	if vectors == nil {
		return
	}

	for i, v := range vectors.MeterAlarms {
		t.Run(string(rune('A'+i)), func(t *testing.T) {
			// Decode hex string from C
			encoded, err := hex.DecodeString(v.Encoded)
			if err != nil {
				t.Fatalf("Invalid hex in test vector: %v", err)
			}

			// Decode using Go
			decoded, err := DecodeMeterAlarm(encoded)
			if err != nil {
				t.Fatalf("DecodeMeterAlarm failed: %v", err)
			}

			// Validate all fields match
			if decoded.Timestamp != v.Timestamp {
				t.Errorf("Timestamp: got %d, want %d", decoded.Timestamp, v.Timestamp)
			}
			if decoded.AlarmType != v.AlarmType {
				t.Errorf("AlarmType: got %d, want %d", decoded.AlarmType, v.AlarmType)
			}
			if decoded.FlowRateLPM != v.FlowRateLPM {
				t.Errorf("FlowRateLPM: got %d, want %d", decoded.FlowRateLPM, v.FlowRateLPM)
			}
			if decoded.DurationSec != v.DurationSec {
				t.Errorf("DurationSec: got %d, want %d", decoded.DurationSec, v.DurationSec)
			}
			if decoded.TotalLiters != v.TotalLiters {
				t.Errorf("TotalLiters: got %d, want %d", decoded.TotalLiters, v.TotalLiters)
			}
			if decoded.Flags != v.Flags {
				t.Errorf("Flags: got %d, want %d", decoded.Flags, v.Flags)
			}
		})
	}
}

// TestCrossValidateMeterConfig validates Go encoding matches C encoding
func TestCrossValidateMeterConfig(t *testing.T) {
	vectors := loadTestVectors(t)
	if vectors == nil {
		return
	}

	for i, v := range vectors.MeterConfigs {
		t.Run(string(rune('A'+i)), func(t *testing.T) {
			// Create Go struct with same values
			config := MeterConfigPayload{
				ConfigVersion:     v.ConfigVersion,
				ReportIntervalSec: v.ReportIntervalSec,
				PulsesPerLiter:    v.PulsesPerLiter,
				LeakThresholdMin:  v.LeakThresholdMin,
				MaxFlowRateLPM:    v.MaxFlowRateLPM,
				Flags:             v.Flags,
			}

			// Encode using Go
			encoded := config.Encode()
			encodedHex := hex.EncodeToString(encoded)

			// Compare with C encoding
			if encodedHex != v.Encoded {
				t.Errorf("Encoding mismatch:\n  Go: %s\n  C:  %s", encodedHex, v.Encoded)
			}
		})
	}
}

// TestCrossValidateMeterReset validates Go encoding matches C encoding
func TestCrossValidateMeterReset(t *testing.T) {
	vectors := loadTestVectors(t)
	if vectors == nil {
		return
	}

	for i, v := range vectors.MeterResets {
		t.Run(string(rune('A'+i)), func(t *testing.T) {
			// Create Go struct with same values
			reset := MeterResetTotalPayload{
				CommandID:      v.CommandID,
				ResetType:      v.ResetType,
				NewTotalLiters: v.NewTotalLiters,
			}

			// Encode using Go
			encoded := reset.Encode()
			encodedHex := hex.EncodeToString(encoded)

			// Compare with C encoding
			if encodedHex != v.Encoded {
				t.Errorf("Encoding mismatch:\n  Go: %s\n  C:  %s", encodedHex, v.Encoded)
			}
		})
	}
}

// TestCrossValidateAck validates Go encoding/decoding matches C
func TestCrossValidateAck(t *testing.T) {
	vectors := loadTestVectors(t)
	if vectors == nil {
		return
	}

	for i, v := range vectors.Acks {
		t.Run(string(rune('A'+i)), func(t *testing.T) {
			// Test encoding
			ack := AckPayload{
				AckedSequence: v.AckedSequence,
				Status:        v.Status,
				Flags:         v.Flags,
			}

			encoded := ack.Encode()
			encodedHex := hex.EncodeToString(encoded)

			if encodedHex != v.Encoded {
				t.Errorf("Encoding mismatch:\n  Go: %s\n  C:  %s", encodedHex, v.Encoded)
			}

			// Test decoding
			cEncoded, _ := hex.DecodeString(v.Encoded)
			decoded, err := DecodeAck(cEncoded)
			if err != nil {
				t.Fatalf("DecodeAck failed: %v", err)
			}

			if decoded.AckedSequence != v.AckedSequence {
				t.Errorf("AckedSequence: got %d, want %d", decoded.AckedSequence, v.AckedSequence)
			}
			if decoded.Status != v.Status {
				t.Errorf("Status: got %d, want %d", decoded.Status, v.Status)
			}
			if decoded.Flags != v.Flags {
				t.Errorf("Flags: got %d, want %d", decoded.Flags, v.Flags)
			}
		})
	}
}

// TestCrossValidateHeader validates Go header encoding/decoding matches C
func TestCrossValidateHeader(t *testing.T) {
	vectors := loadTestVectors(t)
	if vectors == nil {
		return
	}

	for i, v := range vectors.Headers {
		t.Run(string(rune('A'+i)), func(t *testing.T) {
			// Decode C-encoded header
			cEncoded, err := hex.DecodeString(v.Encoded)
			if err != nil {
				t.Fatalf("Invalid hex: %v", err)
			}

			decoded, err := DecodeHeader(cEncoded)
			if err != nil {
				t.Fatalf("DecodeHeader failed: %v", err)
			}

			// Validate fields
			if decoded.Version != v.Version {
				t.Errorf("Version: got %d, want %d", decoded.Version, v.Version)
			}
			if decoded.MsgType != v.MsgType {
				t.Errorf("MsgType: got %d, want %d", decoded.MsgType, v.MsgType)
			}
			if decoded.DeviceType != v.DeviceType {
				t.Errorf("DeviceType: got %d, want %d", decoded.DeviceType, v.DeviceType)
			}
			if decoded.Sequence != v.Sequence {
				t.Errorf("Sequence: got %d, want %d", decoded.Sequence, v.Sequence)
			}

			// Validate magic bytes
			if decoded.Magic[0] != MagicByte1 || decoded.Magic[1] != MagicByte2 {
				t.Errorf("Magic bytes wrong: got [%02X %02X], want [%02X %02X]",
					decoded.Magic[0], decoded.Magic[1], MagicByte1, MagicByte2)
			}

			// Test Go encoding matches C
			uidBytes, _ := hex.DecodeString(v.DeviceUID)
			var uid [8]byte
			copy(uid[:], uidBytes)

			header := NewHeader(v.MsgType, v.DeviceType, uid, v.Sequence)
			goEncoded := header.Encode()
			goEncodedHex := hex.EncodeToString(goEncoded)

			if goEncodedHex != v.Encoded {
				t.Errorf("Header encoding mismatch:\n  Go: %s\n  C:  %s", goEncodedHex, v.Encoded)
			}
		})
	}
}
