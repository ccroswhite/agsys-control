// Package storage provides SQLite database operations for the property controller.
package storage

import "time"

// Property represents the property this controller manages
type Property struct {
	UID       string    `json:"uid"`
	Name      string    `json:"name"`
	Alias     string    `json:"alias,omitempty"`
	UpdatedAt time.Time `json:"updated_at"`
}

// Zone represents a watering zone within the property
type Zone struct {
	UID        string    `json:"uid"`
	PropertyID string    `json:"property_id"`
	Name       string    `json:"name"`
	Alias      string    `json:"alias,omitempty"`
	UpdatedAt  time.Time `json:"updated_at"`
}

// Device represents a registered IoT device
type Device struct {
	UID          string    `json:"uid"`         // MCU unique ID (hex string)
	DeviceType   uint8     `json:"device_type"` // Device type code
	Name         string    `json:"name"`
	Alias        string    `json:"alias,omitempty"`
	ZoneID       string    `json:"zone_id,omitempty"` // Associated zone
	FirstSeen    time.Time `json:"first_seen"`
	LastSeen     time.Time `json:"last_seen"`
	FirmwareVer  string    `json:"firmware_version,omitempty"`
	BatteryMV    uint16    `json:"battery_mv,omitempty"`
	RSSI         int16     `json:"rssi,omitempty"`
	IsRegistered bool      `json:"is_registered"` // True if registered in AgSys
	UpdatedAt    time.Time `json:"updated_at"`
}

// ValveActuator represents an individual valve actuator connected to a valve controller
type ValveActuator struct {
	UID             string    `json:"uid"`            // Unique ID (controller_uid + address)
	ControllerUID   string    `json:"controller_uid"` // Parent valve controller UID
	Address         uint8     `json:"address"`        // DIP switch address (0-63)
	Name            string    `json:"name"`
	Alias           string    `json:"alias,omitempty"`
	ZoneID          string    `json:"zone_id,omitempty"`
	CurrentState    uint8     `json:"current_state"` // Current valve state
	LastStateChange time.Time `json:"last_state_change"`
	IsRegistered    bool      `json:"is_registered"`
	UpdatedAt       time.Time `json:"updated_at"`
}

// SoilMoistureReading represents a soil moisture sensor reading
type SoilMoistureReading struct {
	ID              int64     `json:"id"`
	DeviceUID       string    `json:"device_uid"`
	ProbeID         uint8     `json:"probe_id"` // Probe index 0-3
	MoistureRaw     uint16    `json:"moisture_raw"`
	MoisturePercent uint8     `json:"moisture_percent"`
	Temperature     int16     `json:"temperature"` // 0.1°C units
	BatteryMV       uint16    `json:"battery_mv"`
	RSSI            int16     `json:"rssi"`
	Timestamp       time.Time `json:"timestamp"`
	SyncedToCloud   bool      `json:"synced_to_cloud"`
}

// WaterMeterReading represents a water meter reading
type WaterMeterReading struct {
	ID            int64     `json:"id"`
	DeviceUID     string    `json:"device_uid"`
	TotalLiters   uint32    `json:"total_liters"`
	FlowRateLPM   float32   `json:"flow_rate_lpm"` // Liters per minute
	BatteryMV     uint16    `json:"battery_mv"`
	RSSI          int16     `json:"rssi"`
	Timestamp     time.Time `json:"timestamp"`
	SyncedToCloud bool      `json:"synced_to_cloud"`
}

// ValveEvent represents a valve state change event
type ValveEvent struct {
	ID            int64     `json:"id"`
	ControllerUID string    `json:"controller_uid"`
	ActuatorAddr  uint8     `json:"actuator_addr"`
	PrevState     uint8     `json:"prev_state"`
	NewState      uint8     `json:"new_state"`
	CommandID     uint16    `json:"command_id,omitempty"` // If triggered by command
	Source        string    `json:"source"`               // "schedule", "manual", "emergency"
	Timestamp     time.Time `json:"timestamp"`
	SyncedToCloud bool      `json:"synced_to_cloud"`
}

// Schedule represents a watering schedule
type Schedule struct {
	ID            int64     `json:"id"`
	UID           string    `json:"uid"` // Schedule UID from AgSys
	ControllerUID string    `json:"controller_uid"`
	Version       uint16    `json:"version"`
	Name          string    `json:"name"`
	IsActive      bool      `json:"is_active"`
	CreatedAt     time.Time `json:"created_at"`
	UpdatedAt     time.Time `json:"updated_at"`
}

// ScheduleEntry represents a single entry in a schedule
type ScheduleEntry struct {
	ID           int64  `json:"id"`
	ScheduleID   int64  `json:"schedule_id"`
	DayMask      uint8  `json:"day_mask"` // Bit mask for days
	StartHour    uint8  `json:"start_hour"`
	StartMinute  uint8  `json:"start_minute"`
	DurationMins uint16 `json:"duration_mins"`
	ActuatorMask uint64 `json:"actuator_mask"` // Which actuators to activate
}

// PendingCommand represents a command waiting for acknowledgment
type PendingCommand struct {
	ID            int64     `json:"id"`
	CommandID     uint16    `json:"command_id"`
	ControllerUID string    `json:"controller_uid"`
	ActuatorAddr  uint8     `json:"actuator_addr"`
	Command       uint8     `json:"command"`
	CreatedAt     time.Time `json:"created_at"`
	ExpiresAt     time.Time `json:"expires_at"`
	Retries       int       `json:"retries"`
	MaxRetries    int       `json:"max_retries"`
	Acknowledged  bool      `json:"acknowledged"`
	AckTime       time.Time `json:"ack_time,omitempty"`
	ResultState   uint8     `json:"result_state,omitempty"`
}

// CloudSyncQueue represents items waiting to be synced to cloud
type CloudSyncQueue struct {
	ID        int64     `json:"id"`
	DataType  string    `json:"data_type"` // "sensor", "meter", "valve_event"
	DataID    int64     `json:"data_id"`   // ID in the source table
	Payload   string    `json:"payload"`   // JSON payload
	Priority  int       `json:"priority"`  // Higher = more urgent
	CreatedAt time.Time `json:"created_at"`
	Attempts  int       `json:"attempts"`
	LastError string    `json:"last_error,omitempty"`
}
