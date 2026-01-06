"""
Communication Protocol for AgSys IoT Devices

Defines packet structures and message types matching the device firmware.
"""

import struct
from dataclasses import dataclass
from typing import Optional, Tuple
from enum import IntEnum
import crcmod

# Protocol constants
MAGIC = b'\xAG\x5Y'  # Will be encoded as bytes
PROTOCOL_VERSION = 1

# CRC functions
crc32_func = crcmod.predefined.mkCrcFun('crc-32')
crc16_func = crcmod.mkCrcFun(0x11021, initCrc=0xFFFF, xorOut=0x0000)


class MessageType(IntEnum):
    """Message type identifiers."""
    SENSOR_REPORT = 0x01
    ACK = 0x02
    CONFIG = 0x03
    LOG_DATA = 0x04
    LOG_ACK = 0x05
    TIME_SYNC = 0x06
    
    # OTA messages (0x10-0x1F)
    OTA_ANNOUNCE = 0x10
    OTA_REQUEST = 0x11
    OTA_CHUNK = 0x12
    OTA_CHUNK_ACK = 0x13
    OTA_CHUNK_NACK = 0x14
    OTA_COMPLETE = 0x15
    OTA_ABORT = 0x16
    OTA_STATUS = 0x17


class DeviceType(IntEnum):
    """Device type identifiers."""
    SOIL_MOISTURE = 0x01
    VALVE_CONTROL = 0x02
    WATER_METER = 0x03


class ReportFlag(IntEnum):
    """Sensor report flags."""
    LOW_BATTERY = 0x01
    FIRST_BOOT = 0x02
    CONFIG_REQUEST = 0x04
    HAS_PENDING = 0x08


class AckFlag(IntEnum):
    """ACK response flags."""
    SEND_LOGS = 0x01
    CONFIG_AVAILABLE = 0x02
    TIME_SYNC = 0x04


@dataclass
class PacketHeader:
    """Packet header structure."""
    magic: bytes
    version: int
    msg_type: int
    device_type: int
    uuid: bytes
    sequence: int
    payload_len: int
    
    FORMAT = '<2sBBB16sHB'
    SIZE = struct.calcsize(FORMAT)
    
    def pack(self) -> bytes:
        return struct.pack(
            self.FORMAT,
            self.magic,
            self.version,
            self.msg_type,
            self.device_type,
            self.uuid,
            self.sequence,
            self.payload_len
        )
    
    @classmethod
    def unpack(cls, data: bytes) -> 'PacketHeader':
        fields = struct.unpack(cls.FORMAT, data[:cls.SIZE])
        return cls(*fields)


@dataclass
class SensorReport:
    """Sensor report payload."""
    timestamp: int
    moisture_raw: int
    moisture_percent: int
    battery_mv: int
    temperature: int
    rssi: int
    pending_logs: int
    flags: int
    
    FORMAT = '<IHBHHHBB'
    SIZE = struct.calcsize(FORMAT)
    
    @classmethod
    def unpack(cls, data: bytes) -> 'SensorReport':
        fields = struct.unpack(cls.FORMAT, data[:cls.SIZE])
        return cls(*fields)


@dataclass
class AckPayload:
    """ACK payload structure."""
    acked_sequence: int
    status: int
    flags: int
    
    FORMAT = '<HBB'
    SIZE = struct.calcsize(FORMAT)
    
    def pack(self) -> bytes:
        return struct.pack(self.FORMAT, self.acked_sequence, self.status, self.flags)


@dataclass
class OtaAnnounce:
    """OTA announcement payload."""
    target_device_type: int
    version_major: int
    version_minor: int
    version_patch: int
    firmware_size: int
    total_chunks: int
    firmware_crc: int
    announce_id: int
    
    FORMAT = '<BBBBIHI I'
    SIZE = struct.calcsize(FORMAT)
    
    def pack(self) -> bytes:
        return struct.pack(
            '<BBBBIHII',
            self.target_device_type,
            self.version_major,
            self.version_minor,
            self.version_patch,
            self.firmware_size,
            self.total_chunks,
            self.firmware_crc,
            self.announce_id
        )


@dataclass
class OtaRequest:
    """OTA request payload."""
    announce_id: int
    current_version_major: int
    current_version_minor: int
    current_version_patch: int
    last_chunk_received: int
    
    FORMAT = '<IBBBH'
    SIZE = struct.calcsize(FORMAT)
    
    @classmethod
    def unpack(cls, data: bytes) -> 'OtaRequest':
        fields = struct.unpack(cls.FORMAT, data[:cls.SIZE])
        return cls(*fields)


@dataclass
class OtaChunk:
    """OTA chunk payload."""
    announce_id: int
    chunk_index: int
    chunk_size: int
    chunk_crc: int
    data: bytes
    
    HEADER_FORMAT = '<IHHH'
    HEADER_SIZE = struct.calcsize(HEADER_FORMAT)
    
    def pack(self) -> bytes:
        header = struct.pack(
            self.HEADER_FORMAT,
            self.announce_id,
            self.chunk_index,
            self.chunk_size,
            self.chunk_crc
        )
        return header + self.data


@dataclass
class OtaChunkAck:
    """OTA chunk ACK payload."""
    announce_id: int
    chunk_index: int
    status: int
    
    FORMAT = '<IHB'
    SIZE = struct.calcsize(FORMAT)
    
    @classmethod
    def unpack(cls, data: bytes) -> 'OtaChunkAck':
        fields = struct.unpack(cls.FORMAT, data[:cls.SIZE])
        return cls(*fields)


@dataclass
class OtaComplete:
    """OTA complete payload."""
    announce_id: int
    calculated_crc: int
    status: int
    
    FORMAT = '<IIB'
    SIZE = struct.calcsize(FORMAT)
    
    @classmethod
    def unpack(cls, data: bytes) -> 'OtaComplete':
        fields = struct.unpack(cls.FORMAT, data[:cls.SIZE])
        return cls(*fields)


@dataclass
class OtaStatus:
    """OTA status payload."""
    announce_id: int
    chunks_received: int
    total_chunks: int
    state: int
    error_code: int
    
    FORMAT = '<IHHBB'
    SIZE = struct.calcsize(FORMAT)
    
    @classmethod
    def unpack(cls, data: bytes) -> 'OtaStatus':
        fields = struct.unpack(cls.FORMAT, data[:cls.SIZE])
        return cls(*fields)


class Protocol:
    """Protocol encoder/decoder."""
    
    def __init__(self, controller_uuid: bytes = b'\x00' * 16):
        self.uuid = controller_uuid
        self.sequence = 0
    
    def next_sequence(self) -> int:
        self.sequence = (self.sequence + 1) & 0xFFFF
        return self.sequence
    
    def build_packet(self, msg_type: MessageType, payload: bytes) -> bytes:
        """Build a complete packet with header."""
        header = PacketHeader(
            magic=b'AG',
            version=PROTOCOL_VERSION,
            msg_type=msg_type,
            device_type=0x00,  # Controller
            uuid=self.uuid,
            sequence=self.next_sequence(),
            payload_len=len(payload)
        )
        return header.pack() + payload
    
    def build_ack(self, sequence: int, status: int = 0, flags: int = 0) -> bytes:
        """Build an ACK packet."""
        payload = AckPayload(sequence, status, flags).pack()
        return self.build_packet(MessageType.ACK, payload)
    
    def build_ota_announce(
        self,
        target_device_type: int,
        version: Tuple[int, int, int],
        firmware_size: int,
        firmware_crc: int,
        announce_id: int
    ) -> bytes:
        """Build an OTA announce packet."""
        total_chunks = (firmware_size + 199) // 200  # 200 bytes per chunk
        payload = OtaAnnounce(
            target_device_type=target_device_type,
            version_major=version[0],
            version_minor=version[1],
            version_patch=version[2],
            firmware_size=firmware_size,
            total_chunks=total_chunks,
            firmware_crc=firmware_crc,
            announce_id=announce_id
        ).pack()
        return self.build_packet(MessageType.OTA_ANNOUNCE, payload)
    
    def build_ota_chunk(
        self,
        announce_id: int,
        chunk_index: int,
        data: bytes
    ) -> bytes:
        """Build an OTA chunk packet."""
        chunk_crc = crc16_func(data)
        payload = OtaChunk(
            announce_id=announce_id,
            chunk_index=chunk_index,
            chunk_size=len(data),
            chunk_crc=chunk_crc,
            data=data
        ).pack()
        return self.build_packet(MessageType.OTA_CHUNK, payload)
    
    def build_ota_abort(self, announce_id: int) -> bytes:
        """Build an OTA abort packet."""
        payload = struct.pack('<I', announce_id)
        return self.build_packet(MessageType.OTA_ABORT, payload)
    
    def parse_packet(self, data: bytes) -> Optional[Tuple[PacketHeader, bytes]]:
        """Parse a received packet. Returns (header, payload) or None."""
        if len(data) < PacketHeader.SIZE:
            return None
        
        header = PacketHeader.unpack(data)
        payload = data[PacketHeader.SIZE:PacketHeader.SIZE + header.payload_len]
        
        return (header, payload)
    
    def parse_sensor_report(self, payload: bytes) -> Optional[SensorReport]:
        """Parse a sensor report payload."""
        if len(payload) < SensorReport.SIZE:
            return None
        return SensorReport.unpack(payload)
    
    def parse_ota_request(self, payload: bytes) -> Optional[OtaRequest]:
        """Parse an OTA request payload."""
        if len(payload) < OtaRequest.SIZE:
            return None
        return OtaRequest.unpack(payload)
    
    def parse_ota_chunk_ack(self, payload: bytes) -> Optional[OtaChunkAck]:
        """Parse an OTA chunk ACK payload."""
        if len(payload) < OtaChunkAck.SIZE:
            return None
        return OtaChunkAck.unpack(payload)
    
    def parse_ota_complete(self, payload: bytes) -> Optional[OtaComplete]:
        """Parse an OTA complete payload."""
        if len(payload) < OtaComplete.SIZE:
            return None
        return OtaComplete.unpack(payload)
    
    def parse_ota_status(self, payload: bytes) -> Optional[OtaStatus]:
        """Parse an OTA status payload."""
        if len(payload) < OtaStatus.SIZE:
            return None
        return OtaStatus.unpack(payload)


def uuid_to_str(uuid: bytes) -> str:
    """Convert UUID bytes to hex string."""
    return uuid.hex()


def str_to_uuid(s: str) -> bytes:
    """Convert hex string to UUID bytes."""
    return bytes.fromhex(s.replace('-', ''))
