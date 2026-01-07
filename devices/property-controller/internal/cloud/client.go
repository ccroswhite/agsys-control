// Package cloud provides WebSocket communication with the AgSys cloud service.
package cloud

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

// MessageType defines the type of cloud message
type MessageType string

const (
	// Outbound messages (to cloud)
	MsgTypeSensorData   MessageType = "sensor_data"
	MsgTypeWaterMeter   MessageType = "water_meter"
	MsgTypeValveEvent   MessageType = "valve_event"
	MsgTypeValveAck     MessageType = "valve_ack"
	MsgTypeHeartbeat    MessageType = "heartbeat"
	MsgTypeDeviceStatus MessageType = "device_status"

	// Inbound messages (from cloud)
	MsgTypeConfig         MessageType = "config"
	MsgTypeDeviceList     MessageType = "device_list"
	MsgTypeScheduleUpdate MessageType = "schedule_update"
	MsgTypeValveCommand   MessageType = "valve_command"
	MsgTypeTimeSync       MessageType = "time_sync"
)

// Message represents a WebSocket message to/from the cloud
type Message struct {
	Type      MessageType     `json:"type"`
	ID        string          `json:"id,omitempty"` // Message ID for tracking
	Timestamp int64           `json:"timestamp"`
	Payload   json.RawMessage `json:"payload"`
}

// Config holds cloud client configuration
type Config struct {
	URL            string        // WebSocket URL (wss://...)
	PropertyUID    string        // Property UID for authentication
	APIKey         string        // API key for authentication
	ReconnectDelay time.Duration // Delay between reconnection attempts
	PingInterval   time.Duration // Interval for ping/keepalive
	WriteTimeout   time.Duration // Timeout for write operations
	ReadTimeout    time.Duration // Timeout for read operations
}

// DefaultConfig returns default cloud client configuration
func DefaultConfig() Config {
	return Config{
		ReconnectDelay: 5 * time.Second,
		PingInterval:   30 * time.Second,
		WriteTimeout:   10 * time.Second,
		ReadTimeout:    60 * time.Second,
	}
}

// Client handles WebSocket communication with the AgSys cloud
type Client struct {
	config    Config
	conn      *websocket.Conn
	sendChan  chan *Message
	recvChan  chan *Message
	stopChan  chan struct{}
	wg        sync.WaitGroup
	mu        sync.Mutex
	connected bool

	// Callbacks for different message types
	onConfig       func(json.RawMessage)
	onDeviceList   func(json.RawMessage)
	onSchedule     func(json.RawMessage)
	onValveCommand func(json.RawMessage)
}

// New creates a new cloud client
func New(config Config) *Client {
	return &Client{
		config:   config,
		sendChan: make(chan *Message, 100),
		recvChan: make(chan *Message, 100),
		stopChan: make(chan struct{}),
	}
}

// SetConfigCallback sets the callback for config messages
func (c *Client) SetConfigCallback(cb func(json.RawMessage)) {
	c.mu.Lock()
	c.onConfig = cb
	c.mu.Unlock()
}

// SetDeviceListCallback sets the callback for device list messages
func (c *Client) SetDeviceListCallback(cb func(json.RawMessage)) {
	c.mu.Lock()
	c.onDeviceList = cb
	c.mu.Unlock()
}

// SetScheduleCallback sets the callback for schedule update messages
func (c *Client) SetScheduleCallback(cb func(json.RawMessage)) {
	c.mu.Lock()
	c.onSchedule = cb
	c.mu.Unlock()
}

// SetValveCommandCallback sets the callback for valve command messages
func (c *Client) SetValveCommandCallback(cb func(json.RawMessage)) {
	c.mu.Lock()
	c.onValveCommand = cb
	c.mu.Unlock()
}

// Start connects to the cloud and starts the message loops
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

// IsConnected returns whether the client is connected
func (c *Client) IsConnected() bool {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.connected
}

// Send queues a message for sending to the cloud
func (c *Client) Send(msg *Message) error {
	if msg.Timestamp == 0 {
		msg.Timestamp = time.Now().Unix()
	}

	select {
	case c.sendChan <- msg:
		return nil
	default:
		return fmt.Errorf("send queue full")
	}
}

// SendSensorData sends soil moisture sensor data to the cloud
func (c *Client) SendSensorData(deviceUID string, probeID uint8, moisturePercent uint8,
	temperature int16, batteryMV uint16, timestamp time.Time) error {

	payload := map[string]interface{}{
		"device_uid":       deviceUID,
		"probe_id":         probeID,
		"moisture_percent": moisturePercent,
		"temperature":      temperature,
		"battery_mv":       batteryMV,
		"reading_time":     timestamp.Unix(),
	}

	data, err := json.Marshal(payload)
	if err != nil {
		return err
	}

	return c.Send(&Message{
		Type:    MsgTypeSensorData,
		Payload: data,
	})
}

// SendWaterMeterData sends water meter data to the cloud
func (c *Client) SendWaterMeterData(deviceUID string, totalLiters uint32,
	flowRateLPM float32, batteryMV uint16, timestamp time.Time) error {

	payload := map[string]interface{}{
		"device_uid":    deviceUID,
		"total_liters":  totalLiters,
		"flow_rate_lpm": flowRateLPM,
		"battery_mv":    batteryMV,
		"reading_time":  timestamp.Unix(),
	}

	data, err := json.Marshal(payload)
	if err != nil {
		return err
	}

	return c.Send(&Message{
		Type:    MsgTypeWaterMeter,
		Payload: data,
	})
}

// SendValveEvent sends a valve state change event to the cloud
func (c *Client) SendValveEvent(controllerUID string, actuatorAddr uint8,
	prevState, newState uint8, source string, timestamp time.Time) error {

	payload := map[string]interface{}{
		"controller_uid": controllerUID,
		"actuator_addr":  actuatorAddr,
		"prev_state":     prevState,
		"new_state":      newState,
		"source":         source,
		"event_time":     timestamp.Unix(),
	}

	data, err := json.Marshal(payload)
	if err != nil {
		return err
	}

	return c.Send(&Message{
		Type:    MsgTypeValveEvent,
		Payload: data,
	})
}

// SendValveAck sends a valve command acknowledgment to the cloud
func (c *Client) SendValveAck(controllerUID string, actuatorAddr uint8,
	commandID uint16, resultState uint8, success bool) error {

	payload := map[string]interface{}{
		"controller_uid": controllerUID,
		"actuator_addr":  actuatorAddr,
		"command_id":     commandID,
		"result_state":   resultState,
		"success":        success,
		"ack_time":       time.Now().Unix(),
	}

	data, err := json.Marshal(payload)
	if err != nil {
		return err
	}

	return c.Send(&Message{
		Type:    MsgTypeValveAck,
		Payload: data,
	})
}

// connectionLoop manages the WebSocket connection with automatic reconnection
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
			time.Sleep(c.config.ReconnectDelay)
			continue
		}

		// Run read/write loops until disconnected
		c.runMessageLoops(ctx)

		// Disconnected, wait before reconnecting
		log.Println("Disconnected from cloud, reconnecting...")
		time.Sleep(c.config.ReconnectDelay)
	}
}

// connect establishes the WebSocket connection
func (c *Client) connect() error {
	// Add authentication headers
	header := make(map[string][]string)
	header["X-Property-UID"] = []string{c.config.PropertyUID}
	header["X-API-Key"] = []string{c.config.APIKey}

	dialer := websocket.Dialer{
		HandshakeTimeout: 10 * time.Second,
	}

	conn, _, err := dialer.Dial(c.config.URL, header)
	if err != nil {
		return fmt.Errorf("dial failed: %w", err)
	}

	c.mu.Lock()
	c.conn = conn
	c.connected = true
	c.mu.Unlock()

	log.Printf("Connected to cloud: %s", c.config.URL)
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

	// Ping loop
	wg.Add(1)
	go func() {
		defer wg.Done()
		c.pingLoop(done)
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
		}
	}
}

// pingLoop sends periodic pings to keep the connection alive
func (c *Client) pingLoop(done chan struct{}) {
	ticker := time.NewTicker(c.config.PingInterval)
	defer ticker.Stop()

	for {
		select {
		case <-done:
			return
		case <-c.stopChan:
			return
		case <-ticker.C:
			c.mu.Lock()
			conn := c.conn
			c.mu.Unlock()

			if conn == nil {
				return
			}

			// Send heartbeat message
			heartbeat := &Message{
				Type:      MsgTypeHeartbeat,
				Timestamp: time.Now().Unix(),
			}

			data, _ := json.Marshal(heartbeat)
			conn.SetWriteDeadline(time.Now().Add(c.config.WriteTimeout))
			if err := conn.WriteMessage(websocket.TextMessage, data); err != nil {
				log.Printf("Heartbeat failed: %v", err)
				return
			}
		}
	}
}

// handleMessage processes an incoming message
func (c *Client) handleMessage(msg *Message) {
	c.mu.Lock()
	onConfig := c.onConfig
	onDeviceList := c.onDeviceList
	onSchedule := c.onSchedule
	onValveCommand := c.onValveCommand
	c.mu.Unlock()

	switch msg.Type {
	case MsgTypeConfig:
		if onConfig != nil {
			onConfig(msg.Payload)
		}

	case MsgTypeDeviceList:
		if onDeviceList != nil {
			onDeviceList(msg.Payload)
		}

	case MsgTypeScheduleUpdate:
		if onSchedule != nil {
			onSchedule(msg.Payload)
		}

	case MsgTypeValveCommand:
		if onValveCommand != nil {
			onValveCommand(msg.Payload)
		}

	default:
		log.Printf("Unknown message type: %s", msg.Type)
	}
}

// ValveCommandPayload represents a valve command from the cloud
type ValveCommandPayload struct {
	ControllerUID string `json:"controller_uid"`
	ActuatorAddr  uint8  `json:"actuator_addr"`
	Command       string `json:"command"` // "open", "close", "stop"
	CommandID     string `json:"command_id"`
	Priority      string `json:"priority"` // "normal", "immediate", "emergency"
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
	ControllerUID string          `json:"controller_uid"`
	ScheduleUID   string          `json:"schedule_uid"`
	Version       uint16          `json:"version"`
	Name          string          `json:"name"`
	IsActive      bool            `json:"is_active"`
	Entries       []ScheduleEntry `json:"entries"`
}

// ScheduleEntry represents a single schedule entry
type ScheduleEntry struct {
	DayMask      uint8  `json:"day_mask"`
	StartHour    uint8  `json:"start_hour"`
	StartMinute  uint8  `json:"start_minute"`
	DurationMins uint16 `json:"duration_mins"`
	ActuatorMask uint64 `json:"actuator_mask"`
}

// ParseScheduleUpdate parses a schedule update payload
func ParseScheduleUpdate(data json.RawMessage) (*ScheduleUpdatePayload, error) {
	var schedule ScheduleUpdatePayload
	if err := json.Unmarshal(data, &schedule); err != nil {
		return nil, err
	}
	return &schedule, nil
}

// DeviceListPayload represents the device list from the cloud
type DeviceListPayload struct {
	PropertyUID  string       `json:"property_uid"`
	PropertyName string       `json:"property_name"`
	Devices      []DeviceInfo `json:"devices"`
}

// DeviceInfo represents device information from the cloud
type DeviceInfo struct {
	UID        string `json:"uid"`
	DeviceType uint8  `json:"device_type"`
	Name       string `json:"name"`
	Alias      string `json:"alias,omitempty"`
	ZoneUID    string `json:"zone_uid,omitempty"`
}

// ParseDeviceList parses a device list payload
func ParseDeviceList(data json.RawMessage) (*DeviceListPayload, error) {
	var list DeviceListPayload
	if err := json.Unmarshal(data, &list); err != nil {
		return nil, err
	}
	return &list, nil
}
