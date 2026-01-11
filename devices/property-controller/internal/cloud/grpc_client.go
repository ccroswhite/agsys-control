// Package cloud provides communication with the AgSys cloud service via gRPC.
package cloud

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"math/rand"
	"sync"
	"time"

	controllerv1 "github.com/ccroswhite/agsys-api/gen/go/proto/controller/v1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/keepalive"
	"google.golang.org/grpc/metadata"
	"google.golang.org/protobuf/types/known/timestamppb"
)

const (
	// authTokenMetadataKey is the metadata key for the session token
	authTokenMetadataKey = "x-controller-token"
)

// GRPCConfig holds gRPC client configuration
type GRPCConfig struct {
	ServerAddr   string // gRPC server address (e.g., "api.agsys.io:50051")
	ControllerID string // Controller UUID
	APIKey       string // API key for authentication
	UseTLS       bool   // Whether to use TLS

	// Reconnection settings (exponential backoff)
	InitialRetryDelay time.Duration
	MaxRetryDelay     time.Duration
	BackoffMultiplier float64
	JitterPercent     float64

	// Keepalive settings
	KeepaliveTime    time.Duration
	KeepaliveTimeout time.Duration
}

// DefaultGRPCConfig returns default gRPC client configuration
func DefaultGRPCConfig() GRPCConfig {
	return GRPCConfig{
		UseTLS:            true,
		InitialRetryDelay: 1 * time.Second,
		MaxRetryDelay:     60 * time.Second,
		BackoffMultiplier: 2.0,
		JitterPercent:     0.25,
		KeepaliveTime:     30 * time.Second,
		KeepaliveTimeout:  10 * time.Second,
	}
}

// GRPCClient handles bidirectional gRPC communication with AgSys backend
type GRPCClient struct {
	config GRPCConfig
	conn   *grpc.ClientConn
	client controllerv1.ControllerServiceClient
	stream controllerv1.ControllerService_ConnectClient

	sendChan  chan *controllerv1.ControllerMessage
	stopChan  chan struct{}
	wg        sync.WaitGroup
	mu        sync.Mutex
	connected bool

	// Current retry delay for exponential backoff
	currentRetryDelay time.Duration

	// Firmware version for heartbeats
	firmwareVersion string

	// Session token from authentication
	sessionToken string

	// Callbacks for messages from backend
	onValveCommand    func(*controllerv1.ValveCommand)
	onSchedule        func(*controllerv1.ScheduleUpdate)
	onDeviceAdded     func(*controllerv1.DeviceApproved)
	onConfigUpdate    func(*controllerv1.ConfigUpdate)
	onMeterPinCommand func(*controllerv1.MeterPinCommand)
}

// NewGRPCClient creates a new gRPC cloud client
func NewGRPCClient(config GRPCConfig) *GRPCClient {
	return &GRPCClient{
		config:            config,
		sendChan:          make(chan *controllerv1.ControllerMessage, 100),
		stopChan:          make(chan struct{}),
		currentRetryDelay: config.InitialRetryDelay,
		firmwareVersion:   "1.0.0",
	}
}

// SetFirmwareVersion sets the firmware version reported in heartbeats
func (c *GRPCClient) SetFirmwareVersion(version string) {
	c.firmwareVersion = version
}

// SetValveCommandHandler sets the callback for valve commands
func (c *GRPCClient) SetValveCommandHandler(handler func(*controllerv1.ValveCommand)) {
	c.onValveCommand = handler
}

// SetScheduleHandler sets the callback for schedule updates
func (c *GRPCClient) SetScheduleHandler(handler func(*controllerv1.ScheduleUpdate)) {
	c.onSchedule = handler
}

// SetDeviceAddedHandler sets the callback for device approval notifications
func (c *GRPCClient) SetDeviceAddedHandler(handler func(*controllerv1.DeviceApproved)) {
	c.onDeviceAdded = handler
}

// SetConfigUpdateHandler sets the callback for config updates
func (c *GRPCClient) SetConfigUpdateHandler(handler func(*controllerv1.ConfigUpdate)) {
	c.onConfigUpdate = handler
}

// Connect establishes connection to the gRPC server
func (c *GRPCClient) Connect(ctx context.Context) error {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.connected {
		return nil
	}

	// Set up dial options
	opts := []grpc.DialOption{
		grpc.WithKeepaliveParams(keepalive.ClientParameters{
			Time:                c.config.KeepaliveTime,
			Timeout:             c.config.KeepaliveTimeout,
			PermitWithoutStream: true,
		}),
	}

	if c.config.UseTLS {
		creds := credentials.NewClientTLSFromCert(nil, "")
		opts = append(opts, grpc.WithTransportCredentials(creds))
	} else {
		opts = append(opts, grpc.WithTransportCredentials(insecure.NewCredentials()))
	}

	// Connect to server
	conn, err := grpc.DialContext(ctx, c.config.ServerAddr, opts...)
	if err != nil {
		return fmt.Errorf("failed to connect: %w", err)
	}
	c.conn = conn
	c.client = controllerv1.NewControllerServiceClient(conn)

	// Authenticate
	authResp, err := c.client.Authenticate(ctx, &controllerv1.AuthRequest{
		ControllerId:    c.config.ControllerID,
		ApiKey:          c.config.APIKey,
		FirmwareVersion: c.firmwareVersion,
	})
	if err != nil {
		conn.Close()
		return fmt.Errorf("authentication failed: %w", err)
	}
	if !authResp.Success {
		conn.Close()
		return fmt.Errorf("authentication rejected: %s", authResp.ErrorMessage)
	}

	// Store session token for subsequent requests
	c.sessionToken = authResp.SessionToken

	// Establish bidirectional stream with session token in metadata
	streamCtx := c.contextWithAuth(ctx)
	stream, err := c.client.Connect(streamCtx)
	if err != nil {
		conn.Close()
		return fmt.Errorf("failed to establish stream: %w", err)
	}
	c.stream = stream

	// Send initial heartbeat
	if err := c.sendHeartbeat(); err != nil {
		conn.Close()
		return fmt.Errorf("failed to send initial heartbeat: %w", err)
	}

	c.connected = true
	c.currentRetryDelay = c.config.InitialRetryDelay

	// Start sender and receiver goroutines
	c.wg.Add(2)
	go c.sendLoop()
	go c.receiveLoop()

	log.Printf("Connected to AgSys backend at %s", c.config.ServerAddr)
	return nil
}

// ConnectWithRetry connects with automatic reconnection on failure
func (c *GRPCClient) ConnectWithRetry(ctx context.Context) {
	for {
		select {
		case <-ctx.Done():
			return
		case <-c.stopChan:
			return
		default:
		}

		err := c.Connect(ctx)
		if err == nil {
			return
		}

		log.Printf("Connection failed: %v, retrying in %v", err, c.currentRetryDelay)

		// Wait with jitter
		jitter := time.Duration(float64(c.currentRetryDelay) * c.config.JitterPercent * (rand.Float64()*2 - 1))
		time.Sleep(c.currentRetryDelay + jitter)

		// Increase delay for next attempt
		c.currentRetryDelay = time.Duration(float64(c.currentRetryDelay) * c.config.BackoffMultiplier)
		if c.currentRetryDelay > c.config.MaxRetryDelay {
			c.currentRetryDelay = c.config.MaxRetryDelay
		}
	}
}

// Close closes the connection
func (c *GRPCClient) Close() error {
	c.mu.Lock()
	defer c.mu.Unlock()

	if !c.connected {
		return nil
	}

	close(c.stopChan)
	c.wg.Wait()

	if c.stream != nil {
		c.stream.CloseSend()
	}
	if c.conn != nil {
		c.conn.Close()
	}

	c.connected = false
	c.stopChan = make(chan struct{})
	return nil
}

// IsConnected returns whether the client is connected
func (c *GRPCClient) IsConnected() bool {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.connected
}

func (c *GRPCClient) sendLoop() {
	defer c.wg.Done()

	for {
		select {
		case msg := <-c.sendChan:
			if err := c.stream.Send(msg); err != nil {
				log.Printf("Failed to send message: %v", err)
				c.handleDisconnect()
				return
			}
		case <-c.stopChan:
			return
		}
	}
}

func (c *GRPCClient) receiveLoop() {
	defer c.wg.Done()

	for {
		select {
		case <-c.stopChan:
			return
		default:
		}

		msg, err := c.stream.Recv()
		if err == io.EOF {
			log.Println("Stream closed by server")
			c.handleDisconnect()
			return
		}
		if err != nil {
			log.Printf("Receive error: %v", err)
			c.handleDisconnect()
			return
		}

		c.handleBackendMessage(msg)
	}
}

func (c *GRPCClient) handleBackendMessage(msg *controllerv1.BackendMessage) {
	// Send acknowledgment
	c.sendAck(msg.MessageId, true, "")

	switch payload := msg.Payload.(type) {
	case *controllerv1.BackendMessage_ValveCommand:
		if c.onValveCommand != nil {
			c.onValveCommand(payload.ValveCommand)
		}
	case *controllerv1.BackendMessage_ScheduleUpdate:
		if c.onSchedule != nil {
			c.onSchedule(payload.ScheduleUpdate)
		}
	case *controllerv1.BackendMessage_DeviceApproved:
		if c.onDeviceAdded != nil {
			c.onDeviceAdded(payload.DeviceApproved)
		}
	case *controllerv1.BackendMessage_ConfigUpdate:
		if c.onConfigUpdate != nil {
			c.onConfigUpdate(payload.ConfigUpdate)
		}
	case *controllerv1.BackendMessage_Ping:
		// Respond with heartbeat
		c.SendHeartbeat(0, nil)
	}
}

func (c *GRPCClient) handleDisconnect() {
	c.mu.Lock()
	c.connected = false
	c.mu.Unlock()

	// Trigger reconnection in background
	go c.ConnectWithRetry(context.Background())
}

func (c *GRPCClient) sendAck(messageID string, success bool, errorMsg string) {
	// Acks are sent via CommandAck message type
	// For now, we just log - could implement proper ack tracking
	log.Printf("Acknowledged message %s (success=%v)", messageID, success)
}

// SendHeartbeat sends a heartbeat to the backend
func (c *GRPCClient) SendHeartbeat(uptimeSeconds int64, loraStats *controllerv1.LoRaStats) error {
	msg := &controllerv1.ControllerMessage{
		Payload: &controllerv1.ControllerMessage_Heartbeat{
			Heartbeat: &controllerv1.Heartbeat{
				Timestamp:       timestamppb.Now(),
				UptimeSeconds:   uptimeSeconds,
				FirmwareVersion: c.firmwareVersion,
				LoraStats:       loraStats,
			},
		},
	}

	select {
	case c.sendChan <- msg:
		return nil
	default:
		return fmt.Errorf("send buffer full")
	}
}

func (c *GRPCClient) sendHeartbeat() error {
	return c.SendHeartbeat(0, nil)
}

// SendSensorData sends sensor readings to the backend
func (c *GRPCClient) SendSensorData(deviceUID string, readings []*controllerv1.SensorReading) error {
	msg := &controllerv1.ControllerMessage{
		Payload: &controllerv1.ControllerMessage_SensorData{
			SensorData: &controllerv1.SensorDataBatch{
				DeviceUid: deviceUID,
				Readings:  readings,
			},
		},
	}

	select {
	case c.sendChan <- msg:
		return nil
	default:
		return fmt.Errorf("send buffer full")
	}
}

// SendMeterData sends water meter readings to the backend
func (c *GRPCClient) SendMeterData(deviceUID string, readings []*controllerv1.MeterReading) error {
	msg := &controllerv1.ControllerMessage{
		Payload: &controllerv1.ControllerMessage_MeterData{
			MeterData: &controllerv1.MeterDataBatch{
				DeviceUid: deviceUID,
				Readings:  readings,
			},
		},
	}

	select {
	case c.sendChan <- msg:
		return nil
	default:
		return fmt.Errorf("send buffer full")
	}
}

// MeterAlarmData holds meter alarm information for cloud transmission
type MeterAlarmData struct {
	AlarmType   uint8
	FlowRateLPM float32
	DurationSec uint32
	TotalLiters uint32
	RSSI        int16
	Timestamp   time.Time
}

// mapAlarmType converts internal alarm type to protobuf enum
func mapAlarmType(alarmType uint8) controllerv1.MeterAlarmType {
	switch alarmType {
	case 0:
		return controllerv1.MeterAlarmType_METER_ALARM_TYPE_CLEARED
	case 1:
		return controllerv1.MeterAlarmType_METER_ALARM_TYPE_LEAK
	case 2:
		return controllerv1.MeterAlarmType_METER_ALARM_TYPE_REVERSE_FLOW
	case 3:
		return controllerv1.MeterAlarmType_METER_ALARM_TYPE_TAMPER
	case 4:
		return controllerv1.MeterAlarmType_METER_ALARM_TYPE_HIGH_FLOW
	default:
		return controllerv1.MeterAlarmType_METER_ALARM_TYPE_UNSPECIFIED
	}
}

// SendMeterAlarm sends a water meter alarm to the backend (high priority)
func (c *GRPCClient) SendMeterAlarm(deviceUID string, alarm *MeterAlarmData) error {
	msg := &controllerv1.ControllerMessage{
		Payload: &controllerv1.ControllerMessage_MeterAlarm{
			MeterAlarm: &controllerv1.MeterAlarm{
				DeviceUid:       deviceUID,
				AlarmType:       mapAlarmType(alarm.AlarmType),
				FlowRateLpm:     alarm.FlowRateLPM,
				DurationSeconds: int64(alarm.DurationSec),
				TotalLiters:     float64(alarm.TotalLiters),
				Timestamp:       timestamppb.New(alarm.Timestamp),
				SignalRssi:      int32(alarm.RSSI),
			},
		},
	}

	log.Printf("Sending meter alarm to cloud: device=%s type=%s flow=%.1f duration=%ds",
		deviceUID, mapAlarmType(alarm.AlarmType).String(), alarm.FlowRateLPM, alarm.DurationSec)

	select {
	case c.sendChan <- msg:
		return nil
	default:
		return fmt.Errorf("send buffer full")
	}
}

// SendValveStatus sends valve status updates to the backend
func (c *GRPCClient) SendValveStatus(controllerUID string, actuators []*controllerv1.ActuatorStatus) error {
	msg := &controllerv1.ControllerMessage{
		Payload: &controllerv1.ControllerMessage_ValveStatus{
			ValveStatus: &controllerv1.ValveStatusReport{
				ControllerUid: controllerUID,
				Actuators:     actuators,
			},
		},
	}

	select {
	case c.sendChan <- msg:
		return nil
	default:
		return fmt.Errorf("send buffer full")
	}
}

// SendDeviceDiscovery reports a newly discovered device
func (c *GRPCClient) SendDeviceDiscovery(deviceUID, deviceType, firmwareVersion string, signalRSSI int32) error {
	msg := &controllerv1.ControllerMessage{
		Payload: &controllerv1.ControllerMessage_DeviceDiscovery{
			DeviceDiscovery: &controllerv1.DeviceDiscovery{
				DeviceUid:       deviceUID,
				DeviceType:      deviceType,
				FirmwareVersion: firmwareVersion,
				FirstSeen:       timestamppb.Now(),
				SignalRssi:      signalRSSI,
			},
		},
	}

	select {
	case c.sendChan <- msg:
		return nil
	default:
		return fmt.Errorf("send buffer full")
	}
}

// SendCommandAck acknowledges a command from the backend
func (c *GRPCClient) SendCommandAck(commandID string, success bool, errorMessage string) error {
	msg := &controllerv1.ControllerMessage{
		Payload: &controllerv1.ControllerMessage_CommandAck{
			CommandAck: &controllerv1.CommandAck{
				CommandId:    commandID,
				Success:      success,
				ErrorMessage: errorMessage,
				ExecutedAt:   timestamppb.Now(),
			},
		},
	}

	select {
	case c.sendChan <- msg:
		return nil
	default:
		return fmt.Errorf("send buffer full")
	}
}

// contextWithAuth returns a context with the session token in metadata
func (c *GRPCClient) contextWithAuth(ctx context.Context) context.Context {
	if c.sessionToken == "" {
		return ctx
	}
	md := metadata.Pairs(authTokenMetadataKey, c.sessionToken)
	return metadata.NewOutgoingContext(ctx, md)
}

// Helper to convert legacy JSON payloads (for backwards compatibility during migration)
func (c *GRPCClient) SendLegacySensorData(deviceUID string, readings []json.RawMessage) error {
	// This would parse the legacy format and convert to proto
	// For now, just log a warning
	log.Printf("Warning: SendLegacySensorData called - use SendSensorData with proto types")
	return nil
}
