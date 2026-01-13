// Package engine provides the core logic for the property controller,
// routing messages between LoRa devices and the cloud.
package engine

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"sync"
	"sync/atomic"
	"time"

	"github.com/agsys/property-controller/internal/cloud"
	"github.com/agsys/property-controller/internal/lora"
	"github.com/agsys/property-controller/internal/ota"
	"github.com/agsys/property-controller/internal/protocol"
	"github.com/agsys/property-controller/internal/storage"
	controllerv1 "github.com/ccroswhite/agsys-api/gen/go/proto/controller/v1"
	"google.golang.org/protobuf/types/known/timestamppb"
)

// Config holds engine configuration
type Config struct {
	DatabasePath     string
	GRPCAddr         string // gRPC server address (e.g., "grpc.agsys.io:443")
	ControllerID     string // Controller UUID
	APIKey           string
	UseTLS           bool // Use TLS for gRPC connection
	AESKey           []byte
	LoRaFrequency    uint32
	CommandTimeout   time.Duration
	CommandRetries   int
	SyncInterval     time.Duration
	TimeSyncInterval time.Duration
	FirmwareVersion  string
}

// DefaultConfig returns default engine configuration
func DefaultConfig() Config {
	return Config{
		DatabasePath:     "/var/lib/agsys/controller.db",
		GRPCAddr:         "localhost:50051",
		UseTLS:           false,
		LoRaFrequency:    915000000,
		CommandTimeout:   10 * time.Second,
		CommandRetries:   3,
		SyncInterval:     30 * time.Second,
		TimeSyncInterval: 1 * time.Hour,
		FirmwareVersion:  "1.0.0",
	}
}

// Engine is the core controller that routes messages between devices and cloud
type Engine struct {
	config    Config
	db        *storage.DB
	lora      *lora.Driver
	cloud     *cloud.GRPCClient
	ota       *ota.Manager
	stopChan  chan struct{}
	wg        sync.WaitGroup
	mu        sync.RWMutex
	commandID uint32

	// Registered devices (from cloud)
	registeredDevices map[string]*storage.Device

	// Device firmware versions (updated from reports)
	deviceVersions map[string]ota.Version
}

// New creates a new engine instance
func New(config Config) (*Engine, error) {
	// Open database
	db, err := storage.Open(config.DatabasePath)
	if err != nil {
		return nil, fmt.Errorf("failed to open database: %w", err)
	}

	// Create LoRa driver
	loraConfig := lora.DefaultConfig()
	loraConfig.Frequency = config.LoRaFrequency
	loraConfig.AESKey = config.AESKey

	loraDriver, err := lora.New(loraConfig)
	if err != nil {
		db.Close()
		return nil, fmt.Errorf("failed to create LoRa driver: %w", err)
	}

	// Create gRPC cloud client
	grpcConfig := cloud.DefaultGRPCConfig()
	grpcConfig.ServerAddr = config.GRPCAddr
	grpcConfig.ControllerID = config.ControllerID
	grpcConfig.APIKey = config.APIKey
	grpcConfig.UseTLS = config.UseTLS

	cloudClient := cloud.NewGRPCClient(grpcConfig)
	cloudClient.SetFirmwareVersion(config.FirmwareVersion)

	// Create firmware client for OTA downloads
	firmwareClient := cloud.NewFirmwareClient(grpcConfig)

	// Create OTA manager
	otaConfig := ota.DefaultConfig()
	otaSendFunc := func(deviceUID [8]byte, msgType uint8, payload []byte) error {
		return loraDriver.SendToDevice(deviceUID, msgType, payload)
	}
	otaManager, err := ota.New(otaConfig, otaSendFunc, firmwareClient)
	if err != nil {
		db.Close()
		loraDriver.Stop()
		return nil, fmt.Errorf("failed to create OTA manager: %w", err)
	}

	return &Engine{
		config:            config,
		db:                db,
		lora:              loraDriver,
		cloud:             cloudClient,
		ota:               otaManager,
		stopChan:          make(chan struct{}),
		registeredDevices: make(map[string]*storage.Device),
		deviceVersions:    make(map[string]ota.Version),
	}, nil
}

// Start starts the engine
func (e *Engine) Start(ctx context.Context) error {
	// Set up LoRa receive callback
	e.lora.SetReceiveCallback(e.handleLoRaMessage)

	// Set up gRPC callbacks for messages from cloud
	e.cloud.SetValveCommandHandler(e.handleValveCommandGRPC)
	e.cloud.SetScheduleHandler(e.handleScheduleUpdateGRPC)
	e.cloud.SetDeviceAddedHandler(e.handleDeviceAddedGRPC)
	e.cloud.SetConfigUpdateHandler(e.handleConfigUpdateGRPC)

	// Start LoRa driver
	if err := e.lora.Start(); err != nil {
		return fmt.Errorf("failed to start LoRa driver: %w", err)
	}

	// Start OTA manager
	if err := e.ota.Start(ctx); err != nil {
		return fmt.Errorf("failed to start OTA manager: %w", err)
	}

	// Connect to cloud (with automatic reconnection)
	go e.cloud.ConnectWithRetry(ctx)

	// Start background tasks
	e.wg.Add(1)
	go e.cloudSyncLoop(ctx)

	e.wg.Add(1)
	go e.commandRetryLoop(ctx)

	e.wg.Add(1)
	go e.timeSyncLoop(ctx)

	log.Println("Engine started")
	return nil
}

// Stop stops the engine
func (e *Engine) Stop() error {
	close(e.stopChan)
	e.wg.Wait()

	if err := e.cloud.Close(); err != nil {
		log.Printf("Error stopping cloud client: %v", err)
	}

	// Stop OTA manager
	e.ota.Stop()

	if err := e.lora.Stop(); err != nil {
		log.Printf("Error stopping LoRa driver: %v", err)
	}

	if err := e.db.Close(); err != nil {
		log.Printf("Error closing database: %v", err)
	}

	log.Println("Engine stopped")
	return nil
}

// handleLoRaMessage processes incoming LoRa messages from devices
func (e *Engine) handleLoRaMessage(msg *protocol.LoRaMessage) {
	deviceUID := msg.DeviceUIDString()

	// Check if device is registered
	e.mu.RLock()
	device, registered := e.registeredDevices[deviceUID]
	e.mu.RUnlock()

	if !registered {
		log.Printf("Message from unregistered device: %s", deviceUID)
		// Still process but mark as unregistered
		device = &storage.Device{
			UID:          deviceUID,
			IsRegistered: false,
		}
	}

	// Update device last seen
	now := time.Now()
	device.LastSeen = now
	device.RSSI = msg.RSSI
	e.db.UpsertDevice(device)

	// Process based on message type
	switch msg.Header.MsgType {
	case protocol.MsgTypeSensorReport:
		e.handleSensorData(deviceUID, msg)

	case protocol.MsgTypeWaterMeterReport:
		e.handleWaterMeterData(deviceUID, msg)

	case protocol.MsgTypeMeterAlarm:
		e.handleMeterAlarm(deviceUID, msg)

	case protocol.MsgTypeValveStatus:
		e.handleValveStatus(deviceUID, msg)

	case protocol.MsgTypeValveAck:
		e.handleValveAck(deviceUID, msg)

	case protocol.MsgTypeScheduleRequest:
		e.handleScheduleRequest(deviceUID, msg)

	case protocol.MsgTypeHeartbeat:
		log.Printf("Heartbeat from %s, RSSI: %d", deviceUID, msg.RSSI)

	case protocol.MsgTypeOTARequest:
		if err := e.ota.HandleOTARequest(deviceUID, msg.Header.DeviceType, msg.Payload); err != nil {
			log.Printf("Failed to handle OTA request from %s: %v", deviceUID, err)
		}

	case protocol.MsgTypeOTAReady:
		if err := e.ota.HandleOTAReady(deviceUID, msg.Payload); err != nil {
			log.Printf("Failed to handle OTA ready from %s: %v", deviceUID, err)
		}

	case protocol.MsgTypeOTAStatus:
		if err := e.ota.HandleOTAStatus(deviceUID, msg.Payload); err != nil {
			log.Printf("Failed to handle OTA status from %s: %v", deviceUID, err)
		}

	default:
		log.Printf("Unknown message type 0x%02X from %s", msg.Header.MsgType, deviceUID)
	}
}

// handleSensorData processes soil moisture sensor data
func (e *Engine) handleSensorData(deviceUID string, msg *protocol.LoRaMessage) {
	data, err := protocol.DecodeSensorData(msg.Payload)
	if err != nil {
		log.Printf("Failed to decode sensor data from %s: %v", deviceUID, err)
		return
	}

	// Store in database
	reading := &storage.SoilMoistureReading{
		DeviceUID:       deviceUID,
		ProbeID:         data.ProbeID,
		MoistureRaw:     data.MoistureRaw,
		MoisturePercent: data.MoisturePercent,
		Temperature:     data.Temperature,
		BatteryMV:       data.BatteryMV,
		RSSI:            msg.RSSI,
		Timestamp:       time.Now(),
	}

	id, err := e.db.InsertSoilMoistureReading(reading)
	if err != nil {
		log.Printf("Failed to store sensor reading: %v", err)
		return
	}

	log.Printf("Sensor data from %s probe %d: %d%% moisture, %d°C, %dmV battery",
		deviceUID, data.ProbeID, data.MoisturePercent, data.Temperature/10, data.BatteryMV)

	// Queue for cloud sync
	e.queueForCloudSync("sensor", id, reading)
}

// handleWaterMeterData processes water meter data
func (e *Engine) handleWaterMeterData(deviceUID string, msg *protocol.LoRaMessage) {
	data, err := protocol.DecodeWaterMeter(msg.Payload)
	if err != nil {
		log.Printf("Failed to decode water meter data from %s: %v", deviceUID, err)
		return
	}

	// Store in database
	reading := &storage.WaterMeterReading{
		DeviceUID:   deviceUID,
		TotalLiters: data.TotalLiters,
		FlowRateLPM: float32(data.FlowRateLPM) / 10.0,
		BatteryMV:   data.BatteryMV,
		RSSI:        msg.RSSI,
		Timestamp:   time.Now(),
	}

	id, err := e.db.InsertWaterMeterReading(reading)
	if err != nil {
		log.Printf("Failed to store water meter reading: %v", err)
		return
	}

	log.Printf("Water meter from %s: %d L total, %.1f L/min flow",
		deviceUID, data.TotalLiters, reading.FlowRateLPM)

	// Queue for cloud sync
	e.queueForCloudSync("meter", id, reading)
}

// handleMeterAlarm processes water meter alarm messages
func (e *Engine) handleMeterAlarm(deviceUID string, msg *protocol.LoRaMessage) {
	alarm, err := protocol.DecodeMeterAlarm(msg.Payload)
	if err != nil {
		log.Printf("Failed to decode meter alarm from %s: %v", deviceUID, err)
		return
	}

	alarmTypeStr := protocol.MeterAlarmTypeString(alarm.AlarmType)
	log.Printf("ALARM from water meter %s: %s, flow: %.1f L/min, duration: %ds",
		deviceUID, alarmTypeStr, float32(alarm.FlowRateLPM)/10.0, alarm.DurationSec)

	// Store alarm in database
	meterAlarm := &storage.MeterAlarm{
		DeviceUID:   deviceUID,
		AlarmType:   alarm.AlarmType,
		FlowRateLPM: float32(alarm.FlowRateLPM) / 10.0,
		DurationSec: alarm.DurationSec,
		TotalLiters: alarm.TotalLiters,
		RSSI:        msg.RSSI,
		Timestamp:   time.Now(),
	}

	id, err := e.db.InsertMeterAlarm(meterAlarm)
	if err != nil {
		log.Printf("Failed to store meter alarm: %v", err)
		return
	}

	// Queue for immediate cloud sync (high priority)
	e.queueForCloudSync("meter_alarm", id, meterAlarm)

	// If alarm is active (not cleared), send to cloud immediately
	if alarm.AlarmType != protocol.MeterAlarmCleared {
		go e.sendAlarmToCloud(deviceUID, meterAlarm)
	}
}

// sendAlarmToCloud sends an alarm to the cloud immediately
func (e *Engine) sendAlarmToCloud(deviceUID string, alarm *storage.MeterAlarm) {
	if !e.cloud.IsConnected() {
		log.Printf("Cannot send alarm to cloud: not connected")
		return
	}

	// Convert storage.MeterAlarm to cloud.MeterAlarmData
	alarmData := &cloud.MeterAlarmData{
		AlarmType:   alarm.AlarmType,
		FlowRateLPM: alarm.FlowRateLPM,
		DurationSec: alarm.DurationSec,
		TotalLiters: alarm.TotalLiters,
		RSSI:        alarm.RSSI,
		Timestamp:   alarm.Timestamp,
	}

	if err := e.cloud.SendMeterAlarm(deviceUID, alarmData); err != nil {
		log.Printf("Failed to send alarm to cloud: %v", err)
	} else {
		log.Printf("Alarm sent to cloud for device %s", deviceUID)
	}
}

// SendAck sends an acknowledgment to a device
func (e *Engine) SendAck(deviceUID string, deviceType uint8, sequence uint16, status uint8, flags uint8) error {
	uid, err := lora.ParseDeviceUID(deviceUID)
	if err != nil {
		return fmt.Errorf("invalid device UID: %w", err)
	}

	// Check if OTA is pending for this device
	e.mu.RLock()
	currentVersion, hasVersion := e.deviceVersions[deviceUID]
	e.mu.RUnlock()

	if hasVersion && e.ota.ShouldSetOTAPending(deviceUID, deviceType, currentVersion) {
		flags |= protocol.AckFlagOTAPending
		log.Printf("Setting OTA_PENDING flag for device %s", deviceUID)
	}

	ack := &protocol.AckPayload{
		AckedSequence: sequence,
		Status:        status,
		Flags:         flags,
	}

	payload := ack.Encode()
	return e.lora.SendToDevice(uid, protocol.MsgTypeAck, payload)
}

// SendMeterConfig sends a configuration update to a water meter device
func (e *Engine) SendMeterConfig(deviceUID string, config *protocol.MeterConfigPayload) error {
	uid, err := lora.ParseDeviceUID(deviceUID)
	if err != nil {
		return fmt.Errorf("invalid device UID: %w", err)
	}

	payload := config.Encode()
	return e.lora.SendToDevice(uid, protocol.MsgTypeConfigUpdate, payload)
}

// SendMeterReset sends a totalizer reset command to a water meter
func (e *Engine) SendMeterReset(deviceUID string, resetToZero bool, newTotal uint32) error {
	uid, err := lora.ParseDeviceUID(deviceUID)
	if err != nil {
		return fmt.Errorf("invalid device UID: %w", err)
	}

	// Generate command ID
	cmdID := uint16(atomic.AddUint32(&e.commandID, 1))

	resetType := uint8(0)
	if !resetToZero {
		resetType = 1
	}

	reset := &protocol.MeterResetTotalPayload{
		CommandID:      cmdID,
		ResetType:      resetType,
		NewTotalLiters: newTotal,
	}

	payload := reset.Encode()
	if err := e.lora.SendToDevice(uid, protocol.MsgTypeMeterResetTotal, payload); err != nil {
		return err
	}

	log.Printf("Sent meter reset to %s: cmdID=%d, resetType=%d", deviceUID, cmdID, resetType)
	return nil
}

// handleValveStatus processes valve status reports
func (e *Engine) handleValveStatus(deviceUID string, msg *protocol.LoRaMessage) {
	status, err := protocol.DecodeValveStatus(msg.Payload)
	if err != nil {
		log.Printf("Failed to decode valve status from %s: %v", deviceUID, err)
		return
	}

	// Update actuator state in database
	if err := e.db.UpdateValveActuatorState(deviceUID, status.ActuatorAddr, status.State); err != nil {
		log.Printf("Failed to update valve state: %v", err)
	}

	stateStr := valveStateString(status.State)
	log.Printf("Valve status from %s addr %d: %s, current: %dmA, flags: 0x%02X",
		deviceUID, status.ActuatorAddr, stateStr, status.CurrentMA, status.Flags)

	// Record event
	event := &storage.ValveEvent{
		ControllerUID: deviceUID,
		ActuatorAddr:  status.ActuatorAddr,
		NewState:      status.State,
		Source:        "status",
		Timestamp:     time.Now(),
	}

	id, err := e.db.InsertValveEvent(event)
	if err != nil {
		log.Printf("Failed to store valve event: %v", err)
		return
	}

	// Queue for cloud sync
	e.queueForCloudSync("valve_event", id, event)
}

// handleValveAck processes valve command acknowledgments
func (e *Engine) handleValveAck(deviceUID string, msg *protocol.LoRaMessage) {
	ack, err := protocol.DecodeValveAck(msg.Payload)
	if err != nil {
		log.Printf("Failed to decode valve ack from %s: %v", deviceUID, err)
		return
	}

	// Mark command as acknowledged
	if err := e.db.AcknowledgeCommand(ack.CommandID, ack.ResultState); err != nil {
		log.Printf("Failed to acknowledge command %d: %v", ack.CommandID, err)
	}

	// Update actuator state
	if err := e.db.UpdateValveActuatorState(deviceUID, ack.ActuatorAddr, ack.ResultState); err != nil {
		log.Printf("Failed to update valve state: %v", err)
	}

	successStr := "SUCCESS"
	if !ack.Success {
		successStr = "FAILED"
	}
	log.Printf("Valve ack from %s addr %d: cmd %d %s, state: %s",
		deviceUID, ack.ActuatorAddr, ack.CommandID, successStr, valveStateString(ack.ResultState))

	// Send acknowledgment to cloud via gRPC
	cmdIDStr := fmt.Sprintf("%d", ack.CommandID)
	errMsg := ""
	if !ack.Success {
		errMsg = "command failed"
	}
	if err := e.cloud.SendCommandAck(cmdIDStr, ack.Success, errMsg); err != nil {
		log.Printf("Failed to send valve ack to cloud: %v", err)
	}
}

// handleScheduleRequest processes schedule requests from valve controllers
func (e *Engine) handleScheduleRequest(deviceUID string, msg *protocol.LoRaMessage) {
	log.Printf("Schedule request from %s", deviceUID)

	// Get schedule for this controller
	schedule, entries, err := e.db.GetScheduleForController(deviceUID)
	if err != nil {
		log.Printf("No schedule found for %s: %v", deviceUID, err)
		return
	}

	// Convert to protocol format
	protoEntries := make([]protocol.ScheduleEntry, len(entries))
	for i, e := range entries {
		protoEntries[i] = protocol.ScheduleEntry{
			DayMask:      e.DayMask,
			StartHour:    e.StartHour,
			StartMinute:  e.StartMinute,
			DurationMins: e.DurationMins,
			ActuatorMask: e.ActuatorMask,
		}
	}

	// Send schedule to device
	uid, _ := lora.ParseDeviceUID(deviceUID)
	scheduleMsg := lora.CreateScheduleUpdateMessage(uid, schedule.Version, protoEntries)
	scheduleMsg.Header.Sequence = e.lora.GetNextSeqNum()

	if err := e.lora.Send(scheduleMsg); err != nil {
		log.Printf("Failed to send schedule to %s: %v", deviceUID, err)
	} else {
		log.Printf("Sent schedule v%d with %d entries to %s", schedule.Version, len(entries), deviceUID)
	}
}

// handleDeviceAdded processes device added notifications from the cloud
func (e *Engine) handleDeviceAdded(data json.RawMessage) {
	deviceInfo, err := cloud.ParseDeviceAdded(data)
	if err != nil {
		log.Printf("Failed to parse device added: %v", err)
		return
	}

	e.mu.Lock()
	defer e.mu.Unlock()

	// Add the new device
	device := &storage.Device{
		UID:          deviceInfo.DeviceUID,
		DeviceType:   deviceTypeFromString(deviceInfo.DeviceType),
		Name:         deviceInfo.Name,
		ZoneID:       deviceInfo.ZoneID,
		IsRegistered: true,
		FirstSeen:    time.Now(),
		LastSeen:     time.Now(),
	}
	e.registeredDevices[deviceInfo.DeviceUID] = device

	// Store in database
	if err := e.db.UpsertDevice(device); err != nil {
		log.Printf("Failed to store device %s: %v", deviceInfo.DeviceUID, err)
	}

	log.Printf("Device added: %s (%s) - %s", deviceInfo.DeviceUID, deviceInfo.DeviceType, deviceInfo.Name)
}

// handleConfigUpdate processes config updates from the cloud
func (e *Engine) handleConfigUpdate(data json.RawMessage) {
	cfgUpdate, err := cloud.ParseConfigUpdate(data)
	if err != nil {
		log.Printf("Failed to parse config update: %v", err)
		return
	}

	log.Printf("Config update received for target: %s", cfgUpdate.Target)
	// TODO: Apply configuration changes
}

// handleScheduleUpdate processes schedule updates from the cloud
func (e *Engine) handleScheduleUpdate(data json.RawMessage) {
	update, err := cloud.ParseScheduleUpdate(data)
	if err != nil {
		log.Printf("Failed to parse schedule update: %v", err)
		return
	}

	// Process each schedule in the update
	for _, sched := range update.Schedules {
		// Convert days to day mask
		dayMask := daysToDayMask(sched.Days)
		startHour, startMinute := parseStartTime(sched.StartTime)

		// Convert to storage format
		schedule := &storage.Schedule{
			UID:      sched.ScheduleID,
			Name:     sched.Name,
			IsActive: sched.Enabled,
		}

		// Create a single entry for this schedule
		var actuatorMask uint64
		for _, v := range sched.Valves {
			actuatorMask |= (1 << v.ActuatorAddress)
		}

		entries := []storage.ScheduleEntry{{
			DayMask:      dayMask,
			StartHour:    startHour,
			StartMinute:  startMinute,
			DurationMins: uint16(sched.DurationMinutes),
			ActuatorMask: actuatorMask,
		}}

		// Store in database
		if err := e.db.UpsertSchedule(schedule, entries); err != nil {
			log.Printf("Failed to store schedule: %v", err)
			continue
		}

		log.Printf("Updated schedule %s: %s", sched.ScheduleID, sched.Name)
	}
}

// deviceTypeFromString converts a device type string to uint8
func deviceTypeFromString(s string) uint8 {
	switch s {
	case "soil_moisture":
		return 0x01
	case "valve_controller":
		return 0x02
	case "water_meter":
		return 0x03
	case "valve_actuator":
		return 0x04
	default:
		return 0x00
	}
}

// daysToDayMask converts a slice of day strings to a bitmask
func daysToDayMask(days []string) uint8 {
	var mask uint8
	for _, day := range days {
		switch day {
		case "sun":
			mask |= 0x01
		case "mon":
			mask |= 0x02
		case "tue":
			mask |= 0x04
		case "wed":
			mask |= 0x08
		case "thu":
			mask |= 0x10
		case "fri":
			mask |= 0x20
		case "sat":
			mask |= 0x40
		}
	}
	return mask
}

// parseStartTime parses a time string like "06:00" into hour and minute
func parseStartTime(s string) (uint8, uint8) {
	var hour, minute int
	fmt.Sscanf(s, "%d:%d", &hour, &minute)
	return uint8(hour), uint8(minute)
}

// handleValveCommand processes immediate valve commands from the cloud
func (e *Engine) handleValveCommand(data json.RawMessage) {
	cmd, err := cloud.ParseValveCommand(data)
	if err != nil {
		log.Printf("Failed to parse valve command: %v", err)
		return
	}

	log.Printf("Valve command from cloud: valve %s addr %d -> %s",
		cmd.ValveID, cmd.ActuatorAddress, cmd.Command)

	// Convert command string to protocol command
	var protoCmd uint8
	switch cmd.Command {
	case "open":
		protoCmd = protocol.ValveCmdOpen
	case "close":
		protoCmd = protocol.ValveCmdClose
	case "stop":
		protoCmd = protocol.ValveCmdStop
	default:
		log.Printf("Unknown valve command: %s", cmd.Command)
		return
	}

	// Send command to device
	// TODO: Need to map valve_id to controller_uid - for now use valve_id as controller
	controllerUID := cmd.ValveID // This should be looked up from database
	if err := e.SendValveCommand(controllerUID, cmd.ActuatorAddress, protoCmd); err != nil {
		log.Printf("Failed to send valve command: %v", err)
	}
}

// SendValveCommand sends a valve command to a device and tracks it
func (e *Engine) SendValveCommand(controllerUID string, actuatorAddr uint8, command uint8) error {
	// Generate command ID
	cmdID := uint16(atomic.AddUint32(&e.commandID, 1))

	// Parse device UID
	uid, err := lora.ParseDeviceUID(controllerUID)
	if err != nil {
		return fmt.Errorf("invalid controller UID: %w", err)
	}

	// Create and send message
	msg := lora.CreateValveCommand(uid, actuatorAddr, command, cmdID)
	msg.Header.Sequence = e.lora.GetNextSeqNum()

	if err := e.lora.Send(msg); err != nil {
		return fmt.Errorf("failed to send command: %w", err)
	}

	// Store pending command for tracking
	pending := &storage.PendingCommand{
		CommandID:     cmdID,
		ControllerUID: controllerUID,
		ActuatorAddr:  actuatorAddr,
		Command:       command,
		ExpiresAt:     time.Now().Add(e.config.CommandTimeout),
		MaxRetries:    e.config.CommandRetries,
	}

	if _, err := e.db.InsertPendingCommand(pending); err != nil {
		log.Printf("Failed to store pending command: %v", err)
	}

	log.Printf("Sent valve command %d to %s addr %d: %s",
		cmdID, controllerUID, actuatorAddr, valveCommandString(command))

	return nil
}

// cloudSyncLoop periodically syncs data to the cloud
func (e *Engine) cloudSyncLoop(ctx context.Context) {
	defer e.wg.Done()

	ticker := time.NewTicker(e.config.SyncInterval)
	defer ticker.Stop()

	for {
		select {
		case <-e.stopChan:
			return
		case <-ctx.Done():
			return
		case <-ticker.C:
			e.syncToCloud()
		}
	}
}

// syncToCloud sends unsynced data to the cloud via gRPC
func (e *Engine) syncToCloud() {
	if !e.cloud.IsConnected() {
		return // Skip sync if not connected
	}

	// Sync soil moisture readings - batch by device
	readings, err := e.db.GetUnsyncedSoilMoistureReadings(50)
	if err != nil {
		log.Printf("Failed to get unsynced sensor readings: %v", err)
	} else {
		// Group readings by device
		byDevice := make(map[string][]*controllerv1.SensorReading)
		for _, r := range readings {
			reading := &controllerv1.SensorReading{
				Timestamp: timestamppb.New(r.Timestamp),
				Probes: []*controllerv1.ProbeReading{{
					Index:           int32(r.ProbeID),
					MoisturePercent: float32(r.MoisturePercent),
				}},
				BatteryMv:    int32(r.BatteryMV),
				TemperatureC: float32(r.Temperature) / 10.0,
				SignalRssi:   int32(r.RSSI),
			}
			byDevice[r.DeviceUID] = append(byDevice[r.DeviceUID], reading)
		}

		for deviceUID, deviceReadings := range byDevice {
			if err := e.cloud.SendSensorData(deviceUID, deviceReadings); err != nil {
				log.Printf("Failed to sync sensor readings for %s: %v", deviceUID, err)
				continue
			}
			// Mark all readings for this device as synced
			for _, r := range readings {
				if r.DeviceUID == deviceUID {
					e.db.MarkSoilMoistureReadingSynced(r.ID)
				}
			}
		}
	}

	// Sync water meter readings - batch by device
	meterReadings, err := e.db.GetUnsyncedWaterMeterReadings(50)
	if err != nil {
		log.Printf("Failed to get unsynced meter readings: %v", err)
	} else {
		byDevice := make(map[string][]*controllerv1.MeterReading)
		for _, r := range meterReadings {
			reading := &controllerv1.MeterReading{
				Timestamp:   timestamppb.New(r.Timestamp),
				TotalLiters: float64(r.TotalLiters),
				FlowRateLpm: float32(r.FlowRateLPM),
				BatteryMv:   intPtr32(int32(r.BatteryMV)),
				SignalRssi:  int32(r.RSSI),
			}
			byDevice[r.DeviceUID] = append(byDevice[r.DeviceUID], reading)
		}

		for deviceUID, deviceReadings := range byDevice {
			if err := e.cloud.SendMeterData(deviceUID, deviceReadings); err != nil {
				log.Printf("Failed to sync meter readings for %s: %v", deviceUID, err)
				continue
			}
			for _, r := range meterReadings {
				if r.DeviceUID == deviceUID {
					e.db.MarkWaterMeterReadingSynced(r.ID)
				}
			}
		}
	}

	// Sync valve events
	events, err := e.db.GetUnsyncedValveEvents(50)
	if err != nil {
		log.Printf("Failed to get unsynced valve events: %v", err)
	} else {
		// Group by controller
		byController := make(map[string][]*controllerv1.ActuatorStatus)
		for _, ev := range events {
			status := &controllerv1.ActuatorStatus{
				Address:   int32(ev.ActuatorAddr),
				State:     valveStateString(ev.NewState),
				ChangedAt: timestamppb.New(ev.Timestamp),
			}
			byController[ev.ControllerUID] = append(byController[ev.ControllerUID], status)
		}

		for controllerUID, statuses := range byController {
			if err := e.cloud.SendValveStatus(controllerUID, statuses); err != nil {
				log.Printf("Failed to sync valve events for %s: %v", controllerUID, err)
				continue
			}
			for _, ev := range events {
				if ev.ControllerUID == controllerUID {
					e.db.MarkValveEventSynced(ev.ID)
				}
			}
		}
	}
}

func intPtr32(i int32) *int32 {
	return &i
}

// commandRetryLoop retries expired commands
func (e *Engine) commandRetryLoop(ctx context.Context) {
	defer e.wg.Done()

	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-e.stopChan:
			return
		case <-ctx.Done():
			return
		case <-ticker.C:
			e.retryExpiredCommands()
		}
	}
}

// retryExpiredCommands retries commands that haven't been acknowledged
func (e *Engine) retryExpiredCommands() {
	expired, err := e.db.GetExpiredCommands()
	if err != nil {
		log.Printf("Failed to get expired commands: %v", err)
		return
	}

	for _, cmd := range expired {
		log.Printf("Retrying command %d to %s addr %d (attempt %d/%d)",
			cmd.CommandID, cmd.ControllerUID, cmd.ActuatorAddr, cmd.Retries+1, cmd.MaxRetries)

		// Parse device UID
		uid, err := lora.ParseDeviceUID(cmd.ControllerUID)
		if err != nil {
			log.Printf("Invalid controller UID: %v", err)
			continue
		}

		// Resend command
		msg := lora.CreateValveCommand(uid, cmd.ActuatorAddr, cmd.Command, cmd.CommandID)
		msg.Header.Sequence = e.lora.GetNextSeqNum()

		if err := e.lora.Send(msg); err != nil {
			log.Printf("Failed to retry command: %v", err)
			continue
		}

		// Update retry count and expiry
		newExpiry := time.Now().Add(e.config.CommandTimeout)
		if err := e.db.IncrementCommandRetry(cmd.ID, newExpiry); err != nil {
			log.Printf("Failed to update command retry: %v", err)
		}
	}
}

// timeSyncLoop periodically broadcasts time sync messages
func (e *Engine) timeSyncLoop(ctx context.Context) {
	defer e.wg.Done()

	// Send initial time sync
	e.broadcastTimeSync()

	ticker := time.NewTicker(e.config.TimeSyncInterval)
	defer ticker.Stop()

	for {
		select {
		case <-e.stopChan:
			return
		case <-ctx.Done():
			return
		case <-ticker.C:
			e.broadcastTimeSync()
		}
	}
}

// broadcastTimeSync sends a time sync message to all devices
func (e *Engine) broadcastTimeSync() {
	msg := lora.CreateTimeSyncMessage(0) // UTC offset 0 for now
	msg.Header.Sequence = e.lora.GetNextSeqNum()

	if err := e.lora.Send(msg); err != nil {
		log.Printf("Failed to broadcast time sync: %v", err)
	} else {
		log.Println("Broadcast time sync")
	}
}

// queueForCloudSync queues data for cloud synchronization
func (e *Engine) queueForCloudSync(dataType string, dataID int64, data interface{}) {
	// If connected, try to send immediately
	if e.cloud.IsConnected() {
		// Data will be synced in the next sync cycle
		return
	}

	// Otherwise, data is already in the database with synced_to_cloud = false
	// It will be synced when connection is restored
}

// gRPC message handlers

// handleValveCommandGRPC processes valve commands from the cloud via gRPC
func (e *Engine) handleValveCommandGRPC(cmd *controllerv1.ValveCommand) {
	log.Printf("Valve command from cloud: valve %s addr %d -> %s",
		cmd.ValveId, cmd.ActuatorAddress, cmd.Command.String())

	// Convert command to protocol command
	var protoCmd uint8
	switch cmd.Command {
	case controllerv1.Command_COMMAND_OPEN:
		protoCmd = protocol.ValveCmdOpen
	case controllerv1.Command_COMMAND_CLOSE:
		protoCmd = protocol.ValveCmdClose
	case controllerv1.Command_COMMAND_STOP:
		protoCmd = protocol.ValveCmdStop
	default:
		log.Printf("Unknown valve command: %s", cmd.Command.String())
		return
	}

	// Send command to device
	controllerUID := cmd.ControllerUid
	if err := e.SendValveCommand(controllerUID, uint8(cmd.ActuatorAddress), protoCmd); err != nil {
		log.Printf("Failed to send valve command: %v", err)
	}
}

// handleScheduleUpdateGRPC processes schedule updates from the cloud via gRPC
func (e *Engine) handleScheduleUpdateGRPC(update *controllerv1.ScheduleUpdate) {
	log.Printf("Schedule update for property %s with %d schedules", update.PropertyId, len(update.Schedules))

	for _, sched := range update.Schedules {
		// Convert days to day mask
		dayMask := daysToDayMask(sched.Days)
		startHour, startMinute := parseStartTime(sched.StartTime)

		// Convert to storage format
		schedule := &storage.Schedule{
			UID:      sched.ScheduleId,
			Name:     sched.Name,
			IsActive: sched.Enabled,
		}

		// Create a single entry for this schedule
		var actuatorMask uint64
		for _, v := range sched.Valves {
			actuatorMask |= (1 << v.ActuatorAddress)
		}

		entries := []storage.ScheduleEntry{{
			DayMask:      dayMask,
			StartHour:    startHour,
			StartMinute:  startMinute,
			DurationMins: uint16(sched.DurationMinutes),
			ActuatorMask: actuatorMask,
		}}

		// Store in database
		if err := e.db.UpsertSchedule(schedule, entries); err != nil {
			log.Printf("Failed to store schedule: %v", err)
			continue
		}

		log.Printf("Updated schedule %s: %s", sched.ScheduleId, sched.Name)
	}
}

// handleDeviceAddedGRPC processes device approval notifications from the cloud via gRPC
func (e *Engine) handleDeviceAddedGRPC(approved *controllerv1.DeviceApproved) {
	e.mu.Lock()
	defer e.mu.Unlock()

	// Add the new device
	device := &storage.Device{
		UID:          approved.DeviceUid,
		DeviceType:   deviceTypeFromString(approved.DeviceType),
		Name:         approved.Name,
		ZoneID:       approved.GetZoneId(),
		IsRegistered: true,
		FirstSeen:    time.Now(),
		LastSeen:     time.Now(),
	}
	e.registeredDevices[approved.DeviceUid] = device

	// Store in database
	if err := e.db.UpsertDevice(device); err != nil {
		log.Printf("Failed to store device %s: %v", approved.DeviceUid, err)
	}

	log.Printf("Device approved: %s (%s) - %s", approved.DeviceUid, approved.DeviceType, approved.Name)
}

// handleConfigUpdateGRPC processes config updates from the cloud via gRPC
func (e *Engine) handleConfigUpdateGRPC(update *controllerv1.ConfigUpdate) {
	log.Printf("Config update received for target: %s", update.Target)
	// TODO: Apply configuration changes
	for key, value := range update.Config {
		log.Printf("  %s = %s", key, value)
	}
}

// Helper functions

func valveStateString(state uint8) string {
	switch state {
	case protocol.ValveStateClosed:
		return "CLOSED"
	case protocol.ValveStateOpen:
		return "OPEN"
	case protocol.ValveStateOpening:
		return "OPENING"
	case protocol.ValveStateClosing:
		return "CLOSING"
	case protocol.ValveStateError:
		return "ERROR"
	default:
		return fmt.Sprintf("UNKNOWN(%d)", state)
	}
}

func valveCommandString(cmd uint8) string {
	switch cmd {
	case protocol.ValveCmdClose:
		return "close"
	case protocol.ValveCmdOpen:
		return "open"
	case protocol.ValveCmdStop:
		return "STOP"
	case protocol.ValveCmdQuery:
		return "QUERY"
	default:
		return fmt.Sprintf("UNKNOWN(%d)", cmd)
	}
}
