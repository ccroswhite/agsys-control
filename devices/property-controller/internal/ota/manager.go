// Package ota provides OTA firmware update management for the property controller.
//
// The OTA manager handles:
// - Downloading firmware from the AgSys cloud backend
// - Caching firmware files locally
// - Tracking which devices need updates
// - Setting OTA_PENDING flag in ACKs to wake devices
// - Sending firmware chunks to devices via LoRa
// - Tracking update progress and reporting to cloud
package ota

import (
	"context"
	"crypto/sha256"
	"encoding/binary"
	"fmt"
	"hash/crc32"
	"io"
	"log"
	"os"
	"path/filepath"
	"sync"
	"time"

	"github.com/ccroswhite/agsys-api/pkg/lora"
)

// Config holds OTA manager configuration
type Config struct {
	FirmwareCacheDir string        // Directory to cache firmware files
	ChunkSize        uint16        // Size of each OTA chunk (default 200)
	ChunkTimeout     time.Duration // Timeout waiting for chunk ACK
	MaxRetries       int           // Max retries per chunk
	AnnounceInterval time.Duration // How often to re-announce available updates
}

// DefaultConfig returns default OTA configuration
func DefaultConfig() Config {
	return Config{
		FirmwareCacheDir: "/var/lib/agsys/firmware",
		ChunkSize:        200,
		ChunkTimeout:     10 * time.Second,
		MaxRetries:       5,
		AnnounceInterval: 30 * time.Second,
	}
}

// DeviceUpdateState tracks OTA state for a single device
type DeviceUpdateState int

const (
	StateIdle         DeviceUpdateState = iota
	StatePending                        // OTA_PENDING flag set, waiting for device to request
	StateRequested                      // Device sent OTA_REQUEST
	StateTransferring                   // Sending chunks
	StateVerifying                      // All chunks sent, waiting for verification
	StateComplete                       // Update successful
	StateFailed                         // Update failed
	StateRolledBack                     // Device rolled back
)

// DeviceUpdate tracks update progress for a single device
type DeviceUpdate struct {
	DeviceUID      string
	DeviceType     uint8
	CurrentVersion Version
	TargetVersion  Version
	State          DeviceUpdateState
	ChunksSent     uint16
	ChunksAcked    uint16
	TotalChunks    uint16
	RetryCount     int
	LastActivity   time.Time
	ErrorCode      uint8
	ErrorMessage   string
	StartedAt      time.Time
	CompletedAt    time.Time
}

// Version represents a firmware version
type Version struct {
	Major uint8
	Minor uint8
	Patch uint8
}

func (v Version) String() string {
	return fmt.Sprintf("%d.%d.%d", v.Major, v.Minor, v.Patch)
}

// FirmwareInfo describes a cached firmware file
type FirmwareInfo struct {
	DeviceType    uint8
	Version       Version
	HWRevisionMin uint8
	Size          uint32
	CRC32         uint32
	SHA256        [32]byte
	FilePath      string
	ChunkCount    uint16
	ChunkSize     uint16
}

// SendFunc is the function signature for sending LoRa messages
type SendFunc func(deviceUID [8]byte, msgType uint8, payload []byte) error

// Manager handles OTA firmware updates
type Manager struct {
	config   Config
	sendFunc SendFunc
	mu       sync.RWMutex

	// Cached firmware by device type
	firmware map[uint8]*FirmwareInfo

	// Active updates by device UID
	updates map[string]*DeviceUpdate

	// Devices pending update (need OTA_PENDING flag in ACK)
	pendingDevices map[string]bool

	// Cloud client for downloading firmware
	cloudDownloader FirmwareDownloader

	stopChan chan struct{}
	wg       sync.WaitGroup
}

// FirmwareDownloader interface for downloading firmware from cloud
type FirmwareDownloader interface {
	// GetLatestFirmware returns info about the latest firmware for a device type
	GetLatestFirmware(ctx context.Context, deviceType uint8) (*FirmwareInfo, error)
	// DownloadFirmware downloads firmware to the specified path
	DownloadFirmware(ctx context.Context, deviceType uint8, version Version, destPath string) error
}

// New creates a new OTA manager
func New(config Config, sendFunc SendFunc, downloader FirmwareDownloader) (*Manager, error) {
	// Ensure cache directory exists
	if err := os.MkdirAll(config.FirmwareCacheDir, 0755); err != nil {
		return nil, fmt.Errorf("failed to create firmware cache dir: %w", err)
	}

	return &Manager{
		config:          config,
		sendFunc:        sendFunc,
		firmware:        make(map[uint8]*FirmwareInfo),
		updates:         make(map[string]*DeviceUpdate),
		pendingDevices:  make(map[string]bool),
		cloudDownloader: downloader,
		stopChan:        make(chan struct{}),
	}, nil
}

// Start starts the OTA manager background tasks
func (m *Manager) Start(ctx context.Context) error {
	// Load cached firmware
	if err := m.loadCachedFirmware(); err != nil {
		log.Printf("OTA: Failed to load cached firmware: %v", err)
	}

	// Start background tasks
	m.wg.Add(1)
	go m.firmwareSyncLoop(ctx)

	m.wg.Add(1)
	go m.updateMonitorLoop(ctx)

	log.Println("OTA manager started")
	return nil
}

// Stop stops the OTA manager
func (m *Manager) Stop() {
	close(m.stopChan)
	m.wg.Wait()
	log.Println("OTA manager stopped")
}

// ShouldSetOTAPending returns true if the device should receive OTA_PENDING flag
func (m *Manager) ShouldSetOTAPending(deviceUID string, deviceType uint8, currentVersion Version) bool {
	m.mu.RLock()
	defer m.mu.RUnlock()

	// Check if device is already in an active update
	if update, exists := m.updates[deviceUID]; exists {
		if update.State != StateIdle && update.State != StateComplete && update.State != StateFailed {
			return false // Already updating
		}
	}

	// Check if we have newer firmware for this device type
	fw, exists := m.firmware[deviceType]
	if !exists {
		return false
	}

	// Compare versions
	if isNewerVersion(fw.Version, currentVersion) {
		// Mark device as pending
		m.mu.RUnlock()
		m.mu.Lock()
		m.pendingDevices[deviceUID] = true
		m.mu.Unlock()
		m.mu.RLock()
		return true
	}

	return false
}

// HandleOTARequest processes an OTA request from a device
func (m *Manager) HandleOTARequest(deviceUID string, deviceType uint8, payload []byte) error {
	req, err := lora.DecodeOTARequest(payload)
	if err != nil {
		return fmt.Errorf("failed to decode OTA request: %w", err)
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	// Get firmware for this device type
	fw, exists := m.firmware[deviceType]
	if !exists {
		return fmt.Errorf("no firmware available for device type %d", deviceType)
	}

	// Create or update device update state
	update := &DeviceUpdate{
		DeviceUID:      deviceUID,
		DeviceType:     deviceType,
		CurrentVersion: Version{req.CurrentMajor, req.CurrentMinor, req.CurrentPatch},
		TargetVersion:  fw.Version,
		State:          StateRequested,
		TotalChunks:    fw.ChunkCount,
		LastActivity:   time.Now(),
		StartedAt:      time.Now(),
	}
	m.updates[deviceUID] = update

	// Remove from pending
	delete(m.pendingDevices, deviceUID)

	log.Printf("OTA: Device %s requested update from v%s to v%s",
		deviceUID, update.CurrentVersion, update.TargetVersion)

	// Send OTA announce with firmware details
	return m.sendAnnounce(deviceUID, fw)
}

// HandleOTAReady processes an OTA ready message from a device
func (m *Manager) HandleOTAReady(deviceUID string, payload []byte) error {
	ready, err := lora.DecodeOTAReady(payload)
	if err != nil {
		return fmt.Errorf("failed to decode OTA ready: %w", err)
	}

	m.mu.Lock()
	update, exists := m.updates[deviceUID]
	if !exists {
		m.mu.Unlock()
		return fmt.Errorf("no active update for device %s", deviceUID)
	}

	update.State = StateTransferring
	update.ChunksSent = ready.StartChunk
	update.ChunksAcked = ready.StartChunk
	update.LastActivity = time.Now()
	m.mu.Unlock()

	log.Printf("OTA: Device %s ready, starting from chunk %d", deviceUID, ready.StartChunk)

	// Start sending chunks
	return m.sendNextChunk(deviceUID)
}

// HandleOTAStatus processes an OTA status message from a device
func (m *Manager) HandleOTAStatus(deviceUID string, payload []byte) error {
	status, err := lora.DecodeOTAStatus(payload)
	if err != nil {
		return fmt.Errorf("failed to decode OTA status: %w", err)
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	update, exists := m.updates[deviceUID]
	if !exists {
		// Device reporting status without active update - might be boot after OTA
		log.Printf("OTA: Status from %s: status=%d, error=%d, version=v%d.%d.%d, boot_reason=%d",
			deviceUID, status.Status, status.ErrorCode,
			status.VersionMajor, status.VersionMinor, status.VersionPatch,
			status.BootReason)
		return nil
	}

	update.ChunksAcked = status.ChunksReceived
	update.LastActivity = time.Now()

	switch status.Status {
	case lora.OTAStatusSuccess:
		update.State = StateComplete
		update.CompletedAt = time.Now()
		log.Printf("OTA: Device %s update complete, now running v%d.%d.%d",
			deviceUID, status.VersionMajor, status.VersionMinor, status.VersionPatch)

	case lora.OTAStatusFailed:
		update.State = StateFailed
		update.ErrorCode = status.ErrorCode
		update.ErrorMessage = otaErrorString(status.ErrorCode)
		log.Printf("OTA: Device %s update failed: %s", deviceUID, update.ErrorMessage)

	case lora.OTAStatusRolledBack:
		update.State = StateRolledBack
		update.ErrorCode = status.ErrorCode
		log.Printf("OTA: Device %s rolled back to v%d.%d.%d",
			deviceUID, status.VersionMajor, status.VersionMinor, status.VersionPatch)

	case lora.OTAStatusInProgress:
		log.Printf("OTA: Device %s progress: %d/%d chunks",
			deviceUID, status.ChunksReceived, update.TotalChunks)
	}

	return nil
}

// sendAnnounce sends OTA announce to a device
func (m *Manager) sendAnnounce(deviceUID string, fw *FirmwareInfo) error {
	announce := &lora.OTAAnnouncePayload{
		VersionMajor:  fw.Version.Major,
		VersionMinor:  fw.Version.Minor,
		VersionPatch:  fw.Version.Patch,
		HWRevisionMin: fw.HWRevisionMin,
		FirmwareSize:  fw.Size,
		ChunkCount:    fw.ChunkCount,
		ChunkSize:     fw.ChunkSize,
		FirmwareCRC:   fw.CRC32,
	}

	uid, err := parseDeviceUID(deviceUID)
	if err != nil {
		return err
	}

	return m.sendFunc(uid, lora.MsgTypeOTAAnnounce, announce.Encode())
}

// sendNextChunk sends the next firmware chunk to a device
func (m *Manager) sendNextChunk(deviceUID string) error {
	m.mu.RLock()
	update, exists := m.updates[deviceUID]
	if !exists {
		m.mu.RUnlock()
		return fmt.Errorf("no active update for device %s", deviceUID)
	}

	fw, exists := m.firmware[update.DeviceType]
	if !exists {
		m.mu.RUnlock()
		return fmt.Errorf("firmware not found for device type %d", update.DeviceType)
	}

	chunkIndex := update.ChunksSent
	m.mu.RUnlock()

	if chunkIndex >= update.TotalChunks {
		// All chunks sent, send finish
		return m.sendFinish(deviceUID, fw)
	}

	// Read chunk from file
	chunkData, err := m.readChunk(fw, chunkIndex)
	if err != nil {
		return fmt.Errorf("failed to read chunk %d: %w", chunkIndex, err)
	}

	chunk := &lora.OTAChunkPayload{
		ChunkIndex: chunkIndex,
		ChunkSize:  uint16(len(chunkData)),
		Data:       chunkData,
	}

	uid, err := parseDeviceUID(deviceUID)
	if err != nil {
		return err
	}

	if err := m.sendFunc(uid, lora.MsgTypeOTAChunk, chunk.Encode()); err != nil {
		return err
	}

	m.mu.Lock()
	update.ChunksSent = chunkIndex + 1
	update.LastActivity = time.Now()
	m.mu.Unlock()

	return nil
}

// sendFinish sends OTA finish message to a device
func (m *Manager) sendFinish(deviceUID string, fw *FirmwareInfo) error {
	finish := &lora.OTAFinishPayload{
		FirmwareCRC: fw.CRC32,
		TotalChunks: fw.ChunkCount,
	}

	uid, err := parseDeviceUID(deviceUID)
	if err != nil {
		return err
	}

	m.mu.Lock()
	if update, exists := m.updates[deviceUID]; exists {
		update.State = StateVerifying
		update.LastActivity = time.Now()
	}
	m.mu.Unlock()

	log.Printf("OTA: Sent finish to %s, CRC=0x%08X", deviceUID, fw.CRC32)
	return m.sendFunc(uid, lora.MsgTypeOTAFinish, finish.Encode())
}

// readChunk reads a chunk from the firmware file
func (m *Manager) readChunk(fw *FirmwareInfo, chunkIndex uint16) ([]byte, error) {
	f, err := os.Open(fw.FilePath)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	offset := int64(chunkIndex) * int64(fw.ChunkSize)
	if _, err := f.Seek(offset, io.SeekStart); err != nil {
		return nil, err
	}

	// Last chunk may be smaller
	size := int(fw.ChunkSize)
	remaining := int(fw.Size) - int(offset)
	if remaining < size {
		size = remaining
	}

	buf := make([]byte, size)
	n, err := f.Read(buf)
	if err != nil && err != io.EOF {
		return nil, err
	}

	return buf[:n], nil
}

// loadCachedFirmware loads firmware info from the cache directory
func (m *Manager) loadCachedFirmware() error {
	entries, err := os.ReadDir(m.config.FirmwareCacheDir)
	if err != nil {
		if os.IsNotExist(err) {
			return nil
		}
		return err
	}

	for _, entry := range entries {
		if entry.IsDir() {
			continue
		}

		// Parse filename: devicetype_major.minor.patch.bin
		var deviceType uint8
		var major, minor, patch uint8
		n, _ := fmt.Sscanf(entry.Name(), "%d_%d.%d.%d.bin", &deviceType, &major, &minor, &patch)
		if n != 4 {
			continue
		}

		filePath := filepath.Join(m.config.FirmwareCacheDir, entry.Name())
		fw, err := m.loadFirmwareFile(filePath, deviceType, Version{major, minor, patch})
		if err != nil {
			log.Printf("OTA: Failed to load firmware %s: %v", entry.Name(), err)
			continue
		}

		m.firmware[deviceType] = fw
		log.Printf("OTA: Loaded firmware for device type %d: v%s (%d bytes)",
			deviceType, fw.Version, fw.Size)
	}

	return nil
}

// loadFirmwareFile loads and validates a firmware file
func (m *Manager) loadFirmwareFile(path string, deviceType uint8, version Version) (*FirmwareInfo, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	// Get file size
	stat, err := f.Stat()
	if err != nil {
		return nil, err
	}

	size := uint32(stat.Size())

	// Calculate CRC32 and SHA256
	crcHash := crc32.NewIEEE()
	shaHash := sha256.New()
	writer := io.MultiWriter(crcHash, shaHash)

	if _, err := io.Copy(writer, f); err != nil {
		return nil, err
	}

	var sha [32]byte
	copy(sha[:], shaHash.Sum(nil))

	chunkCount := (size + uint32(m.config.ChunkSize) - 1) / uint32(m.config.ChunkSize)

	return &FirmwareInfo{
		DeviceType:    deviceType,
		Version:       version,
		HWRevisionMin: 0, // TODO: Read from firmware header
		Size:          size,
		CRC32:         crcHash.Sum32(),
		SHA256:        sha,
		FilePath:      path,
		ChunkCount:    uint16(chunkCount),
		ChunkSize:     m.config.ChunkSize,
	}, nil
}

// firmwareSyncLoop periodically checks for new firmware from cloud
func (m *Manager) firmwareSyncLoop(ctx context.Context) {
	defer m.wg.Done()

	ticker := time.NewTicker(1 * time.Hour)
	defer ticker.Stop()

	// Initial sync
	m.syncFirmwareFromCloud(ctx)

	for {
		select {
		case <-m.stopChan:
			return
		case <-ctx.Done():
			return
		case <-ticker.C:
			m.syncFirmwareFromCloud(ctx)
		}
	}
}

// syncFirmwareFromCloud downloads latest firmware from cloud
func (m *Manager) syncFirmwareFromCloud(ctx context.Context) {
	if m.cloudDownloader == nil {
		return
	}

	deviceTypes := []uint8{
		lora.DeviceTypeSoilMoisture,
		lora.DeviceTypeValveController,
		lora.DeviceTypeWaterMeter,
	}

	for _, dt := range deviceTypes {
		info, err := m.cloudDownloader.GetLatestFirmware(ctx, dt)
		if err != nil {
			log.Printf("OTA: Failed to get latest firmware for type %d: %v", dt, err)
			continue
		}

		if info == nil {
			continue // No firmware available
		}

		// Check if we already have this version
		m.mu.RLock()
		existing, exists := m.firmware[dt]
		m.mu.RUnlock()

		if exists && !isNewerVersion(info.Version, existing.Version) {
			continue // Already have latest
		}

		// Download new firmware
		filename := fmt.Sprintf("%d_%d.%d.%d.bin", dt, info.Version.Major, info.Version.Minor, info.Version.Patch)
		destPath := filepath.Join(m.config.FirmwareCacheDir, filename)

		log.Printf("OTA: Downloading firmware for type %d v%s", dt, info.Version)
		if err := m.cloudDownloader.DownloadFirmware(ctx, dt, info.Version, destPath); err != nil {
			log.Printf("OTA: Failed to download firmware: %v", err)
			continue
		}

		// Load the downloaded firmware
		fw, err := m.loadFirmwareFile(destPath, dt, info.Version)
		if err != nil {
			log.Printf("OTA: Failed to load downloaded firmware: %v", err)
			os.Remove(destPath)
			continue
		}

		m.mu.Lock()
		m.firmware[dt] = fw
		m.mu.Unlock()

		log.Printf("OTA: Updated firmware for type %d to v%s", dt, fw.Version)
	}
}

// updateMonitorLoop monitors active updates for timeouts
func (m *Manager) updateMonitorLoop(ctx context.Context) {
	defer m.wg.Done()

	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-m.stopChan:
			return
		case <-ctx.Done():
			return
		case <-ticker.C:
			m.checkUpdateTimeouts()
		}
	}
}

// checkUpdateTimeouts checks for stalled updates
func (m *Manager) checkUpdateTimeouts() {
	m.mu.Lock()
	defer m.mu.Unlock()

	now := time.Now()

	for deviceUID, update := range m.updates {
		if update.State == StateComplete || update.State == StateFailed || update.State == StateRolledBack {
			continue
		}

		if now.Sub(update.LastActivity) > m.config.ChunkTimeout {
			update.RetryCount++

			if update.RetryCount > m.config.MaxRetries {
				update.State = StateFailed
				update.ErrorMessage = "timeout"
				log.Printf("OTA: Device %s update timed out after %d retries", deviceUID, m.config.MaxRetries)
				continue
			}

			// Retry last chunk
			if update.State == StateTransferring {
				log.Printf("OTA: Retrying chunk %d for %s (attempt %d)", update.ChunksSent-1, deviceUID, update.RetryCount)
				update.ChunksSent-- // Will resend
				go m.sendNextChunk(deviceUID)
			}
		}
	}
}

// GetUpdateStatus returns the status of all active updates
func (m *Manager) GetUpdateStatus() map[string]*DeviceUpdate {
	m.mu.RLock()
	defer m.mu.RUnlock()

	result := make(map[string]*DeviceUpdate, len(m.updates))
	for k, v := range m.updates {
		result[k] = v
	}
	return result
}

// GetPendingDevices returns devices that need OTA_PENDING flag
func (m *Manager) GetPendingDevices() []string {
	m.mu.RLock()
	defer m.mu.RUnlock()

	result := make([]string, 0, len(m.pendingDevices))
	for uid := range m.pendingDevices {
		result = append(result, uid)
	}
	return result
}

// Helper functions

func isNewerVersion(a, b Version) bool {
	if a.Major != b.Major {
		return a.Major > b.Major
	}
	if a.Minor != b.Minor {
		return a.Minor > b.Minor
	}
	return a.Patch > b.Patch
}

func parseDeviceUID(s string) ([8]byte, error) {
	var uid [8]byte
	if len(s) != 16 {
		return uid, fmt.Errorf("invalid device UID length: %d", len(s))
	}
	for i := 0; i < 8; i++ {
		var b uint8
		_, err := fmt.Sscanf(s[i*2:i*2+2], "%02X", &b)
		if err != nil {
			return uid, err
		}
		uid[i] = b
	}
	return uid, nil
}

func otaErrorString(code uint8) string {
	switch code {
	case lora.OTAErrorNone:
		return "none"
	case lora.OTAErrorCRCMismatch:
		return "CRC mismatch"
	case lora.OTAErrorSizeMismatch:
		return "size mismatch"
	case lora.OTAErrorHWIncompatible:
		return "hardware incompatible"
	case lora.OTAErrorFlashWrite:
		return "flash write error"
	case lora.OTAErrorTimeout:
		return "timeout"
	case lora.OTAErrorValidation:
		return "validation failed"
	default:
		return fmt.Sprintf("unknown(%d)", code)
	}
}

// EncodeUint16LE encodes a uint16 in little-endian
func EncodeUint16LE(v uint16) []byte {
	buf := make([]byte, 2)
	binary.LittleEndian.PutUint16(buf, v)
	return buf
}
