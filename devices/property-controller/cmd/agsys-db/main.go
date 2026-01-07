// AgSys Database CLI Tool
// Provides command-line access to the property controller database
package main

import (
	"database/sql"
	"fmt"
	"os"
	"strings"
	"text/tabwriter"
	"time"

	_ "github.com/mattn/go-sqlite3"
	"github.com/spf13/cobra"
)

var (
	dbPath  string
	rootCmd = &cobra.Command{
		Use:   "agsys-db",
		Short: "AgSys Database CLI",
		Long:  "Command-line tool for inspecting and managing the AgSys property controller database.",
	}

	devicesCmd = &cobra.Command{
		Use:   "devices",
		Short: "List all devices",
		RunE:  listDevices,
	}

	sensorCmd = &cobra.Command{
		Use:   "sensor [device-uid]",
		Short: "Show soil moisture readings",
		Args:  cobra.MaximumNArgs(1),
		RunE:  showSensorData,
	}

	meterCmd = &cobra.Command{
		Use:   "meter [device-uid]",
		Short: "Show water meter readings",
		Args:  cobra.MaximumNArgs(1),
		RunE:  showMeterData,
	}

	valvesCmd = &cobra.Command{
		Use:   "valves",
		Short: "Show valve actuator states",
		RunE:  showValves,
	}

	eventsCmd = &cobra.Command{
		Use:   "events [controller-uid]",
		Short: "Show valve events",
		Args:  cobra.MaximumNArgs(1),
		RunE:  showEvents,
	}

	schedulesCmd = &cobra.Command{
		Use:   "schedules",
		Short: "Show watering schedules",
		RunE:  showSchedules,
	}

	pendingCmd = &cobra.Command{
		Use:   "pending",
		Short: "Show pending commands",
		RunE:  showPending,
	}

	statsCmd = &cobra.Command{
		Use:   "stats",
		Short: "Show database statistics",
		RunE:  showStats,
	}

	queryCmd = &cobra.Command{
		Use:   "query [sql]",
		Short: "Execute a raw SQL query",
		Args:  cobra.ExactArgs(1),
		RunE:  executeQuery,
	}

	limit int
)

func init() {
	rootCmd.PersistentFlags().StringVarP(&dbPath, "database", "d", "/var/lib/agsys/controller.db", "Database file path")

	sensorCmd.Flags().IntVarP(&limit, "limit", "n", 20, "Number of records to show")
	meterCmd.Flags().IntVarP(&limit, "limit", "n", 20, "Number of records to show")
	eventsCmd.Flags().IntVarP(&limit, "limit", "n", 20, "Number of records to show")

	rootCmd.AddCommand(devicesCmd)
	rootCmd.AddCommand(sensorCmd)
	rootCmd.AddCommand(meterCmd)
	rootCmd.AddCommand(valvesCmd)
	rootCmd.AddCommand(eventsCmd)
	rootCmd.AddCommand(schedulesCmd)
	rootCmd.AddCommand(pendingCmd)
	rootCmd.AddCommand(statsCmd)
	rootCmd.AddCommand(queryCmd)
}

func main() {
	if err := rootCmd.Execute(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func openDB() (*sql.DB, error) {
	return sql.Open("sqlite3", dbPath+"?mode=ro")
}

func listDevices(cmd *cobra.Command, args []string) error {
	db, err := openDB()
	if err != nil {
		return err
	}
	defer db.Close()

	rows, err := db.Query(`
		SELECT uid, device_type, name, alias, zone_id, last_seen, battery_mv, rssi, is_registered
		FROM devices ORDER BY last_seen DESC
	`)
	if err != nil {
		return err
	}
	defer rows.Close()

	w := tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
	fmt.Fprintln(w, "UID\tTYPE\tNAME\tALIAS\tZONE\tLAST SEEN\tBATTERY\tRSSI\tREG")
	fmt.Fprintln(w, "---\t----\t----\t-----\t----\t---------\t-------\t----\t---")

	for rows.Next() {
		var uid, name string
		var deviceType int
		var alias, zoneID sql.NullString
		var lastSeen time.Time
		var batteryMV, rssi sql.NullInt64
		var isRegistered bool

		if err := rows.Scan(&uid, &deviceType, &name, &alias, &zoneID, &lastSeen, &batteryMV, &rssi, &isRegistered); err != nil {
			return err
		}

		typeStr := deviceTypeString(deviceType)
		aliasStr := alias.String
		zoneStr := zoneID.String
		if zoneStr == "" {
			zoneStr = "-"
		}
		battStr := "-"
		if batteryMV.Valid {
			battStr = fmt.Sprintf("%dmV", batteryMV.Int64)
		}
		rssiStr := "-"
		if rssi.Valid {
			rssiStr = fmt.Sprintf("%ddBm", rssi.Int64)
		}
		regStr := "N"
		if isRegistered {
			regStr = "Y"
		}

		fmt.Fprintf(w, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
			uid[:16], typeStr, name, aliasStr, zoneStr,
			lastSeen.Format("2006-01-02 15:04"), battStr, rssiStr, regStr)
	}
	w.Flush()
	return nil
}

func showSensorData(cmd *cobra.Command, args []string) error {
	db, err := openDB()
	if err != nil {
		return err
	}
	defer db.Close()

	var query string
	var queryArgs []interface{}

	if len(args) > 0 {
		query = `
			SELECT device_uid, probe_id, moisture_percent, temperature, battery_mv, rssi, timestamp, synced_to_cloud
			FROM soil_moisture_readings WHERE device_uid = ? ORDER BY timestamp DESC LIMIT ?
		`
		queryArgs = []interface{}{args[0], limit}
	} else {
		query = `
			SELECT device_uid, probe_id, moisture_percent, temperature, battery_mv, rssi, timestamp, synced_to_cloud
			FROM soil_moisture_readings ORDER BY timestamp DESC LIMIT ?
		`
		queryArgs = []interface{}{limit}
	}

	rows, err := db.Query(query, queryArgs...)
	if err != nil {
		return err
	}
	defer rows.Close()

	w := tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
	fmt.Fprintln(w, "DEVICE\tPROBE\tMOISTURE\tTEMP\tBATTERY\tRSSI\tTIME\tSYNC")
	fmt.Fprintln(w, "------\t-----\t--------\t----\t-------\t----\t----\t----")

	for rows.Next() {
		var deviceUID string
		var probeID, moisturePercent int
		var temperature, batteryMV, rssi int
		var timestamp time.Time
		var synced bool

		if err := rows.Scan(&deviceUID, &probeID, &moisturePercent, &temperature, &batteryMV, &rssi, &timestamp, &synced); err != nil {
			return err
		}

		syncStr := "N"
		if synced {
			syncStr = "Y"
		}

		fmt.Fprintf(w, "%s\t%d\t%d%%\t%.1f°C\t%dmV\t%ddBm\t%s\t%s\n",
			deviceUID[:16], probeID, moisturePercent, float64(temperature)/10.0,
			batteryMV, rssi, timestamp.Format("01-02 15:04"), syncStr)
	}
	w.Flush()
	return nil
}

func showMeterData(cmd *cobra.Command, args []string) error {
	db, err := openDB()
	if err != nil {
		return err
	}
	defer db.Close()

	var query string
	var queryArgs []interface{}

	if len(args) > 0 {
		query = `
			SELECT device_uid, total_liters, flow_rate_lpm, battery_mv, rssi, timestamp, synced_to_cloud
			FROM water_meter_readings WHERE device_uid = ? ORDER BY timestamp DESC LIMIT ?
		`
		queryArgs = []interface{}{args[0], limit}
	} else {
		query = `
			SELECT device_uid, total_liters, flow_rate_lpm, battery_mv, rssi, timestamp, synced_to_cloud
			FROM water_meter_readings ORDER BY timestamp DESC LIMIT ?
		`
		queryArgs = []interface{}{limit}
	}

	rows, err := db.Query(query, queryArgs...)
	if err != nil {
		return err
	}
	defer rows.Close()

	w := tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
	fmt.Fprintln(w, "DEVICE\tTOTAL (L)\tFLOW (L/min)\tBATTERY\tRSSI\tTIME\tSYNC")
	fmt.Fprintln(w, "------\t---------\t-----------\t-------\t----\t----\t----")

	for rows.Next() {
		var deviceUID string
		var totalLiters int
		var flowRate float64
		var batteryMV, rssi int
		var timestamp time.Time
		var synced bool

		if err := rows.Scan(&deviceUID, &totalLiters, &flowRate, &batteryMV, &rssi, &timestamp, &synced); err != nil {
			return err
		}

		syncStr := "N"
		if synced {
			syncStr = "Y"
		}

		fmt.Fprintf(w, "%s\t%d\t%.1f\t%dmV\t%ddBm\t%s\t%s\n",
			deviceUID[:16], totalLiters, flowRate, batteryMV, rssi,
			timestamp.Format("01-02 15:04"), syncStr)
	}
	w.Flush()
	return nil
}

func showValves(cmd *cobra.Command, args []string) error {
	db, err := openDB()
	if err != nil {
		return err
	}
	defer db.Close()

	rows, err := db.Query(`
		SELECT uid, controller_uid, address, name, alias, current_state, last_state_change, is_registered
		FROM valve_actuators ORDER BY controller_uid, address
	`)
	if err != nil {
		return err
	}
	defer rows.Close()

	w := tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
	fmt.Fprintln(w, "UID\tCONTROLLER\tADDR\tNAME\tSTATE\tLAST CHANGE\tREG")
	fmt.Fprintln(w, "---\t----------\t----\t----\t-----\t-----------\t---")

	for rows.Next() {
		var uid, controllerUID, name string
		var alias sql.NullString
		var address, currentState int
		var lastChange sql.NullTime
		var isRegistered bool

		if err := rows.Scan(&uid, &controllerUID, &address, &name, &alias, &currentState, &lastChange, &isRegistered); err != nil {
			return err
		}

		stateStr := valveStateString(currentState)
		changeStr := "-"
		if lastChange.Valid {
			changeStr = lastChange.Time.Format("01-02 15:04")
		}
		regStr := "N"
		if isRegistered {
			regStr = "Y"
		}

		fmt.Fprintf(w, "%s\t%s\t%d\t%s\t%s\t%s\t%s\n",
			uid, controllerUID[:16], address, name, stateStr, changeStr, regStr)
	}
	w.Flush()
	return nil
}

func showEvents(cmd *cobra.Command, args []string) error {
	db, err := openDB()
	if err != nil {
		return err
	}
	defer db.Close()

	var query string
	var queryArgs []interface{}

	if len(args) > 0 {
		query = `
			SELECT controller_uid, actuator_addr, prev_state, new_state, source, timestamp, synced_to_cloud
			FROM valve_events WHERE controller_uid = ? ORDER BY timestamp DESC LIMIT ?
		`
		queryArgs = []interface{}{args[0], limit}
	} else {
		query = `
			SELECT controller_uid, actuator_addr, prev_state, new_state, source, timestamp, synced_to_cloud
			FROM valve_events ORDER BY timestamp DESC LIMIT ?
		`
		queryArgs = []interface{}{limit}
	}

	rows, err := db.Query(query, queryArgs...)
	if err != nil {
		return err
	}
	defer rows.Close()

	w := tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
	fmt.Fprintln(w, "CONTROLLER\tADDR\tFROM\tTO\tSOURCE\tTIME\tSYNC")
	fmt.Fprintln(w, "----------\t----\t----\t--\t------\t----\t----")

	for rows.Next() {
		var controllerUID, source string
		var actuatorAddr int
		var prevState, newState sql.NullInt64
		var timestamp time.Time
		var synced bool

		if err := rows.Scan(&controllerUID, &actuatorAddr, &prevState, &newState, &source, &timestamp, &synced); err != nil {
			return err
		}

		prevStr := "-"
		if prevState.Valid {
			prevStr = valveStateString(int(prevState.Int64))
		}
		newStr := valveStateString(int(newState.Int64))
		syncStr := "N"
		if synced {
			syncStr = "Y"
		}

		fmt.Fprintf(w, "%s\t%d\t%s\t%s\t%s\t%s\t%s\n",
			controllerUID[:16], actuatorAddr, prevStr, newStr, source,
			timestamp.Format("01-02 15:04"), syncStr)
	}
	w.Flush()
	return nil
}

func showSchedules(cmd *cobra.Command, args []string) error {
	db, err := openDB()
	if err != nil {
		return err
	}
	defer db.Close()

	rows, err := db.Query(`
		SELECT s.uid, s.controller_uid, s.version, s.name, s.is_active, 
			   COUNT(e.id) as entry_count, s.updated_at
		FROM schedules s
		LEFT JOIN schedule_entries e ON s.id = e.schedule_id
		GROUP BY s.id
		ORDER BY s.controller_uid
	`)
	if err != nil {
		return err
	}
	defer rows.Close()

	w := tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
	fmt.Fprintln(w, "UID\tCONTROLLER\tVER\tNAME\tENTRIES\tACTIVE\tUPDATED")
	fmt.Fprintln(w, "---\t----------\t---\t----\t-------\t------\t-------")

	for rows.Next() {
		var uid, controllerUID, name string
		var version, entryCount int
		var isActive bool
		var updatedAt time.Time

		if err := rows.Scan(&uid, &controllerUID, &version, &name, &isActive, &entryCount, &updatedAt); err != nil {
			return err
		}

		activeStr := "N"
		if isActive {
			activeStr = "Y"
		}

		fmt.Fprintf(w, "%s\t%s\t%d\t%s\t%d\t%s\t%s\n",
			uid[:16], controllerUID[:16], version, name, entryCount, activeStr,
			updatedAt.Format("01-02 15:04"))
	}
	w.Flush()
	return nil
}

func showPending(cmd *cobra.Command, args []string) error {
	db, err := openDB()
	if err != nil {
		return err
	}
	defer db.Close()

	rows, err := db.Query(`
		SELECT command_id, controller_uid, actuator_addr, command, created_at, expires_at, retries, max_retries, acknowledged
		FROM pending_commands WHERE acknowledged = 0 ORDER BY created_at DESC
	`)
	if err != nil {
		return err
	}
	defer rows.Close()

	w := tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
	fmt.Fprintln(w, "CMD ID\tCONTROLLER\tADDR\tCOMMAND\tCREATED\tEXPIRES\tRETRIES")
	fmt.Fprintln(w, "------\t----------\t----\t-------\t-------\t-------\t-------")

	for rows.Next() {
		var commandID int
		var controllerUID string
		var actuatorAddr, command, retries, maxRetries int
		var createdAt, expiresAt time.Time
		var acknowledged bool

		if err := rows.Scan(&commandID, &controllerUID, &actuatorAddr, &command, &createdAt, &expiresAt, &retries, &maxRetries, &acknowledged); err != nil {
			return err
		}

		cmdStr := valveCommandString(command)

		fmt.Fprintf(w, "%d\t%s\t%d\t%s\t%s\t%s\t%d/%d\n",
			commandID, controllerUID[:16], actuatorAddr, cmdStr,
			createdAt.Format("15:04:05"), expiresAt.Format("15:04:05"),
			retries, maxRetries)
	}
	w.Flush()
	return nil
}

func showStats(cmd *cobra.Command, args []string) error {
	db, err := openDB()
	if err != nil {
		return err
	}
	defer db.Close()

	fmt.Println("Database Statistics")
	fmt.Println("===================")

	// Devices
	var deviceCount int
	db.QueryRow("SELECT COUNT(*) FROM devices").Scan(&deviceCount)
	fmt.Printf("Devices: %d\n", deviceCount)

	// Sensor readings
	var sensorCount, unsyncedSensor int
	db.QueryRow("SELECT COUNT(*) FROM soil_moisture_readings").Scan(&sensorCount)
	db.QueryRow("SELECT COUNT(*) FROM soil_moisture_readings WHERE synced_to_cloud = 0").Scan(&unsyncedSensor)
	fmt.Printf("Sensor readings: %d (unsynced: %d)\n", sensorCount, unsyncedSensor)

	// Water meter readings
	var meterCount, unsyncedMeter int
	db.QueryRow("SELECT COUNT(*) FROM water_meter_readings").Scan(&meterCount)
	db.QueryRow("SELECT COUNT(*) FROM water_meter_readings WHERE synced_to_cloud = 0").Scan(&unsyncedMeter)
	fmt.Printf("Meter readings: %d (unsynced: %d)\n", meterCount, unsyncedMeter)

	// Valve events
	var eventCount, unsyncedEvents int
	db.QueryRow("SELECT COUNT(*) FROM valve_events").Scan(&eventCount)
	db.QueryRow("SELECT COUNT(*) FROM valve_events WHERE synced_to_cloud = 0").Scan(&unsyncedEvents)
	fmt.Printf("Valve events: %d (unsynced: %d)\n", eventCount, unsyncedEvents)

	// Pending commands
	var pendingCount int
	db.QueryRow("SELECT COUNT(*) FROM pending_commands WHERE acknowledged = 0").Scan(&pendingCount)
	fmt.Printf("Pending commands: %d\n", pendingCount)

	// Schedules
	var scheduleCount int
	db.QueryRow("SELECT COUNT(*) FROM schedules").Scan(&scheduleCount)
	fmt.Printf("Schedules: %d\n", scheduleCount)

	return nil
}

func executeQuery(cmd *cobra.Command, args []string) error {
	db, err := openDB()
	if err != nil {
		return err
	}
	defer db.Close()

	query := args[0]

	// Only allow SELECT queries for safety
	if !strings.HasPrefix(strings.ToUpper(strings.TrimSpace(query)), "SELECT") {
		return fmt.Errorf("only SELECT queries are allowed")
	}

	rows, err := db.Query(query)
	if err != nil {
		return err
	}
	defer rows.Close()

	cols, err := rows.Columns()
	if err != nil {
		return err
	}

	w := tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
	fmt.Fprintln(w, strings.Join(cols, "\t"))
	fmt.Fprintln(w, strings.Repeat("-\t", len(cols)))

	values := make([]interface{}, len(cols))
	valuePtrs := make([]interface{}, len(cols))
	for i := range values {
		valuePtrs[i] = &values[i]
	}

	for rows.Next() {
		if err := rows.Scan(valuePtrs...); err != nil {
			return err
		}

		var row []string
		for _, v := range values {
			switch val := v.(type) {
			case nil:
				row = append(row, "NULL")
			case []byte:
				row = append(row, string(val))
			default:
				row = append(row, fmt.Sprintf("%v", val))
			}
		}
		fmt.Fprintln(w, strings.Join(row, "\t"))
	}
	w.Flush()
	return nil
}

func deviceTypeString(t int) string {
	switch t {
	case 1:
		return "SOIL"
	case 2:
		return "METER"
	case 3:
		return "VALVE_CTRL"
	case 4:
		return "VALVE_ACT"
	default:
		return fmt.Sprintf("UNK(%d)", t)
	}
}

func valveStateString(state int) string {
	switch state {
	case 0:
		return "CLOSED"
	case 1:
		return "OPEN"
	case 2:
		return "OPENING"
	case 3:
		return "CLOSING"
	case 255:
		return "ERROR"
	default:
		return fmt.Sprintf("UNK(%d)", state)
	}
}

func valveCommandString(cmd int) string {
	switch cmd {
	case 0:
		return "CLOSE"
	case 1:
		return "OPEN"
	case 2:
		return "STOP"
	case 3:
		return "QUERY"
	default:
		return fmt.Sprintf("UNK(%d)", cmd)
	}
}
