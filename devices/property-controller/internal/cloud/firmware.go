// Package cloud provides communication with the AgSys cloud service.
// This file implements firmware downloading for OTA updates.
package cloud

import (
	"context"
	"fmt"
	"io"
	"log"
	"os"

	"github.com/agsys/property-controller/internal/ota"
	controllerv1 "github.com/ccroswhite/agsys-api/gen/go/proto/controller/v1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/metadata"
)

// FirmwareClient handles firmware downloads from the AgSys backend.
// It implements the ota.FirmwareDownloader interface.
type FirmwareClient struct {
	config       GRPCConfig
	conn         *grpc.ClientConn
	client       controllerv1.FirmwareServiceClient
	sessionToken string
}

// NewFirmwareClient creates a new firmware client
func NewFirmwareClient(config GRPCConfig) *FirmwareClient {
	return &FirmwareClient{
		config: config,
	}
}

// SetSessionToken sets the session token for authenticated requests
func (c *FirmwareClient) SetSessionToken(token string) {
	c.sessionToken = token
}

// Connect establishes connection to the firmware service
func (c *FirmwareClient) Connect(ctx context.Context) error {
	if c.conn != nil {
		return nil // Already connected
	}

	opts := []grpc.DialOption{}

	if c.config.UseTLS {
		creds := credentials.NewClientTLSFromCert(nil, "")
		opts = append(opts, grpc.WithTransportCredentials(creds))
	} else {
		opts = append(opts, grpc.WithTransportCredentials(insecure.NewCredentials()))
	}

	conn, err := grpc.DialContext(ctx, c.config.ServerAddr, opts...)
	if err != nil {
		return fmt.Errorf("failed to connect to firmware service: %w", err)
	}

	c.conn = conn
	c.client = controllerv1.NewFirmwareServiceClient(conn)
	return nil
}

// Close closes the connection
func (c *FirmwareClient) Close() error {
	if c.conn != nil {
		err := c.conn.Close()
		c.conn = nil
		c.client = nil
		return err
	}
	return nil
}

// contextWithAuth returns a context with the session token in metadata
func (c *FirmwareClient) contextWithAuth(ctx context.Context) context.Context {
	if c.sessionToken == "" {
		return ctx
	}
	md := metadata.Pairs(authTokenMetadataKey, c.sessionToken)
	return metadata.NewOutgoingContext(ctx, md)
}

// deviceTypeToProto converts internal device type to proto enum
func deviceTypeToProto(deviceType uint8) controllerv1.DeviceTypeEnum {
	switch deviceType {
	case 0x01:
		return controllerv1.DeviceTypeEnum_DEVICE_TYPE_SOIL_MOISTURE
	case 0x02:
		return controllerv1.DeviceTypeEnum_DEVICE_TYPE_VALVE_CONTROLLER
	case 0x03:
		return controllerv1.DeviceTypeEnum_DEVICE_TYPE_WATER_METER
	case 0x04:
		return controllerv1.DeviceTypeEnum_DEVICE_TYPE_VALVE_ACTUATOR
	default:
		return controllerv1.DeviceTypeEnum_DEVICE_TYPE_UNSPECIFIED
	}
}

// GetLatestFirmware returns info about the latest firmware for a device type.
// Implements ota.FirmwareDownloader interface.
func (c *FirmwareClient) GetLatestFirmware(ctx context.Context, deviceType uint8) (*ota.FirmwareInfo, error) {
	if c.client == nil {
		if err := c.Connect(ctx); err != nil {
			return nil, err
		}
	}

	authCtx := c.contextWithAuth(ctx)
	resp, err := c.client.GetLatestFirmware(authCtx, &controllerv1.GetLatestFirmwareRequest{
		ControllerId: c.config.ControllerID,
		DeviceType:   deviceTypeToProto(deviceType),
	})
	if err != nil {
		return nil, fmt.Errorf("GetLatestFirmware RPC failed: %w", err)
	}

	if !resp.Available || resp.Firmware == nil {
		return nil, nil // No firmware available
	}

	fw := resp.Firmware
	return &ota.FirmwareInfo{
		DeviceType: deviceType,
		Version: ota.Version{
			Major: uint8(fw.VersionMajor),
			Minor: uint8(fw.VersionMinor),
			Patch: uint8(fw.VersionPatch),
		},
		HWRevisionMin: uint8(fw.HwRevisionMin),
		Size:          uint32(fw.SizeBytes),
		CRC32:         fw.Crc32,
	}, nil
}

// DownloadFirmware downloads firmware to the specified path.
// Implements ota.FirmwareDownloader interface.
func (c *FirmwareClient) DownloadFirmware(ctx context.Context, deviceType uint8, version ota.Version, destPath string) error {
	if c.client == nil {
		if err := c.Connect(ctx); err != nil {
			return err
		}
	}

	authCtx := c.contextWithAuth(ctx)

	// First get firmware info to get the firmware_id
	infoResp, err := c.client.GetLatestFirmware(authCtx, &controllerv1.GetLatestFirmwareRequest{
		ControllerId: c.config.ControllerID,
		DeviceType:   deviceTypeToProto(deviceType),
	})
	if err != nil {
		return fmt.Errorf("failed to get firmware info: %w", err)
	}
	if !infoResp.Available || infoResp.Firmware == nil {
		return fmt.Errorf("firmware not available")
	}

	firmwareID := infoResp.Firmware.FirmwareId
	versionStr := fmt.Sprintf("%d.%d.%d", version.Major, version.Minor, version.Patch)

	log.Printf("Firmware: Downloading %s v%s (ID: %s)", deviceTypeToProto(deviceType), versionStr, firmwareID)

	// Start streaming download
	stream, err := c.client.DownloadFirmware(authCtx, &controllerv1.DownloadFirmwareRequest{
		ControllerId: c.config.ControllerID,
		FirmwareId:   firmwareID,
		DeviceType:   deviceTypeToProto(deviceType),
		Version:      versionStr,
	})
	if err != nil {
		return fmt.Errorf("failed to start firmware download: %w", err)
	}

	// Create destination file
	f, err := os.Create(destPath)
	if err != nil {
		return fmt.Errorf("failed to create destination file: %w", err)
	}
	defer f.Close()

	var totalBytes int64
	var chunksReceived int32

	for {
		chunk, err := stream.Recv()
		if err == io.EOF {
			break
		}
		if err != nil {
			os.Remove(destPath) // Clean up partial file
			return fmt.Errorf("error receiving chunk: %w", err)
		}

		n, err := f.Write(chunk.Data)
		if err != nil {
			os.Remove(destPath)
			return fmt.Errorf("error writing chunk: %w", err)
		}

		totalBytes += int64(n)
		chunksReceived++

		if chunk.ChunkIndex%10 == 0 || chunk.IsLast {
			log.Printf("Firmware: Downloaded chunk %d/%d", chunk.ChunkIndex+1, chunk.TotalChunks)
		}

		if chunk.IsLast {
			break
		}
	}

	log.Printf("Firmware: Download complete - %d bytes in %d chunks", totalBytes, chunksReceived)
	return nil
}

// ReportOTAStatus reports the result of an OTA update to the backend
func (c *FirmwareClient) ReportOTAStatus(ctx context.Context, report *OTAReport) error {
	if c.client == nil {
		if err := c.Connect(ctx); err != nil {
			return err
		}
	}

	authCtx := c.contextWithAuth(ctx)

	_, err := c.client.ReportOTAStatus(authCtx, &controllerv1.OTAStatusReport{
		ControllerId:      c.config.ControllerID,
		DeviceUid:         report.DeviceUID,
		FirmwareId:        report.FirmwareID,
		Result:            otaResultToProto(report.Result),
		ErrorMessage:      report.ErrorMessage,
		ChunksTransferred: int32(report.ChunksTransferred),
		DurationSeconds:   int32(report.DurationSeconds),
		PreviousVersion:   report.PreviousVersion,
		NewVersion:        report.NewVersion,
		BootReason:        bootReasonToProto(report.BootReason),
	})
	if err != nil {
		return fmt.Errorf("ReportOTAStatus RPC failed: %w", err)
	}

	return nil
}

// OTAReport holds OTA status information to report to the backend
type OTAReport struct {
	DeviceUID         string
	FirmwareID        string
	Result            OTAResultType
	ErrorMessage      string
	ChunksTransferred int
	DurationSeconds   int
	PreviousVersion   string
	NewVersion        string
	BootReason        uint8
}

// OTAResultType represents the result of an OTA update
type OTAResultType int

const (
	OTAResultSuccess    OTAResultType = 1
	OTAResultFailed     OTAResultType = 2
	OTAResultRolledBack OTAResultType = 3
	OTAResultInProgress OTAResultType = 4
	OTAResultCancelled  OTAResultType = 5
)

func otaResultToProto(result OTAResultType) controllerv1.OTAResult {
	switch result {
	case OTAResultSuccess:
		return controllerv1.OTAResult_OTA_RESULT_SUCCESS
	case OTAResultFailed:
		return controllerv1.OTAResult_OTA_RESULT_FAILED
	case OTAResultRolledBack:
		return controllerv1.OTAResult_OTA_RESULT_ROLLED_BACK
	case OTAResultInProgress:
		return controllerv1.OTAResult_OTA_RESULT_IN_PROGRESS
	case OTAResultCancelled:
		return controllerv1.OTAResult_OTA_RESULT_CANCELLED
	default:
		return controllerv1.OTAResult_OTA_RESULT_UNSPECIFIED
	}
}

func bootReasonToProto(reason uint8) controllerv1.BootReason {
	switch reason {
	case 0x00:
		return controllerv1.BootReason_BOOT_REASON_NORMAL
	case 0x01:
		return controllerv1.BootReason_BOOT_REASON_POWER_CYCLE
	case 0x02:
		return controllerv1.BootReason_BOOT_REASON_WATCHDOG
	case 0x03:
		return controllerv1.BootReason_BOOT_REASON_OTA_SUCCESS
	case 0x04:
		return controllerv1.BootReason_BOOT_REASON_OTA_ROLLBACK
	case 0x05:
		return controllerv1.BootReason_BOOT_REASON_HARD_FAULT
	default:
		return controllerv1.BootReason_BOOT_REASON_UNSPECIFIED
	}
}
