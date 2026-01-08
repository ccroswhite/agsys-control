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
	"github.com/agsys/property-controller/internal/protocol"
	"github.com/agsys/property-controller/internal/storage"
)

// Config holds engine configuration
type Config struct {
	DatabasePath     string
	CloudURL         string
	PropertyUID      string
	APIKey           string
	AESKey           []byte
	LoRaFrequency    uint32
	CommandTimeout   time.Duration
	CommandRetries   int
	SyncInterval     time.Duration
	TimeSyncInterval time.Duration
}

// DefaultConfig returns default engine configuration
func DefaultConfig() Config {
	return Config{
		DatabasePath:     "/var/lib/agsys/controller.db",
		LoRaFrequency:    915000000,
		CommandTimeout:   10 * time.Second,
		CommandRetries:   3,
		SyncInterval:     30 * time.Second,
		TimeSyncInterval: 1 * time.Hour,
	}
}

// Engine is the core controller that routes messages between devices and cloud
type Engine struct {
	config    Config
	db        *storage.DB
	lora      *lora.Driver
	cloud     *cloud.Client
	stopChan  chan struct{}
	wg        sync.WaitGroup
	mu        sync.RWMutex
	commandID uint32

	// Registered devices (from cloud)
	registeredDevices map[string]*storage.Device
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

	// Create cloud client
	cloudConfig := cloud.DefaultConfig()
	cloudConfig.URL = config.CloudURL
	cloudConfig.PropertyUID = config.PropertyUID
	cloudConfig.APIKey = config.APIKey

	cloudClient := cloud.New(cloudConfig)

	return &Engine{
		config:            config,
		db:                db,
		lora:              loraDriver,
		cloud:             cloudClient,
		stopChan:          make(chan struct{}),
		registeredDevices: make(map[string]*storage.Device),
	}, nil
}

// Start starts the engine
func (e *Engine) Start(ctx context.Context) error {
	// Set up callbacks
	e.lora.SetReceiveCallback(e.handleLoRaMessage)
	e.cloud.SetDeviceListCallback(e.handleDeviceList)
	e.cloud.SetScheduleCallback(e.handleScheduleUpdate)
	e.cloud.SetValveCommandCallback(e.handleValveCommand)

	// Start LoRa driver
	if err := e.lora.Start(); err != nil {
		return fmt.Errorf("failed to start LoRa driver: %w", err)
	}

	// Start cloud client
	if err := e.cloud.Start(ctx); err != nil {
		e.lora.Stop()
		return fmt.Errorf("failed to start cloud client: %w", err)
	}

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

	if err := e.cloud.Stop(); err != nil {
		log.Printf("Error stopping cloud client: %v", err)
	}

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

	case protocol.MsgTypeValveStatus:
		e.handleValveStatus(deviceUID, msg)

	case protocol.MsgTypeValveAck:
		e.handleValveAck(deviceUID, msg)

	case protocol.MsgTypeScheduleRequest:
		e.handleScheduleRequest(deviceUID, msg)

	case protocol.MsgTypeHeartbeat:
		log.Printf("Heartbeat from %s, RSSI: %d", deviceUID, msg.RSSI)

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

	// Send acknowledgment to cloud
	e.cloud.SendValveAck(deviceUID, ack.ActuatorAddr, ack.CommandID, ack.ResultState, ack.Success)
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

// handleDeviceList processes device list updates from the cloud
func (e *Engine) handleDeviceList(data json.RawMessage) {
	list, err := cloud.ParseDeviceList(data)
	if err != nil {
		log.Printf("Failed to parse device list: %v", err)
		return
	}

	e.mu.Lock()
	defer e.mu.Unlock()

	// Clear existing registered devices
	e.registeredDevices = make(map[string]*storage.Device)

	// Add new devices
	for _, d := range list.Devices {
		device := &storage.Device{
			UID:          d.UID,
			DeviceType:   d.DeviceType,
			Name:         d.Name,
			Alias:        d.Alias,
			ZoneID:       d.ZoneUID,
			IsRegistered: true,
			FirstSeen:    time.Now(),
			LastSeen:     time.Now(),
		}
		e.registeredDevices[d.UID] = device

		// Store in database
		if err := e.db.UpsertDevice(device); err != nil {
			log.Printf("Failed to store device %s: %v", d.UID, err)
		}
	}

	log.Printf("Updated device list: %d devices registered", len(list.Devices))
}

// handleScheduleUpdate processes schedule updates from the cloud
func (e *Engine) handleScheduleUpdate(data json.RawMessage) {
	update, err := cloud.ParseScheduleUpdate(data)
	if err != nil {
		log.Printf("Failed to parse schedule update: %v", err)
		return
	}

	// Convert to storage format
	schedule := &storage.Schedule{
		UID:           update.ScheduleUID,
		ControllerUID: update.ControllerUID,
		Version:       update.Version,
		Name:          update.Name,
		IsActive:      update.IsActive,
	}

	entries := make([]storage.ScheduleEntry, len(update.Entries))
	for i, e := range update.Entries {
		entries[i] = storage.ScheduleEntry{
			DayMask:      e.DayMask,
			StartHour:    e.StartHour,
			StartMinute:  e.StartMinute,
			DurationMins: e.DurationMins,
			ActuatorMask: e.ActuatorMask,
		}
	}

	// Store in database
	if err := e.db.UpsertSchedule(schedule, entries); err != nil {
		log.Printf("Failed to store schedule: %v", err)
		return
	}

	log.Printf("Updated schedule %s v%d for controller %s", update.ScheduleUID, update.Version, update.ControllerUID)
}

// handleValveCommand processes immediate valve commands from the cloud
func (e *Engine) handleValveCommand(data json.RawMessage) {
	cmd, err := cloud.ParseValveCommand(data)
	if err != nil {
		log.Printf("Failed to parse valve command: %v", err)
		return
	}

	log.Printf("Valve command from cloud: %s addr %d -> %s",
		cmd.ControllerUID, cmd.ActuatorAddr, cmd.Command)

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
	if err := e.SendValveCommand(cmd.ControllerUID, cmd.ActuatorAddr, protoCmd); err != nil {
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

// syncToCloud sends unsynced data to the cloud
func (e *Engine) syncToCloud() {
	if !e.cloud.IsConnected() {
		return
	}

	// Sync soil moisture readings
	readings, err := e.db.GetUnsyncedSoilMoistureReadings(50)
	if err != nil {
		log.Printf("Failed to get unsynced sensor readings: %v", err)
	} else {
		for _, r := range readings {
			if err := e.cloud.SendSensorData(r.DeviceUID, r.ProbeID, r.MoisturePercent,
				r.Temperature, r.BatteryMV, r.Timestamp); err != nil {
				log.Printf("Failed to sync sensor reading: %v", err)
				continue
			}
			e.db.MarkSoilMoistureReadingSynced(r.ID)
		}
	}

	// Sync water meter readings
	meterReadings, err := e.db.GetUnsyncedWaterMeterReadings(50)
	if err != nil {
		log.Printf("Failed to get unsynced meter readings: %v", err)
	} else {
		for _, r := range meterReadings {
			if err := e.cloud.SendWaterMeterData(r.DeviceUID, r.TotalLiters,
				r.FlowRateLPM, r.BatteryMV, r.Timestamp); err != nil {
				log.Printf("Failed to sync meter reading: %v", err)
				continue
			}
			e.db.MarkWaterMeterReadingSynced(r.ID)
		}
	}

	// Sync valve events
	events, err := e.db.GetUnsyncedValveEvents(50)
	if err != nil {
		log.Printf("Failed to get unsynced valve events: %v", err)
	} else {
		for _, ev := range events {
			if err := e.cloud.SendValveEvent(ev.ControllerUID, ev.ActuatorAddr,
				ev.PrevState, ev.NewState, ev.Source, ev.Timestamp); err != nil {
				log.Printf("Failed to sync valve event: %v", err)
				continue
			}
			e.db.MarkValveEventSynced(ev.ID)
		}
	}
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
		return "CLOSE"
	case protocol.ValveCmdOpen:
		return "OPEN"
	case protocol.ValveCmdStop:
		return "STOP"
	case protocol.ValveCmdQuery:
		return "QUERY"
	default:
		return fmt.Sprintf("UNKNOWN(%d)", cmd)
	}
}
