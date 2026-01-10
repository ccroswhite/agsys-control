package engine

import (
	"os"
	"testing"
	"time"

	"github.com/agsys/property-controller/internal/protocol"
	"github.com/agsys/property-controller/internal/storage"
)

// MockLoRaDriver simulates the LoRa driver for testing
type MockLoRaDriver struct {
	sentMessages []*protocol.LoRaMessage
	onReceive    func(*protocol.LoRaMessage)
}

func NewMockLoRaDriver() *MockLoRaDriver {
	return &MockLoRaDriver{
		sentMessages: make([]*protocol.LoRaMessage, 0),
	}
}

func (m *MockLoRaDriver) Start() error                                      { return nil }
func (m *MockLoRaDriver) Stop() error                                       { return nil }
func (m *MockLoRaDriver) SetReceiveCallback(cb func(*protocol.LoRaMessage)) { m.onReceive = cb }
func (m *MockLoRaDriver) Send(msg *protocol.LoRaMessage) error {
	m.sentMessages = append(m.sentMessages, msg)
	return nil
}
func (m *MockLoRaDriver) SendToDevice(uid [8]byte, msgType uint8, payload []byte) error {
	msg := &protocol.LoRaMessage{
		Header: protocol.Header{
			Magic:     [2]byte{protocol.MagicByte1, protocol.MagicByte2},
			Version:   protocol.ProtocolVersion,
			MsgType:   msgType,
			DeviceUID: uid,
		},
		Payload: payload,
	}
	return m.Send(msg)
}
func (m *MockLoRaDriver) Broadcast(msgType uint8, payload []byte) error { return nil }

// SimulateReceive simulates receiving a message from a device
func (m *MockLoRaDriver) SimulateReceive(msg *protocol.LoRaMessage) {
	if m.onReceive != nil {
		m.onReceive(msg)
	}
}

// GetSentMessages returns all messages sent through the mock driver
func (m *MockLoRaDriver) GetSentMessages() []*protocol.LoRaMessage {
	return m.sentMessages
}

// ClearSentMessages clears the sent message buffer
func (m *MockLoRaDriver) ClearSentMessages() {
	m.sentMessages = make([]*protocol.LoRaMessage, 0)
}

// MockCloudClient simulates the cloud gRPC client for testing
type MockCloudClient struct {
	connected   bool
	sensorData  []interface{}
	meterData   []interface{}
	meterAlarms []interface{}
	valveStatus []interface{}
}

func NewMockCloudClient() *MockCloudClient {
	return &MockCloudClient{connected: true}
}

func (m *MockCloudClient) IsConnected() bool   { return m.connected }
func (m *MockCloudClient) SetConnected(c bool) { m.connected = c }

// testHelper creates a test engine with mock dependencies
func setupTestEngine(t *testing.T) (*Engine, *MockLoRaDriver, *storage.DB, func()) {
	t.Helper()

	// Create temp database
	tmpFile, err := os.CreateTemp("", "agsys-test-*.db")
	if err != nil {
		t.Fatalf("Failed to create temp db: %v", err)
	}
	tmpFile.Close()

	db, err := storage.Open(tmpFile.Name())
	if err != nil {
		os.Remove(tmpFile.Name())
		t.Fatalf("Failed to open database: %v", err)
	}

	mockLora := NewMockLoRaDriver()

	cleanup := func() {
		db.Close()
		os.Remove(tmpFile.Name())
	}

	// Note: We can't fully construct Engine without more mocking
	// This is a simplified test setup
	return nil, mockLora, db, cleanup
}

// TestMeterAlarmStorage tests that meter alarms are stored correctly
func TestMeterAlarmStorage(t *testing.T) {
	// Create temp database
	tmpFile, err := os.CreateTemp("", "agsys-test-*.db")
	if err != nil {
		t.Fatalf("Failed to create temp db: %v", err)
	}
	defer os.Remove(tmpFile.Name())
	tmpFile.Close()

	db, err := storage.Open(tmpFile.Name())
	if err != nil {
		t.Fatalf("Failed to open database: %v", err)
	}
	defer db.Close()

	// Insert a meter alarm
	alarm := &storage.MeterAlarm{
		DeviceUID:   "0102030405060708",
		AlarmType:   protocol.MeterAlarmLeak,
		FlowRateLPM: 15.5,
		DurationSec: 3600,
		TotalLiters: 50000,
		RSSI:        -85,
		Timestamp:   time.Now(),
	}

	id, err := db.InsertMeterAlarm(alarm)
	if err != nil {
		t.Fatalf("InsertMeterAlarm failed: %v", err)
	}
	if id <= 0 {
		t.Error("Expected positive ID from insert")
	}

	// Retrieve unsynced alarms
	alarms, err := db.GetUnsyncedMeterAlarms(10)
	if err != nil {
		t.Fatalf("GetUnsyncedMeterAlarms failed: %v", err)
	}
	if len(alarms) != 1 {
		t.Fatalf("Expected 1 alarm, got %d", len(alarms))
	}

	retrieved := alarms[0]
	if retrieved.DeviceUID != alarm.DeviceUID {
		t.Errorf("DeviceUID mismatch: got %s, want %s", retrieved.DeviceUID, alarm.DeviceUID)
	}
	if retrieved.AlarmType != alarm.AlarmType {
		t.Errorf("AlarmType mismatch: got %d, want %d", retrieved.AlarmType, alarm.AlarmType)
	}
	if retrieved.DurationSec != alarm.DurationSec {
		t.Errorf("DurationSec mismatch: got %d, want %d", retrieved.DurationSec, alarm.DurationSec)
	}

	// Mark as synced
	err = db.MarkMeterAlarmSynced(id)
	if err != nil {
		t.Fatalf("MarkMeterAlarmSynced failed: %v", err)
	}

	// Should no longer appear in unsynced
	alarms, err = db.GetUnsyncedMeterAlarms(10)
	if err != nil {
		t.Fatalf("GetUnsyncedMeterAlarms failed: %v", err)
	}
	if len(alarms) != 0 {
		t.Errorf("Expected 0 unsynced alarms after marking synced, got %d", len(alarms))
	}
}

// TestWaterMeterReadingStorage tests water meter reading storage
func TestWaterMeterReadingStorage(t *testing.T) {
	tmpFile, err := os.CreateTemp("", "agsys-test-*.db")
	if err != nil {
		t.Fatalf("Failed to create temp db: %v", err)
	}
	defer os.Remove(tmpFile.Name())
	tmpFile.Close()

	db, err := storage.Open(tmpFile.Name())
	if err != nil {
		t.Fatalf("Failed to open database: %v", err)
	}
	defer db.Close()

	reading := &storage.WaterMeterReading{
		DeviceUID:   "0102030405060708",
		TotalLiters: 12345,
		FlowRateLPM: 5.5,
		BatteryMV:   3700,
		RSSI:        -90,
		Timestamp:   time.Now(),
	}

	id, err := db.InsertWaterMeterReading(reading)
	if err != nil {
		t.Fatalf("InsertWaterMeterReading failed: %v", err)
	}
	if id <= 0 {
		t.Error("Expected positive ID from insert")
	}

	// Retrieve unsynced readings
	readings, err := db.GetUnsyncedWaterMeterReadings(10)
	if err != nil {
		t.Fatalf("GetUnsyncedWaterMeterReadings failed: %v", err)
	}
	if len(readings) != 1 {
		t.Fatalf("Expected 1 reading, got %d", len(readings))
	}

	if readings[0].TotalLiters != reading.TotalLiters {
		t.Errorf("TotalLiters mismatch: got %d, want %d", readings[0].TotalLiters, reading.TotalLiters)
	}
}

// TestDeviceUpsert tests device registration and updates
func TestDeviceUpsert(t *testing.T) {
	tmpFile, err := os.CreateTemp("", "agsys-test-*.db")
	if err != nil {
		t.Fatalf("Failed to create temp db: %v", err)
	}
	defer os.Remove(tmpFile.Name())
	tmpFile.Close()

	db, err := storage.Open(tmpFile.Name())
	if err != nil {
		t.Fatalf("Failed to open database: %v", err)
	}
	defer db.Close()

	device := &storage.Device{
		UID:        "0102030405060708",
		DeviceType: protocol.DeviceTypeWaterMeter,
		Name:       "Water Meter 1",
		FirstSeen:  time.Now(),
		LastSeen:   time.Now(),
		BatteryMV:  3700,
		RSSI:       -85,
	}

	// First insert
	err = db.UpsertDevice(device)
	if err != nil {
		t.Fatalf("UpsertDevice (insert) failed: %v", err)
	}

	// Update with new values
	device.BatteryMV = 3600
	device.RSSI = -90
	device.LastSeen = time.Now()
	err = db.UpsertDevice(device)
	if err != nil {
		t.Fatalf("UpsertDevice (update) failed: %v", err)
	}

	// Retrieve and verify
	retrieved, err := db.GetDevice(device.UID)
	if err != nil {
		t.Fatalf("GetDevice failed: %v", err)
	}

	if retrieved.BatteryMV != 3600 {
		t.Errorf("BatteryMV not updated: got %d, want 3600", retrieved.BatteryMV)
	}
	if retrieved.RSSI != -90 {
		t.Errorf("RSSI not updated: got %d, want -90", retrieved.RSSI)
	}
}

// TestProtocolMessageFlow tests encoding a message like a device would,
// then decoding it like the property controller would
func TestProtocolMessageFlow(t *testing.T) {
	// Simulate device sending a meter alarm
	deviceUID := [8]byte{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}

	// Build header like device firmware would
	header := protocol.NewHeader(
		protocol.MsgTypeMeterAlarm,
		protocol.DeviceTypeWaterMeter,
		deviceUID,
		1234,
	)

	// Build alarm payload (matching C struct layout)
	alarmPayload := make([]byte, 16)
	// timestamp (4 bytes)
	timestamp := uint32(12345)
	alarmPayload[0] = byte(timestamp)
	alarmPayload[1] = byte(timestamp >> 8)
	alarmPayload[2] = byte(timestamp >> 16)
	alarmPayload[3] = byte(timestamp >> 24)
	// alarmType (1 byte)
	alarmPayload[4] = protocol.MeterAlarmLeak
	// flowRateLPM (2 bytes) - 15.0 L/min = 150
	flowRate := uint16(150)
	alarmPayload[5] = byte(flowRate)
	alarmPayload[6] = byte(flowRate >> 8)
	// durationSec (4 bytes)
	duration := uint32(3600)
	alarmPayload[7] = byte(duration)
	alarmPayload[8] = byte(duration >> 8)
	alarmPayload[9] = byte(duration >> 16)
	alarmPayload[10] = byte(duration >> 24)
	// totalLiters (4 bytes)
	total := uint32(50000)
	alarmPayload[11] = byte(total)
	alarmPayload[12] = byte(total >> 8)
	alarmPayload[13] = byte(total >> 16)
	alarmPayload[14] = byte(total >> 24)
	// flags (1 byte)
	alarmPayload[15] = 0x01

	// Create full message
	msg := &protocol.LoRaMessage{
		Header:  *header,
		Payload: alarmPayload,
		RSSI:    -85,
	}

	// Verify header is valid
	if !msg.Header.IsValid() {
		t.Error("Header should be valid")
	}

	// Decode like property controller would
	decoded, err := protocol.DecodeMeterAlarm(msg.Payload)
	if err != nil {
		t.Fatalf("DecodeMeterAlarm failed: %v", err)
	}

	// Verify decoded values
	if decoded.Timestamp != timestamp {
		t.Errorf("Timestamp mismatch: got %d, want %d", decoded.Timestamp, timestamp)
	}
	if decoded.AlarmType != protocol.MeterAlarmLeak {
		t.Errorf("AlarmType mismatch: got %d, want %d", decoded.AlarmType, protocol.MeterAlarmLeak)
	}
	if decoded.FlowRateLPM != flowRate {
		t.Errorf("FlowRateLPM mismatch: got %d, want %d", decoded.FlowRateLPM, flowRate)
	}
	if decoded.DurationSec != duration {
		t.Errorf("DurationSec mismatch: got %d, want %d", decoded.DurationSec, duration)
	}
	if decoded.TotalLiters != total {
		t.Errorf("TotalLiters mismatch: got %d, want %d", decoded.TotalLiters, total)
	}
}

// TestConfigUpdateFlow tests sending config from controller to device
func TestConfigUpdateFlow(t *testing.T) {
	// Property controller creates config
	config := &protocol.MeterConfigPayload{
		ConfigVersion:     5,
		ReportIntervalSec: 120,
		PulsesPerLiter:    45000,
		LeakThresholdMin:  30,
		MaxFlowRateLPM:    500,
		Flags:             protocol.MeterCfgLeakDetectEn,
	}

	// Encode for transmission
	encoded := config.Encode()

	// Simulate device receiving and parsing
	// (This matches what the C firmware would do)
	if len(encoded) < 11 {
		t.Fatalf("Encoded config too short: %d bytes", len(encoded))
	}

	// Parse like C firmware
	parsedVersion := uint16(encoded[0]) | uint16(encoded[1])<<8
	parsedInterval := uint16(encoded[2]) | uint16(encoded[3])<<8
	parsedPulses := uint16(encoded[4]) | uint16(encoded[5])<<8
	parsedLeak := uint16(encoded[6]) | uint16(encoded[7])<<8
	parsedMaxFlow := uint16(encoded[8]) | uint16(encoded[9])<<8
	parsedFlags := encoded[10]

	if parsedVersion != config.ConfigVersion {
		t.Errorf("ConfigVersion mismatch: got %d, want %d", parsedVersion, config.ConfigVersion)
	}
	if parsedInterval != config.ReportIntervalSec {
		t.Errorf("ReportIntervalSec mismatch: got %d, want %d", parsedInterval, config.ReportIntervalSec)
	}
	if parsedPulses != config.PulsesPerLiter {
		t.Errorf("PulsesPerLiter mismatch: got %d, want %d", parsedPulses, config.PulsesPerLiter)
	}
	if parsedLeak != config.LeakThresholdMin {
		t.Errorf("LeakThresholdMin mismatch: got %d, want %d", parsedLeak, config.LeakThresholdMin)
	}
	if parsedMaxFlow != config.MaxFlowRateLPM {
		t.Errorf("MaxFlowRateLPM mismatch: got %d, want %d", parsedMaxFlow, config.MaxFlowRateLPM)
	}
	if parsedFlags != config.Flags {
		t.Errorf("Flags mismatch: got %d, want %d", parsedFlags, config.Flags)
	}
}
