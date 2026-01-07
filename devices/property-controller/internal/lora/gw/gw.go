// Package gw contains Go structures matching the ChirpStack Concentratord protobuf API.
// These are manually defined to avoid requiring protoc compilation.
// Based on: https://github.com/chirpstack/chirpstack/blob/master/api/proto/gw/gw.proto
package gw

// CodeRate represents LoRa coding rate
type CodeRate int32

const (
	CodeRate_CR_UNDEFINED CodeRate = 0
	CodeRate_CR_4_5       CodeRate = 1
	CodeRate_CR_4_6       CodeRate = 2
	CodeRate_CR_4_7       CodeRate = 3
	CodeRate_CR_4_8       CodeRate = 4
)

func (c CodeRate) String() string {
	switch c {
	case CodeRate_CR_4_5:
		return "4/5"
	case CodeRate_CR_4_6:
		return "4/6"
	case CodeRate_CR_4_7:
		return "4/7"
	case CodeRate_CR_4_8:
		return "4/8"
	default:
		return "undefined"
	}
}

// TxAckStatus represents the status of a downlink transmission
type TxAckStatus int32

const (
	TxAckStatus_IGNORED             TxAckStatus = 0
	TxAckStatus_OK                  TxAckStatus = 1
	TxAckStatus_TOO_LATE            TxAckStatus = 2
	TxAckStatus_TOO_EARLY           TxAckStatus = 3
	TxAckStatus_COLLISION_PACKET    TxAckStatus = 4
	TxAckStatus_COLLISION_BEACON    TxAckStatus = 5
	TxAckStatus_TX_FREQ             TxAckStatus = 6
	TxAckStatus_TX_POWER            TxAckStatus = 7
	TxAckStatus_GPS_UNLOCKED        TxAckStatus = 8
	TxAckStatus_QUEUE_FULL          TxAckStatus = 9
	TxAckStatus_INTERNAL_ERROR      TxAckStatus = 10
	TxAckStatus_DUTY_CYCLE_OVERFLOW TxAckStatus = 11
)

func (s TxAckStatus) String() string {
	switch s {
	case TxAckStatus_OK:
		return "OK"
	case TxAckStatus_TOO_LATE:
		return "TOO_LATE"
	case TxAckStatus_TOO_EARLY:
		return "TOO_EARLY"
	case TxAckStatus_COLLISION_PACKET:
		return "COLLISION_PACKET"
	case TxAckStatus_TX_FREQ:
		return "TX_FREQ"
	case TxAckStatus_TX_POWER:
		return "TX_POWER"
	case TxAckStatus_QUEUE_FULL:
		return "QUEUE_FULL"
	case TxAckStatus_INTERNAL_ERROR:
		return "INTERNAL_ERROR"
	default:
		return "UNKNOWN"
	}
}

// CRCStatus represents CRC check status
type CRCStatus int32

const (
	CRCStatus_NO_CRC  CRCStatus = 0
	CRCStatus_BAD_CRC CRCStatus = 1
	CRCStatus_CRC_OK  CRCStatus = 2
)

// Event wraps events from Concentratord
type Event struct {
	// Only one of these will be set
	UplinkFrame  *UplinkFrame
	GatewayStats *GatewayStats
}

// Command wraps commands to Concentratord
type Command struct {
	// Only one of these will be set
	SendDownlinkFrame       *DownlinkFrame
	SetGatewayConfiguration *GatewayConfiguration
	GetGatewayId            *GetGatewayIdRequest
}

// UplinkFrame represents a received LoRa frame
type UplinkFrame struct {
	PhyPayload []byte
	TxInfo     *UplinkTxInfo
	RxInfo     *UplinkRxInfo
}

// UplinkTxInfo contains TX metadata for uplink
type UplinkTxInfo struct {
	Frequency  uint32
	Modulation *Modulation
}

// UplinkRxInfo contains RX metadata for uplink
type UplinkRxInfo struct {
	GatewayId string
	UplinkId  uint32
	Rssi      int32
	Snr       float32
	Channel   uint32
	RfChain   uint32
	Context   []byte
	CrcStatus CRCStatus
}

// DownlinkFrame represents a frame to transmit
type DownlinkFrame struct {
	DownlinkId uint32
	GatewayId  string
	Items      []*DownlinkFrameItem
}

// DownlinkFrameItem represents a single downlink opportunity
type DownlinkFrameItem struct {
	PhyPayload []byte
	TxInfo     *DownlinkTxInfo
}

// DownlinkTxInfo contains TX parameters for downlink
type DownlinkTxInfo struct {
	Frequency  uint32
	Power      int32
	Modulation *Modulation
	Board      uint32
	Antenna    uint32
	Timing     *Timing
	Context    []byte
}

// Modulation contains modulation parameters
type Modulation struct {
	// Only one of these will be set
	Lora *LoraModulationInfo
	Fsk  *FskModulationInfo
}

// LoraModulationInfo contains LoRa-specific modulation parameters
type LoraModulationInfo struct {
	Bandwidth             uint32
	SpreadingFactor       uint32
	CodeRate              CodeRate
	PolarizationInversion bool
	Preamble              uint32
	NoCrc                 bool
}

// FskModulationInfo contains FSK-specific modulation parameters
type FskModulationInfo struct {
	FrequencyDeviation uint32
	Datarate           uint32
}

// Timing contains timing information for downlink
type Timing struct {
	// Only one of these will be set
	Immediately *ImmediatelyTimingInfo
	Delay       *DelayTimingInfo
	GpsEpoch    *GPSEpochTimingInfo
}

// ImmediatelyTimingInfo indicates immediate transmission
type ImmediatelyTimingInfo struct{}

// DelayTimingInfo indicates delayed transmission
type DelayTimingInfo struct {
	DelayNanos int64
}

// GPSEpochTimingInfo indicates GPS-timed transmission
type GPSEpochTimingInfo struct {
	TimeSinceGpsEpochNanos int64
}

// DownlinkTxAck represents acknowledgment of a downlink
type DownlinkTxAck struct {
	GatewayId  string
	DownlinkId uint32
	Items      []*DownlinkTxAckItem
}

// DownlinkTxAckItem represents status of a single downlink item
type DownlinkTxAckItem struct {
	Status TxAckStatus
}

// GatewayStats contains gateway statistics
type GatewayStats struct {
	GatewayId           string
	RxPacketsReceived   uint32
	RxPacketsReceivedOk uint32
	TxPacketsReceived   uint32
	TxPacketsEmitted    uint32
}

// GatewayConfiguration contains gateway configuration
type GatewayConfiguration struct {
	GatewayId string
	Version   string
}

// GetGatewayIdRequest is a request for the gateway ID
type GetGatewayIdRequest struct{}

// GetGatewayIdResponse contains the gateway ID
type GetGatewayIdResponse struct {
	GatewayId string
}
