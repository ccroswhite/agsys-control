// Package gw provides marshaling/unmarshaling for ChirpStack Concentratord messages.
// Uses a simple binary format compatible with the Concentratord ZMQ API.
package gw

import (
	"encoding/binary"
	"fmt"
)

// MarshalCommand serializes a command for sending to Concentratord
func MarshalCommand(cmd *Command) ([]byte, error) {
	if cmd.GetGatewayId != nil {
		// Empty payload for gateway_id request
		return nil, nil
	}

	if cmd.SendDownlinkFrame != nil {
		return MarshalDownlinkFrame(cmd.SendDownlinkFrame)
	}

	return nil, fmt.Errorf("unknown command type")
}

// MarshalDownlinkFrame serializes a downlink frame
func MarshalDownlinkFrame(dl *DownlinkFrame) ([]byte, error) {
	if len(dl.Items) == 0 {
		return nil, fmt.Errorf("no downlink items")
	}

	item := dl.Items[0]
	payload := item.PhyPayload
	txInfo := item.TxInfo

	// Simple binary format:
	// 4 bytes: downlink_id
	// 4 bytes: frequency
	// 4 bytes: power (signed)
	// 4 bytes: bandwidth
	// 4 bytes: spreading_factor
	// 1 byte: coding_rate
	// 1 byte: timing (0=immediate)
	// 2 bytes: payload length
	// N bytes: payload

	buf := make([]byte, 24+len(payload))
	binary.LittleEndian.PutUint32(buf[0:4], dl.DownlinkId)
	binary.LittleEndian.PutUint32(buf[4:8], txInfo.Frequency)
	binary.LittleEndian.PutUint32(buf[8:12], uint32(txInfo.Power))

	if txInfo.Modulation != nil && txInfo.Modulation.Lora != nil {
		binary.LittleEndian.PutUint32(buf[12:16], txInfo.Modulation.Lora.Bandwidth)
		binary.LittleEndian.PutUint32(buf[16:20], txInfo.Modulation.Lora.SpreadingFactor)
		buf[20] = byte(txInfo.Modulation.Lora.CodeRate)
	}

	buf[21] = 0 // Immediate timing
	binary.LittleEndian.PutUint16(buf[22:24], uint16(len(payload)))
	copy(buf[24:], payload)

	return buf, nil
}

// UnmarshalEvent deserializes an event from Concentratord
func UnmarshalEvent(eventType string, data []byte) (*Event, error) {
	event := &Event{}

	switch eventType {
	case "up":
		uplink, err := UnmarshalUplinkFrame(data)
		if err != nil {
			return nil, err
		}
		event.UplinkFrame = uplink

	case "stats":
		stats, err := UnmarshalGatewayStats(data)
		if err != nil {
			return nil, err
		}
		event.GatewayStats = stats

	default:
		return nil, fmt.Errorf("unknown event type: %s", eventType)
	}

	return event, nil
}

// UnmarshalUplinkFrame deserializes an uplink frame
// Note: This expects the Concentratord's protobuf format
func UnmarshalUplinkFrame(data []byte) (*UplinkFrame, error) {
	// For now, use a simplified format that we'll refine
	// The actual Concentratord uses protobuf, but we can parse the key fields

	if len(data) < 20 {
		return nil, fmt.Errorf("uplink data too short: %d bytes", len(data))
	}

	// This is a placeholder - actual implementation would use protobuf
	// For testing, we'll assume raw PHY payload
	return &UplinkFrame{
		PhyPayload: data,
		RxInfo: &UplinkRxInfo{
			Rssi: -50, // Placeholder
			Snr:  10.0,
		},
	}, nil
}

// UnmarshalGatewayStats deserializes gateway statistics
func UnmarshalGatewayStats(data []byte) (*GatewayStats, error) {
	// Placeholder implementation
	return &GatewayStats{}, nil
}

// UnmarshalDownlinkTxAck deserializes a TX acknowledgment
func UnmarshalDownlinkTxAck(data []byte) (*DownlinkTxAck, error) {
	if len(data) < 8 {
		return nil, fmt.Errorf("tx ack data too short: %d bytes", len(data))
	}

	// Simple format:
	// 4 bytes: downlink_id
	// 4 bytes: status

	ack := &DownlinkTxAck{
		DownlinkId: binary.LittleEndian.Uint32(data[0:4]),
		Items: []*DownlinkTxAckItem{
			{Status: TxAckStatus(binary.LittleEndian.Uint32(data[4:8]))},
		},
	}

	return ack, nil
}

// UnmarshalGetGatewayIdResponse deserializes a gateway ID response
func UnmarshalGetGatewayIdResponse(data []byte) (*GetGatewayIdResponse, error) {
	// Gateway ID is 8 bytes, returned as hex string
	if len(data) < 8 {
		return nil, fmt.Errorf("gateway id response too short: %d bytes", len(data))
	}

	gatewayId := fmt.Sprintf("%016x", binary.BigEndian.Uint64(data[0:8]))
	return &GetGatewayIdResponse{GatewayId: gatewayId}, nil
}
