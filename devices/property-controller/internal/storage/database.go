package storage

import (
	"database/sql"
	"fmt"
	"time"

	_ "github.com/mattn/go-sqlite3"
)

// DB wraps the SQLite database connection
type DB struct {
	conn *sql.DB
}

// Open opens or creates the SQLite database
func Open(path string) (*DB, error) {
	conn, err := sql.Open("sqlite3", path+"?_journal_mode=WAL&_busy_timeout=5000")
	if err != nil {
		return nil, fmt.Errorf("failed to open database: %w", err)
	}

	db := &DB{conn: conn}
	if err := db.migrate(); err != nil {
		conn.Close()
		return nil, fmt.Errorf("failed to migrate database: %w", err)
	}

	return db, nil
}

// Close closes the database connection
func (db *DB) Close() error {
	return db.conn.Close()
}

// migrate creates the database schema
func (db *DB) migrate() error {
	schema := `
	-- Property configuration
	CREATE TABLE IF NOT EXISTS property (
		uid TEXT PRIMARY KEY,
		name TEXT NOT NULL,
		alias TEXT,
		updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
	);

	-- Zones within the property
	CREATE TABLE IF NOT EXISTS zones (
		uid TEXT PRIMARY KEY,
		property_id TEXT NOT NULL,
		name TEXT NOT NULL,
		alias TEXT,
		updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
		FOREIGN KEY (property_id) REFERENCES property(uid)
	);

	-- Registered devices
	CREATE TABLE IF NOT EXISTS devices (
		uid TEXT PRIMARY KEY,
		device_type INTEGER NOT NULL,
		name TEXT NOT NULL,
		alias TEXT,
		zone_id TEXT,
		first_seen DATETIME DEFAULT CURRENT_TIMESTAMP,
		last_seen DATETIME DEFAULT CURRENT_TIMESTAMP,
		firmware_version TEXT,
		battery_mv INTEGER,
		rssi INTEGER,
		is_registered INTEGER DEFAULT 0,
		updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
		FOREIGN KEY (zone_id) REFERENCES zones(uid)
	);

	-- Valve actuators (children of valve controllers)
	CREATE TABLE IF NOT EXISTS valve_actuators (
		uid TEXT PRIMARY KEY,
		controller_uid TEXT NOT NULL,
		address INTEGER NOT NULL,
		name TEXT NOT NULL,
		alias TEXT,
		zone_id TEXT,
		current_state INTEGER DEFAULT 0,
		last_state_change DATETIME,
		is_registered INTEGER DEFAULT 0,
		updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
		FOREIGN KEY (controller_uid) REFERENCES devices(uid),
		FOREIGN KEY (zone_id) REFERENCES zones(uid),
		UNIQUE(controller_uid, address)
	);

	-- Soil moisture readings
	CREATE TABLE IF NOT EXISTS soil_moisture_readings (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		device_uid TEXT NOT NULL,
		probe_id INTEGER NOT NULL,
		moisture_raw INTEGER NOT NULL,
		moisture_percent INTEGER NOT NULL,
		temperature INTEGER,
		battery_mv INTEGER,
		rssi INTEGER,
		timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
		synced_to_cloud INTEGER DEFAULT 0,
		FOREIGN KEY (device_uid) REFERENCES devices(uid)
	);
	CREATE INDEX IF NOT EXISTS idx_soil_moisture_device ON soil_moisture_readings(device_uid);
	CREATE INDEX IF NOT EXISTS idx_soil_moisture_timestamp ON soil_moisture_readings(timestamp);
	CREATE INDEX IF NOT EXISTS idx_soil_moisture_synced ON soil_moisture_readings(synced_to_cloud);

	-- Water meter readings
	CREATE TABLE IF NOT EXISTS water_meter_readings (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		device_uid TEXT NOT NULL,
		total_liters INTEGER NOT NULL,
		flow_rate_lpm REAL,
		battery_mv INTEGER,
		rssi INTEGER,
		timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
		synced_to_cloud INTEGER DEFAULT 0,
		FOREIGN KEY (device_uid) REFERENCES devices(uid)
	);
	CREATE INDEX IF NOT EXISTS idx_water_meter_device ON water_meter_readings(device_uid);
	CREATE INDEX IF NOT EXISTS idx_water_meter_timestamp ON water_meter_readings(timestamp);
	CREATE INDEX IF NOT EXISTS idx_water_meter_synced ON water_meter_readings(synced_to_cloud);

	-- Valve events
	CREATE TABLE IF NOT EXISTS valve_events (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		controller_uid TEXT NOT NULL,
		actuator_addr INTEGER NOT NULL,
		prev_state INTEGER,
		new_state INTEGER NOT NULL,
		command_id INTEGER,
		source TEXT NOT NULL,
		timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
		synced_to_cloud INTEGER DEFAULT 0,
		FOREIGN KEY (controller_uid) REFERENCES devices(uid)
	);
	CREATE INDEX IF NOT EXISTS idx_valve_events_controller ON valve_events(controller_uid);
	CREATE INDEX IF NOT EXISTS idx_valve_events_timestamp ON valve_events(timestamp);
	CREATE INDEX IF NOT EXISTS idx_valve_events_synced ON valve_events(synced_to_cloud);

	-- Watering schedules
	CREATE TABLE IF NOT EXISTS schedules (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		uid TEXT UNIQUE NOT NULL,
		controller_uid TEXT NOT NULL,
		version INTEGER NOT NULL,
		name TEXT NOT NULL,
		is_active INTEGER DEFAULT 1,
		created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
		updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
		FOREIGN KEY (controller_uid) REFERENCES devices(uid)
	);

	-- Schedule entries
	CREATE TABLE IF NOT EXISTS schedule_entries (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		schedule_id INTEGER NOT NULL,
		day_mask INTEGER NOT NULL,
		start_hour INTEGER NOT NULL,
		start_minute INTEGER NOT NULL,
		duration_mins INTEGER NOT NULL,
		actuator_mask INTEGER NOT NULL,
		FOREIGN KEY (schedule_id) REFERENCES schedules(id) ON DELETE CASCADE
	);

	-- Pending commands awaiting acknowledgment
	CREATE TABLE IF NOT EXISTS pending_commands (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		command_id INTEGER NOT NULL,
		controller_uid TEXT NOT NULL,
		actuator_addr INTEGER NOT NULL,
		command INTEGER NOT NULL,
		created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
		expires_at DATETIME NOT NULL,
		retries INTEGER DEFAULT 0,
		max_retries INTEGER DEFAULT 3,
		acknowledged INTEGER DEFAULT 0,
		ack_time DATETIME,
		result_state INTEGER,
		FOREIGN KEY (controller_uid) REFERENCES devices(uid)
	);
	CREATE INDEX IF NOT EXISTS idx_pending_commands_id ON pending_commands(command_id);
	CREATE INDEX IF NOT EXISTS idx_pending_commands_expires ON pending_commands(expires_at);

	-- Cloud sync queue
	CREATE TABLE IF NOT EXISTS cloud_sync_queue (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		data_type TEXT NOT NULL,
		data_id INTEGER NOT NULL,
		payload TEXT NOT NULL,
		priority INTEGER DEFAULT 0,
		created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
		attempts INTEGER DEFAULT 0,
		last_error TEXT
	);
	CREATE INDEX IF NOT EXISTS idx_sync_queue_priority ON cloud_sync_queue(priority DESC, created_at);

	-- Water meter alarms
	CREATE TABLE IF NOT EXISTS meter_alarms (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		device_uid TEXT NOT NULL,
		alarm_type INTEGER NOT NULL,
		flow_rate_lpm REAL,
		duration_sec INTEGER,
		total_liters INTEGER,
		rssi INTEGER,
		timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
		synced_to_cloud INTEGER DEFAULT 0,
		FOREIGN KEY (device_uid) REFERENCES devices(uid)
	);
	CREATE INDEX IF NOT EXISTS idx_meter_alarms_device ON meter_alarms(device_uid);
	CREATE INDEX IF NOT EXISTS idx_meter_alarms_timestamp ON meter_alarms(timestamp);
	CREATE INDEX IF NOT EXISTS idx_meter_alarms_synced ON meter_alarms(synced_to_cloud);

	-- Water meter configuration
	CREATE TABLE IF NOT EXISTS meter_configs (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		device_uid TEXT UNIQUE NOT NULL,
		config_version INTEGER NOT NULL,
		report_interval_sec INTEGER NOT NULL,
		pulses_per_liter INTEGER NOT NULL,
		leak_threshold_min INTEGER NOT NULL,
		max_flow_rate_lpm INTEGER NOT NULL,
		flags INTEGER NOT NULL,
		updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
		FOREIGN KEY (device_uid) REFERENCES devices(uid)
	);
	`

	_, err := db.conn.Exec(schema)
	return err
}

// --- Device Operations ---

// UpsertDevice inserts or updates a device
func (db *DB) UpsertDevice(d *Device) error {
	query := `
		INSERT INTO devices (uid, device_type, name, alias, zone_id, first_seen, last_seen, 
			firmware_version, battery_mv, rssi, is_registered, updated_at)
		VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
		ON CONFLICT(uid) DO UPDATE SET
			last_seen = excluded.last_seen,
			firmware_version = COALESCE(excluded.firmware_version, firmware_version),
			battery_mv = COALESCE(excluded.battery_mv, battery_mv),
			rssi = COALESCE(excluded.rssi, rssi),
			updated_at = excluded.updated_at
	`
	_, err := db.conn.Exec(query, d.UID, d.DeviceType, d.Name, d.Alias, d.ZoneID,
		d.FirstSeen, d.LastSeen, d.FirmwareVer, d.BatteryMV, d.RSSI, d.IsRegistered, time.Now())
	return err
}

// GetDevice retrieves a device by UID
func (db *DB) GetDevice(uid string) (*Device, error) {
	query := `SELECT uid, device_type, name, alias, zone_id, first_seen, last_seen,
		firmware_version, battery_mv, rssi, is_registered, updated_at
		FROM devices WHERE uid = ?`

	d := &Device{}
	var zoneID, alias, fwVer sql.NullString
	err := db.conn.QueryRow(query, uid).Scan(&d.UID, &d.DeviceType, &d.Name, &alias,
		&zoneID, &d.FirstSeen, &d.LastSeen, &fwVer, &d.BatteryMV, &d.RSSI, &d.IsRegistered, &d.UpdatedAt)
	if err != nil {
		return nil, err
	}
	d.Alias = alias.String
	d.ZoneID = zoneID.String
	d.FirmwareVer = fwVer.String
	return d, nil
}

// GetAllDevices retrieves all devices
func (db *DB) GetAllDevices() ([]*Device, error) {
	query := `SELECT uid, device_type, name, alias, zone_id, first_seen, last_seen,
		firmware_version, battery_mv, rssi, is_registered, updated_at FROM devices`

	rows, err := db.conn.Query(query)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var devices []*Device
	for rows.Next() {
		d := &Device{}
		var zoneID, alias, fwVer sql.NullString
		if err := rows.Scan(&d.UID, &d.DeviceType, &d.Name, &alias, &zoneID,
			&d.FirstSeen, &d.LastSeen, &fwVer, &d.BatteryMV, &d.RSSI, &d.IsRegistered, &d.UpdatedAt); err != nil {
			return nil, err
		}
		d.Alias = alias.String
		d.ZoneID = zoneID.String
		d.FirmwareVer = fwVer.String
		devices = append(devices, d)
	}
	return devices, rows.Err()
}

// IsDeviceRegistered checks if a device UID is in the registered list
func (db *DB) IsDeviceRegistered(uid string) (bool, error) {
	var registered bool
	err := db.conn.QueryRow("SELECT is_registered FROM devices WHERE uid = ?", uid).Scan(&registered)
	if err == sql.ErrNoRows {
		return false, nil
	}
	return registered, err
}

// --- Soil Moisture Operations ---

// InsertSoilMoistureReading inserts a new soil moisture reading
func (db *DB) InsertSoilMoistureReading(r *SoilMoistureReading) (int64, error) {
	query := `INSERT INTO soil_moisture_readings 
		(device_uid, probe_id, moisture_raw, moisture_percent, temperature, battery_mv, rssi, timestamp)
		VALUES (?, ?, ?, ?, ?, ?, ?, ?)`

	result, err := db.conn.Exec(query, r.DeviceUID, r.ProbeID, r.MoistureRaw,
		r.MoisturePercent, r.Temperature, r.BatteryMV, r.RSSI, r.Timestamp)
	if err != nil {
		return 0, err
	}
	return result.LastInsertId()
}

// GetSoilMoistureReadings retrieves readings for a device
func (db *DB) GetSoilMoistureReadings(deviceUID string, limit int) ([]*SoilMoistureReading, error) {
	query := `SELECT id, device_uid, probe_id, moisture_raw, moisture_percent, temperature,
		battery_mv, rssi, timestamp, synced_to_cloud
		FROM soil_moisture_readings WHERE device_uid = ?
		ORDER BY timestamp DESC LIMIT ?`

	rows, err := db.conn.Query(query, deviceUID, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var readings []*SoilMoistureReading
	for rows.Next() {
		r := &SoilMoistureReading{}
		if err := rows.Scan(&r.ID, &r.DeviceUID, &r.ProbeID, &r.MoistureRaw,
			&r.MoisturePercent, &r.Temperature, &r.BatteryMV, &r.RSSI, &r.Timestamp, &r.SyncedToCloud); err != nil {
			return nil, err
		}
		readings = append(readings, r)
	}
	return readings, rows.Err()
}

// GetUnsyncedSoilMoistureReadings retrieves readings not yet synced to cloud
func (db *DB) GetUnsyncedSoilMoistureReadings(limit int) ([]*SoilMoistureReading, error) {
	query := `SELECT id, device_uid, probe_id, moisture_raw, moisture_percent, temperature,
		battery_mv, rssi, timestamp, synced_to_cloud
		FROM soil_moisture_readings WHERE synced_to_cloud = 0
		ORDER BY timestamp LIMIT ?`

	rows, err := db.conn.Query(query, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var readings []*SoilMoistureReading
	for rows.Next() {
		r := &SoilMoistureReading{}
		if err := rows.Scan(&r.ID, &r.DeviceUID, &r.ProbeID, &r.MoistureRaw,
			&r.MoisturePercent, &r.Temperature, &r.BatteryMV, &r.RSSI, &r.Timestamp, &r.SyncedToCloud); err != nil {
			return nil, err
		}
		readings = append(readings, r)
	}
	return readings, rows.Err()
}

// MarkSoilMoistureReadingSynced marks a reading as synced
func (db *DB) MarkSoilMoistureReadingSynced(id int64) error {
	_, err := db.conn.Exec("UPDATE soil_moisture_readings SET synced_to_cloud = 1 WHERE id = ?", id)
	return err
}

// --- Water Meter Operations ---

// InsertWaterMeterReading inserts a new water meter reading
func (db *DB) InsertWaterMeterReading(r *WaterMeterReading) (int64, error) {
	query := `INSERT INTO water_meter_readings 
		(device_uid, total_volume_l, flow_rate_lpm, signal_uv, temperature_c, signal_quality, battery_mv, rssi, timestamp)
		VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`

	result, err := db.conn.Exec(query, r.DeviceUID, r.TotalVolumeL, r.FlowRateLPM,
		r.SignalUV, r.TemperatureC, r.SignalQuality, r.BatteryMV, r.RSSI, r.Timestamp)
	if err != nil {
		return 0, err
	}
	return result.LastInsertId()
}

// GetUnsyncedWaterMeterReadings retrieves readings not yet synced to cloud
func (db *DB) GetUnsyncedWaterMeterReadings(limit int) ([]*WaterMeterReading, error) {
	query := `SELECT id, device_uid, total_volume_l, flow_rate_lpm, signal_uv, temperature_c, signal_quality, battery_mv, rssi, timestamp, synced_to_cloud
		FROM water_meter_readings WHERE synced_to_cloud = 0
		ORDER BY timestamp LIMIT ?`

	rows, err := db.conn.Query(query, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var readings []*WaterMeterReading
	for rows.Next() {
		r := &WaterMeterReading{}
		if err := rows.Scan(&r.ID, &r.DeviceUID, &r.TotalVolumeL, &r.FlowRateLPM,
			&r.SignalUV, &r.TemperatureC, &r.SignalQuality, &r.BatteryMV, &r.RSSI, &r.Timestamp, &r.SyncedToCloud); err != nil {
			return nil, err
		}
		readings = append(readings, r)
	}
	return readings, rows.Err()
}

// MarkWaterMeterReadingSynced marks a reading as synced
func (db *DB) MarkWaterMeterReadingSynced(id int64) error {
	_, err := db.conn.Exec("UPDATE water_meter_readings SET synced_to_cloud = 1 WHERE id = ?", id)
	return err
}

// --- Meter Alarm Operations ---

// InsertMeterAlarm inserts a new meter alarm
func (db *DB) InsertMeterAlarm(a *MeterAlarm) (int64, error) {
	query := `INSERT INTO meter_alarms 
		(device_uid, alarm_type, flow_rate_lpm, duration_sec, total_volume_l, rssi, timestamp)
		VALUES (?, ?, ?, ?, ?, ?, ?)`

	result, err := db.conn.Exec(query, a.DeviceUID, a.AlarmType, a.FlowRateLPM,
		a.DurationSec, a.TotalVolumeL, a.RSSI, a.Timestamp)
	if err != nil {
		return 0, err
	}
	return result.LastInsertId()
}

// GetUnsyncedMeterAlarms retrieves alarms not yet synced to cloud
func (db *DB) GetUnsyncedMeterAlarms(limit int) ([]*MeterAlarm, error) {
	query := `SELECT id, device_uid, alarm_type, flow_rate_lpm, duration_sec, total_volume_l, rssi, timestamp, synced_to_cloud
		FROM meter_alarms WHERE synced_to_cloud = 0
		ORDER BY timestamp LIMIT ?`

	rows, err := db.conn.Query(query, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var alarms []*MeterAlarm
	for rows.Next() {
		a := &MeterAlarm{}
		if err := rows.Scan(&a.ID, &a.DeviceUID, &a.AlarmType, &a.FlowRateLPM,
			&a.DurationSec, &a.TotalVolumeL, &a.RSSI, &a.Timestamp, &a.SyncedToCloud); err != nil {
			return nil, err
		}
		alarms = append(alarms, a)
	}
	return alarms, rows.Err()
}

// MarkMeterAlarmSynced marks an alarm as synced
func (db *DB) MarkMeterAlarmSynced(id int64) error {
	_, err := db.conn.Exec("UPDATE meter_alarms SET synced_to_cloud = 1 WHERE id = ?", id)
	return err
}

// --- Valve Operations ---

// InsertValveEvent inserts a new valve event
func (db *DB) InsertValveEvent(e *ValveEvent) (int64, error) {
	query := `INSERT INTO valve_events 
		(controller_uid, actuator_addr, prev_state, new_state, command_id, source, timestamp)
		VALUES (?, ?, ?, ?, ?, ?, ?)`

	result, err := db.conn.Exec(query, e.ControllerUID, e.ActuatorAddr, e.PrevState,
		e.NewState, e.CommandID, e.Source, e.Timestamp)
	if err != nil {
		return 0, err
	}
	return result.LastInsertId()
}

// GetUnsyncedValveEvents retrieves events not yet synced to cloud
func (db *DB) GetUnsyncedValveEvents(limit int) ([]*ValveEvent, error) {
	query := `SELECT id, controller_uid, actuator_addr, prev_state, new_state, command_id, source, timestamp, synced_to_cloud
		FROM valve_events WHERE synced_to_cloud = 0
		ORDER BY timestamp LIMIT ?`

	rows, err := db.conn.Query(query, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var events []*ValveEvent
	for rows.Next() {
		e := &ValveEvent{}
		if err := rows.Scan(&e.ID, &e.ControllerUID, &e.ActuatorAddr, &e.PrevState,
			&e.NewState, &e.CommandID, &e.Source, &e.Timestamp, &e.SyncedToCloud); err != nil {
			return nil, err
		}
		events = append(events, e)
	}
	return events, rows.Err()
}

// MarkValveEventSynced marks an event as synced
func (db *DB) MarkValveEventSynced(id int64) error {
	_, err := db.conn.Exec("UPDATE valve_events SET synced_to_cloud = 1 WHERE id = ?", id)
	return err
}

// UpdateValveActuatorState updates the current state of a valve actuator
func (db *DB) UpdateValveActuatorState(controllerUID string, addr uint8, state uint8) error {
	uid := fmt.Sprintf("%s_%02d", controllerUID, addr)
	query := `INSERT INTO valve_actuators (uid, controller_uid, address, name, current_state, last_state_change)
		VALUES (?, ?, ?, ?, ?, ?)
		ON CONFLICT(uid) DO UPDATE SET current_state = excluded.current_state, last_state_change = excluded.last_state_change`

	_, err := db.conn.Exec(query, uid, controllerUID, addr, fmt.Sprintf("Valve %d", addr), state, time.Now())
	return err
}

// --- Pending Commands ---

// InsertPendingCommand inserts a new pending command
func (db *DB) InsertPendingCommand(cmd *PendingCommand) (int64, error) {
	query := `INSERT INTO pending_commands 
		(command_id, controller_uid, actuator_addr, command, expires_at, max_retries)
		VALUES (?, ?, ?, ?, ?, ?)`

	result, err := db.conn.Exec(query, cmd.CommandID, cmd.ControllerUID, cmd.ActuatorAddr,
		cmd.Command, cmd.ExpiresAt, cmd.MaxRetries)
	if err != nil {
		return 0, err
	}
	return result.LastInsertId()
}

// AcknowledgeCommand marks a command as acknowledged
func (db *DB) AcknowledgeCommand(commandID uint16, resultState uint8) error {
	query := `UPDATE pending_commands SET acknowledged = 1, ack_time = ?, result_state = ?
		WHERE command_id = ? AND acknowledged = 0`
	_, err := db.conn.Exec(query, time.Now(), resultState, commandID)
	return err
}

// GetPendingCommand retrieves a pending command by ID
func (db *DB) GetPendingCommand(commandID uint16) (*PendingCommand, error) {
	query := `SELECT id, command_id, controller_uid, actuator_addr, command, created_at,
		expires_at, retries, max_retries, acknowledged, ack_time, result_state
		FROM pending_commands WHERE command_id = ?`

	cmd := &PendingCommand{}
	var ackTime sql.NullTime
	err := db.conn.QueryRow(query, commandID).Scan(&cmd.ID, &cmd.CommandID, &cmd.ControllerUID,
		&cmd.ActuatorAddr, &cmd.Command, &cmd.CreatedAt, &cmd.ExpiresAt, &cmd.Retries,
		&cmd.MaxRetries, &cmd.Acknowledged, &ackTime, &cmd.ResultState)
	if err != nil {
		return nil, err
	}
	if ackTime.Valid {
		cmd.AckTime = ackTime.Time
	}
	return cmd, nil
}

// GetExpiredCommands retrieves commands that have expired without acknowledgment
func (db *DB) GetExpiredCommands() ([]*PendingCommand, error) {
	query := `SELECT id, command_id, controller_uid, actuator_addr, command, created_at,
		expires_at, retries, max_retries, acknowledged
		FROM pending_commands WHERE acknowledged = 0 AND expires_at < ? AND retries < max_retries`

	rows, err := db.conn.Query(query, time.Now())
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var commands []*PendingCommand
	for rows.Next() {
		cmd := &PendingCommand{}
		if err := rows.Scan(&cmd.ID, &cmd.CommandID, &cmd.ControllerUID, &cmd.ActuatorAddr,
			&cmd.Command, &cmd.CreatedAt, &cmd.ExpiresAt, &cmd.Retries, &cmd.MaxRetries, &cmd.Acknowledged); err != nil {
			return nil, err
		}
		commands = append(commands, cmd)
	}
	return commands, rows.Err()
}

// IncrementCommandRetry increments the retry count and updates expiry
func (db *DB) IncrementCommandRetry(id int64, newExpiry time.Time) error {
	_, err := db.conn.Exec("UPDATE pending_commands SET retries = retries + 1, expires_at = ? WHERE id = ?",
		newExpiry, id)
	return err
}

// --- Schedule Operations ---

// UpsertSchedule inserts or updates a schedule
func (db *DB) UpsertSchedule(s *Schedule, entries []ScheduleEntry) error {
	tx, err := db.conn.Begin()
	if err != nil {
		return err
	}
	defer tx.Rollback()

	// Upsert schedule
	query := `INSERT INTO schedules (uid, controller_uid, version, name, is_active, updated_at)
		VALUES (?, ?, ?, ?, ?, ?)
		ON CONFLICT(uid) DO UPDATE SET version = excluded.version, name = excluded.name,
			is_active = excluded.is_active, updated_at = excluded.updated_at`

	result, err := tx.Exec(query, s.UID, s.ControllerUID, s.Version, s.Name, s.IsActive, time.Now())
	if err != nil {
		return err
	}

	// Get schedule ID
	var scheduleID int64
	err = tx.QueryRow("SELECT id FROM schedules WHERE uid = ?", s.UID).Scan(&scheduleID)
	if err != nil {
		return err
	}

	// Delete old entries
	_, err = tx.Exec("DELETE FROM schedule_entries WHERE schedule_id = ?", scheduleID)
	if err != nil {
		return err
	}

	// Insert new entries
	for _, entry := range entries {
		_, err = tx.Exec(`INSERT INTO schedule_entries 
			(schedule_id, day_mask, start_hour, start_minute, duration_mins, actuator_mask)
			VALUES (?, ?, ?, ?, ?, ?)`,
			scheduleID, entry.DayMask, entry.StartHour, entry.StartMinute, entry.DurationMins, entry.ActuatorMask)
		if err != nil {
			return err
		}
	}

	_ = result // suppress unused warning
	return tx.Commit()
}

// GetScheduleForController retrieves the active schedule for a controller
func (db *DB) GetScheduleForController(controllerUID string) (*Schedule, []ScheduleEntry, error) {
	query := `SELECT id, uid, controller_uid, version, name, is_active, created_at, updated_at
		FROM schedules WHERE controller_uid = ? AND is_active = 1`

	s := &Schedule{}
	err := db.conn.QueryRow(query, controllerUID).Scan(&s.ID, &s.UID, &s.ControllerUID,
		&s.Version, &s.Name, &s.IsActive, &s.CreatedAt, &s.UpdatedAt)
	if err != nil {
		return nil, nil, err
	}

	// Get entries
	rows, err := db.conn.Query(`SELECT id, schedule_id, day_mask, start_hour, start_minute, duration_mins, actuator_mask
		FROM schedule_entries WHERE schedule_id = ?`, s.ID)
	if err != nil {
		return nil, nil, err
	}
	defer rows.Close()

	var entries []ScheduleEntry
	for rows.Next() {
		var e ScheduleEntry
		if err := rows.Scan(&e.ID, &e.ScheduleID, &e.DayMask, &e.StartHour, &e.StartMinute,
			&e.DurationMins, &e.ActuatorMask); err != nil {
			return nil, nil, err
		}
		entries = append(entries, e)
	}

	return s, entries, rows.Err()
}
