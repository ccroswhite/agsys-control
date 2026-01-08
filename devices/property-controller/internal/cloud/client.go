// Package cloud provides communication with the AgSys cloud service.
// Uses HTTPS REST for data submission and WebSocket for real-time events.
package cloud

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"math/rand"
	"net/http"
	"sync"
	"time"

	"github.com/google/uuid"
	"github.com/gorilla/websocket"
)

// MessageType defines the type of WebSocket message
type MessageType string

const (
	// Outbound WebSocket messages (to cloud)
	MsgTypeAck  MessageType = "ack"
	MsgTypePong MessageType = "pong"

	// Inbound WebSocket messages (from cloud)
	MsgTypeValveCommand   MessageType = "valve_command"
	MsgTypeScheduleUpdate MessageType = "schedule_update"
	MsgTypeDeviceAdded    MessageType = "device_added"
	MsgTypeConfigUpdate   MessageType = "config_update"
	MsgTypePing           MessageType = "ping"
)

// Message represents a WebSocket message to/from the cloud
type Message struct {
	Type      MessageType     `json:"type"`
	ID        string          `json:"id,omitempty"`
	Timestamp string          `json:"timestamp"`
	Payload   json.RawMessage `json:"payload"`
}

// Config holds cloud client configuration
type Config struct {
	BaseURL      string // REST API base URL (https://api.agsys.io/api/v1/device)
	WebSocketURL string // WebSocket URL (wss://api.agsys.io/ws/device)
	ControllerID string // Controller UUID
	APIKey       string // API key for authentication

	PingInterval time.Duration // Interval for ping/keepalive
	WriteTimeout time.Duration // Timeout for write operations
	ReadTimeout  time.Duration // Timeout for read operations
	HTTPTimeout  time.Duration // Timeout for HTTP requests

	// Reconnection settings (exponential backoff)
	InitialRetryDelay time.Duration
	MaxRetryDelay     time.Duration
	BackoffMultiplier float64
	JitterPercent     float64
}

// DefaultConfig returns default cloud client configuration
func DefaultConfig() Config {
	return Config{
		PingInterval:      30 * time.Second,
		WriteTimeout:      10 * time.Second,
		ReadTimeout:       60 * time.Second,
		HTTPTimeout:       30 * time.Second,
		InitialRetryDelay: 1 * time.Second,
		MaxRetryDelay:     60 * time.Second,
		BackoffMultiplier: 2.0,
		JitterPercent:     0.25,
	}
}

// Client handles communication with the AgSys cloud
type Client struct {
	config     Config
	httpClient *http.Client
	conn       *websocket.Conn
	sendChan   chan *Message
	stopChan   chan struct{}
	wg         sync.WaitGroup
	mu         sync.Mutex
	connected  bool

	// Current retry delay for exponential backoff
	currentRetryDelay time.Duration

	// Callbacks for WebSocket messages from cloud
	onValveCommand func(json.RawMessage)
	onSchedule     func(json.RawMessage)
	onDeviceAdded  func(json.RawMessage)
	onConfigUpdate func(json.RawMessage)
}

// New creates a new cloud client
func New(config Config) *Client {
	return &Client{
		config: config,
		httpClient: &http.Client{
			Timeout: config.HTTPTimeout,
		},
		sendChan:          make(chan *Message, 100),
		stopChan:          make(chan struct{}),
		currentRetryDelay: config.InitialRetryDelay,
	}
}

// SetValveCommandCallback sets the callback for valve command messages
func (c *Client) SetValveCommandCallback(cb func(json.RawMessage)) {
	c.mu.Lock()
	c.onValveCommand = cb
	c.mu.Unlock()
}

// SetScheduleCallback sets the callback for schedule update messages
func (c *Client) SetScheduleCallback(cb func(json.RawMessage)) {
	c.mu.Lock()
	c.onSchedule = cb
	c.mu.Unlock()
}

// SetDeviceAddedCallback sets the callback for device added messages
func (c *Client) SetDeviceAddedCallback(cb func(json.RawMessage)) {
	c.mu.Lock()
	c.onDeviceAdded = cb
	c.mu.Unlock()
}

// SetConfigUpdateCallback sets the callback for config update messages
func (c *Client) SetConfigUpdateCallback(cb func(json.RawMessage)) {
	c.mu.Lock()
	c.onConfigUpdate = cb
	c.mu.Unlock()
}

// Start connects to the cloud and starts the WebSocket message loops
func (c *Client) Start(ctx context.Context) error {
	c.wg.Add(1)
	go c.connectionLoop(ctx)
	return nil
}

// Stop disconnects from the cloud and stops all loops
func (c *Client) Stop() error {
	close(c.stopChan)
	c.wg.Wait()
	return nil
}

// IsConnected returns whether the WebSocket is connected
func (c *Client) IsConnected() bool {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.connected
}

// =============================================================================
// REST API Methods (Controller → Backend)
// =============================================================================

// SensorReading represents a single sensor reading
type SensorReading struct {
	Timestamp    time.Time   `json:"timestamp"`
	Probes       []ProbeData `json:"probes"`
	BatteryMV    int         `json:"battery_mv"`
	TemperatureC float64     `json:"temperature_c,omitempty"`
	SignalRSSI   int         `json:"signal_rssi,omitempty"`
}

// ProbeData represents data from a single probe
type ProbeData struct {
	Index           int     `json:"index"`
	FrequencyHz     int     `json:"frequency_hz,omitempty"`
	MoisturePercent float64 `json:"moisture_percent"`
}

// IngestSensorData sends soil moisture sensor readings via REST API
func (c *Client) IngestSensorData(ctx context.Context, deviceUID string, readings []SensorReading) error {
	payload := map[string]interface{}{
		"device_uid": deviceUID,
		"readings":   readings,
	}
	return c.postJSON(ctx, "/sensors/ingest", payload)
}

// MeterReading represents a single water meter reading
type MeterReading struct {
	Timestamp   time.Time   `json:"timestamp"`
	TotalLiters float64     `json:"total_liters"`
	FlowRateLPM float64     `json:"flow_rate_lpm"`
	VelocityMPS float64     `json:"velocity_mps,omitempty"`
	Direction   string      `json:"direction,omitempty"`
	BatteryMV   int         `json:"battery_mv,omitempty"`
	SignalRSSI  int         `json:"signal_rssi,omitempty"`
	Flags       *MeterFlags `json:"flags,omitempty"`
}

// MeterFlags represents water meter status flags
type MeterFlags struct {
	LowBattery  bool `json:"low_battery"`
	ReverseFlow bool `json:"reverse_flow"`
	CoilFault   bool `json:"coil_fault"`
}

// IngestMeterData sends water meter readings via REST API
func (c *Client) IngestMeterData(ctx context.Context, deviceUID string, readings []MeterReading) error {
	payload := map[string]interface{}{
		"device_uid": deviceUID,
		"readings":   readings,
	}
	return c.postJSON(ctx, "/meters/ingest", payload)
}

// ActuatorStatus represents the status of a valve actuator
type ActuatorStatus struct {
	Address   uint8          `json:"address"`
	State     string         `json:"state"`
	CurrentMA int            `json:"current_ma,omitempty"`
	ChangedAt time.Time      `json:"changed_at"`
	CommandID string         `json:"command_id,omitempty"`
	Flags     *ActuatorFlags `json:"flags,omitempty"`
}

// ActuatorFlags represents valve actuator status flags
type ActuatorFlags struct {
	PowerFail   bool `json:"power_fail"`
	Overcurrent bool `json:"overcurrent"`
	OnBattery   bool `json:"on_battery"`
}

// ReportValveStatus sends valve status updates via REST API
func (c *Client) ReportValveStatus(ctx context.Context, controllerUID string, actuators []ActuatorStatus) error {
	payload := map[string]interface{}{
		"controller_uid": controllerUID,
		"actuators":      actuators,
	}
	return c.postJSON(ctx, "/valves/status", payload)
}

// AcknowledgeValveCommand acknowledges a valve command via REST API
func (c *Client) AcknowledgeValveCommand(ctx context.Context, valveID, commandID string, success bool, currentState string, errorMsg *string) error {
	payload := map[string]interface{}{
		"command_id":    commandID,
		"success":       success,
		"current_state": currentState,
		"executed_at":   time.Now().UTC().Format(time.RFC3339),
	}
	if errorMsg != nil {
		payload["error_message"] = *errorMsg
	}
	return c.postJSON(ctx, fmt.Sprintf("/valves/%s/acknowledge", valveID), payload)
}

// RegisterDevice registers a new device discovered by the controller
func (c *Client) RegisterDevice(ctx context.Context, deviceUID, deviceType, firmwareVersion string, firstSeen time.Time, signalRSSI int) error {
	payload := map[string]interface{}{
		"device_uid":       deviceUID,
		"device_type":      deviceType,
		"firmware_version": firmwareVersion,
		"first_seen":       firstSeen.UTC().Format(time.RFC3339),
		"signal_rssi":      signalRSSI,
	}
	return c.postJSON(ctx, "/devices/register", payload)
}

// LoRaStats represents LoRa radio statistics
type LoRaStats struct {
	PacketsReceived int `json:"packets_received"`
	PacketsSent     int `json:"packets_sent"`
	Errors          int `json:"errors"`
}

// SendHeartbeat sends a heartbeat to the backend via REST API
func (c *Client) SendHeartbeat(ctx context.Context, uptimeSeconds int64, firmwareVersion string, loraStats *LoRaStats, connectedDevices int) error {
	payload := map[string]interface{}{
		"timestamp":         time.Now().UTC().Format(time.RFC3339),
		"uptime_seconds":    uptimeSeconds,
		"firmware_version":  firmwareVersion,
		"connected_devices": connectedDevices,
	}
	if loraStats != nil {
		payload["lora_stats"] = loraStats
	}
	return c.postJSON(ctx, "/controllers/heartbeat", payload)
}

// postJSON sends a POST request with JSON body to the REST API
func (c *Client) postJSON(ctx context.Context, endpoint string, payload interface{}) error {
	data, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("marshal payload: %w", err)
	}

	url := c.config.BaseURL + endpoint
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, url, bytes.NewReader(data))
	if err != nil {
		return fmt.Errorf("create request: %w", err)
	}

	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-API-Key", c.config.APIKey)
	req.Header.Set("X-Controller-ID", c.config.ControllerID)

	resp, err := c.httpClient.Do(req)
	if err != nil {
		return fmt.Errorf("send request: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode >= 400 {
		body, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("API error %d: %s", resp.StatusCode, string(body))
	}

	return nil
}

// =============================================================================
// WebSocket Methods (Backend → Controller)
// =============================================================================

// connectionLoop manages the WebSocket connection with exponential backoff
func (c *Client) connectionLoop(ctx context.Context) {
	defer c.wg.Done()

	for {
		select {
		case <-c.stopChan:
			c.disconnect()
			return
		case <-ctx.Done():
			c.disconnect()
			return
		default:
		}

		// Attempt to connect
		if err := c.connect(); err != nil {
			log.Printf("Failed to connect to cloud: %v", err)
			c.waitWithBackoff()
			continue
		}

		// Reset retry delay on successful connection
		c.currentRetryDelay = c.config.InitialRetryDelay

		// Run read/write loops until disconnected
		c.runMessageLoops(ctx)

		// Disconnected, wait before reconnecting
		log.Println("Disconnected from cloud, reconnecting...")
		c.waitWithBackoff()
	}
}

// waitWithBackoff waits for the current retry delay with jitter
func (c *Client) waitWithBackoff() {
	// Add jitter
	jitter := c.currentRetryDelay.Seconds() * c.config.JitterPercent * (rand.Float64()*2 - 1)
	delay := c.currentRetryDelay + time.Duration(jitter*float64(time.Second))

	time.Sleep(delay)

	// Increase delay for next time (exponential backoff)
	c.currentRetryDelay = time.Duration(float64(c.currentRetryDelay) * c.config.BackoffMultiplier)
	if c.currentRetryDelay > c.config.MaxRetryDelay {
		c.currentRetryDelay = c.config.MaxRetryDelay
	}
}

// connect establishes the WebSocket connection
func (c *Client) connect() error {
	// Build WebSocket URL with query parameters
	wsURL := fmt.Sprintf("%s?api_key=%s&controller_id=%s",
		c.config.WebSocketURL, c.config.APIKey, c.config.ControllerID)

	dialer := websocket.Dialer{
		HandshakeTimeout: 10 * time.Second,
	}

	conn, _, err := dialer.Dial(wsURL, nil)
	if err != nil {
		return fmt.Errorf("dial failed: %w", err)
	}

	c.mu.Lock()
	c.conn = conn
	c.connected = true
	c.mu.Unlock()

	log.Printf("Connected to cloud WebSocket: %s", c.config.WebSocketURL)
	return nil
}

// disconnect closes the WebSocket connection
func (c *Client) disconnect() {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.conn != nil {
		c.conn.Close()
		c.conn = nil
	}
	c.connected = false
}

// runMessageLoops runs the read and write loops
func (c *Client) runMessageLoops(ctx context.Context) {
	var wg sync.WaitGroup
	done := make(chan struct{})

	// Read loop
	wg.Add(1)
	go func() {
		defer wg.Done()
		c.readLoop(done)
	}()

	// Write loop
	wg.Add(1)
	go func() {
		defer wg.Done()
		c.writeLoop(ctx, done)
	}()

	// Wait for any loop to exit
	wg.Wait()
}

// readLoop reads messages from the WebSocket
func (c *Client) readLoop(done chan struct{}) {
	defer close(done)

	for {
		c.mu.Lock()
		conn := c.conn
		c.mu.Unlock()

		if conn == nil {
			return
		}

		conn.SetReadDeadline(time.Now().Add(c.config.ReadTimeout))

		_, data, err := conn.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure) {
				log.Printf("WebSocket read error: %v", err)
			}
			return
		}

		var msg Message
		if err := json.Unmarshal(data, &msg); err != nil {
			log.Printf("Failed to parse message: %v", err)
			continue
		}

		// Handle message based on type
		c.handleMessage(&msg)
	}
}

// writeLoop sends messages to the WebSocket
func (c *Client) writeLoop(ctx context.Context, done chan struct{}) {
	ticker := time.NewTicker(c.config.PingInterval)
	defer ticker.Stop()

	for {
		select {
		case <-done:
			return
		case <-ctx.Done():
			return
		case <-c.stopChan:
			return

		case msg := <-c.sendChan:
			c.mu.Lock()
			conn := c.conn
			c.mu.Unlock()

			if conn == nil {
				continue
			}

			data, err := json.Marshal(msg)
			if err != nil {
				log.Printf("Failed to marshal message: %v", err)
				continue
			}

			conn.SetWriteDeadline(time.Now().Add(c.config.WriteTimeout))
			if err := conn.WriteMessage(websocket.TextMessage, data); err != nil {
				log.Printf("WebSocket write error: %v", err)
				return
			}

		case <-ticker.C:
			// Send WebSocket ping frame
			c.mu.Lock()
			conn := c.conn
			c.mu.Unlock()

			if conn == nil {
				return
			}

			conn.SetWriteDeadline(time.Now().Add(c.config.WriteTimeout))
			if err := conn.WriteMessage(websocket.PingMessage, nil); err != nil {
				log.Printf("Ping failed: %v", err)
				return
			}
		}
	}
}

// handleMessage processes an incoming WebSocket message
func (c *Client) handleMessage(msg *Message) {
	c.mu.Lock()
	onValveCommand := c.onValveCommand
	onSchedule := c.onSchedule
	onDeviceAdded := c.onDeviceAdded
	onConfigUpdate := c.onConfigUpdate
	c.mu.Unlock()

	switch msg.Type {
	case MsgTypeValveCommand:
		if onValveCommand != nil {
			onValveCommand(msg.Payload)
		}
		// Send acknowledgment
		c.sendAck(msg.ID, true, nil)

	case MsgTypeScheduleUpdate:
		if onSchedule != nil {
			onSchedule(msg.Payload)
		}
		c.sendAck(msg.ID, true, nil)

	case MsgTypeDeviceAdded:
		if onDeviceAdded != nil {
			onDeviceAdded(msg.Payload)
		}
		c.sendAck(msg.ID, true, nil)

	case MsgTypeConfigUpdate:
		if onConfigUpdate != nil {
			onConfigUpdate(msg.Payload)
		}
		c.sendAck(msg.ID, true, nil)

	case MsgTypePing:
		c.sendPong(msg.ID)

	default:
		log.Printf("Unknown message type: %s", msg.Type)
	}
}

// sendAck sends an acknowledgment message
func (c *Client) sendAck(messageID string, success bool, errMsg *string) {
	payload := map[string]interface{}{
		"message_id": messageID,
		"success":    success,
	}
	if errMsg != nil {
		payload["error"] = *errMsg
	}

	payloadBytes, _ := json.Marshal(payload)

	msg := &Message{
		Type:      MsgTypeAck,
		ID:        uuid.New().String(),
		Timestamp: time.Now().UTC().Format(time.RFC3339),
		Payload:   payloadBytes,
	}

	select {
	case c.sendChan <- msg:
	default:
		log.Printf("Send queue full, dropping ack")
	}
}

// sendPong sends a pong response to a ping
func (c *Client) sendPong(pingID string) {
	payload := map[string]interface{}{
		"ping_id": pingID,
	}

	payloadBytes, _ := json.Marshal(payload)

	msg := &Message{
		Type:      MsgTypePong,
		ID:        uuid.New().String(),
		Timestamp: time.Now().UTC().Format(time.RFC3339),
		Payload:   payloadBytes,
	}

	select {
	case c.sendChan <- msg:
	default:
		log.Printf("Send queue full, dropping pong")
	}
}

// =============================================================================
// Payload Types for Inbound Messages
// =============================================================================

// ValveCommandPayload represents a valve command from the cloud
type ValveCommandPayload struct {
	ValveID         string `json:"valve_id"`
	ActuatorAddress uint8  `json:"actuator_address"`
	Command         string `json:"command"` // "open", "close", "stop"
	CommandID       string `json:"command_id"`
	DurationSeconds *int   `json:"duration_seconds"`
	Priority        string `json:"priority"` // "normal", "emergency"
	Source          string `json:"source"`
	SourceID        string `json:"source_id"`
}

// ParseValveCommand parses a valve command payload
func ParseValveCommand(data json.RawMessage) (*ValveCommandPayload, error) {
	var cmd ValveCommandPayload
	if err := json.Unmarshal(data, &cmd); err != nil {
		return nil, err
	}
	return &cmd, nil
}

// ScheduleUpdatePayload represents a schedule update from the cloud
type ScheduleUpdatePayload struct {
	PropertyID string     `json:"property_id"`
	Schedules  []Schedule `json:"schedules"`
}

// Schedule represents a single irrigation schedule
type Schedule struct {
	ScheduleID      string          `json:"schedule_id"`
	ZoneID          string          `json:"zone_id"`
	Name            string          `json:"name"`
	Enabled         bool            `json:"enabled"`
	Days            []string        `json:"days"`
	StartTime       string          `json:"start_time"`
	DurationMinutes int             `json:"duration_minutes"`
	Valves          []ScheduleValve `json:"valves"`
}

// ScheduleValve represents a valve in a schedule
type ScheduleValve struct {
	ValveID         string `json:"valve_id"`
	ActuatorAddress uint8  `json:"actuator_address"`
}

// ParseScheduleUpdate parses a schedule update payload
func ParseScheduleUpdate(data json.RawMessage) (*ScheduleUpdatePayload, error) {
	var schedule ScheduleUpdatePayload
	if err := json.Unmarshal(data, &schedule); err != nil {
		return nil, err
	}
	return &schedule, nil
}

// DeviceAddedPayload represents a device added notification from the cloud
type DeviceAddedPayload struct {
	DeviceID   string                 `json:"device_id"`
	DeviceUID  string                 `json:"device_uid"`
	DeviceType string                 `json:"device_type"`
	Name       string                 `json:"name"`
	ZoneID     string                 `json:"zone_id,omitempty"`
	Config     map[string]interface{} `json:"config,omitempty"`
}

// ParseDeviceAdded parses a device added payload
func ParseDeviceAdded(data json.RawMessage) (*DeviceAddedPayload, error) {
	var device DeviceAddedPayload
	if err := json.Unmarshal(data, &device); err != nil {
		return nil, err
	}
	return &device, nil
}

// ConfigUpdatePayload represents a config update from the cloud
type ConfigUpdatePayload struct {
	Target string                 `json:"target"` // "controller" or device UID
	Config map[string]interface{} `json:"config"`
}

// ParseConfigUpdate parses a config update payload
func ParseConfigUpdate(data json.RawMessage) (*ConfigUpdatePayload, error) {
	var cfg ConfigUpdatePayload
	if err := json.Unmarshal(data, &cfg); err != nil {
		return nil, err
	}
	return &cfg, nil
}
