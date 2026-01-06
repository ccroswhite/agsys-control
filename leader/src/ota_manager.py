"""
OTA Manager for AgSys Controller

Manages firmware updates for the IoT device fleet via LoRa.
Handles announcement broadcasting, chunk sending, and device tracking.
"""

import os
import time
import random
import threading
import logging
from dataclasses import dataclass, field
from typing import Dict, Optional, List, Callable
from enum import IntEnum
import crcmod

from protocol import (
    Protocol, MessageType, DeviceType, PacketHeader,
    OtaRequest, OtaChunkAck, OtaComplete, OtaStatus,
    uuid_to_str, crc32_func
)

logger = logging.getLogger(__name__)

# OTA Configuration
OTA_CHUNK_SIZE = 200
OTA_ANNOUNCE_INTERVAL_SEC = 30
OTA_CHUNK_TIMEOUT_SEC = 10
OTA_MAX_RETRIES = 5


class DeviceOtaState(IntEnum):
    """OTA state for a device."""
    UNKNOWN = 0
    ANNOUNCED = 1
    REQUESTED = 2
    RECEIVING = 3
    COMPLETE = 4
    ERROR = 5


@dataclass
class DeviceOtaInfo:
    """Tracking info for a device during OTA."""
    uuid: bytes
    state: DeviceOtaState = DeviceOtaState.UNKNOWN
    current_version: tuple = (0, 0, 0)
    last_chunk_sent: int = -1
    last_chunk_acked: int = -1
    retry_count: int = 0
    last_activity: float = 0.0
    error_message: str = ""


@dataclass
class OtaSession:
    """Active OTA update session."""
    announce_id: int
    target_device_type: int
    firmware_path: str
    firmware_data: bytes
    firmware_size: int
    firmware_crc: int
    version: tuple
    total_chunks: int
    devices: Dict[str, DeviceOtaInfo] = field(default_factory=dict)
    start_time: float = 0.0
    is_active: bool = False


class OtaManager:
    """Manages OTA firmware updates for device fleet."""
    
    def __init__(self, protocol: Protocol, send_func: Callable[[bytes], bool]):
        """
        Initialize OTA manager.
        
        Args:
            protocol: Protocol instance for packet building
            send_func: Function to send LoRa packets
        """
        self.protocol = protocol
        self.send = send_func
        self.session: Optional[OtaSession] = None
        self._lock = threading.Lock()
        self._announce_thread: Optional[threading.Thread] = None
        self._running = False
        
        # Callbacks
        self.on_device_complete: Optional[Callable[[str], None]] = None
        self.on_session_complete: Optional[Callable[[int, int], None]] = None
        self.on_progress: Optional[Callable[[str, int, int], None]] = None
    
    def start_update(
        self,
        firmware_path: str,
        version: tuple,
        target_device_type: int = 0xFF
    ) -> int:
        """
        Start a new OTA update session.
        
        Args:
            firmware_path: Path to firmware binary file
            version: Tuple of (major, minor, patch)
            target_device_type: Device type to update (0xFF = all)
        
        Returns:
            announce_id for this session
        """
        if self.session and self.session.is_active:
            raise RuntimeError("OTA session already in progress")
        
        # Load firmware
        if not os.path.exists(firmware_path):
            raise FileNotFoundError(f"Firmware not found: {firmware_path}")
        
        with open(firmware_path, 'rb') as f:
            firmware_data = f.read()
        
        firmware_size = len(firmware_data)
        firmware_crc = crc32_func(firmware_data)
        total_chunks = (firmware_size + OTA_CHUNK_SIZE - 1) // OTA_CHUNK_SIZE
        announce_id = random.randint(1, 0xFFFFFFFF)
        
        logger.info(
            f"Starting OTA: {firmware_path}, v{version[0]}.{version[1]}.{version[2]}, "
            f"{firmware_size} bytes, {total_chunks} chunks, CRC=0x{firmware_crc:08X}"
        )
        
        self.session = OtaSession(
            announce_id=announce_id,
            target_device_type=target_device_type,
            firmware_path=firmware_path,
            firmware_data=firmware_data,
            firmware_size=firmware_size,
            firmware_crc=firmware_crc,
            version=version,
            total_chunks=total_chunks,
            start_time=time.time(),
            is_active=True
        )
        
        # Start announcement thread
        self._running = True
        self._announce_thread = threading.Thread(target=self._announce_loop, daemon=True)
        self._announce_thread.start()
        
        return announce_id
    
    def stop_update(self):
        """Stop the current OTA session."""
        self._running = False
        if self._announce_thread:
            self._announce_thread.join(timeout=2.0)
            self._announce_thread = None
        
        if self.session:
            # Send abort to all devices
            packet = self.protocol.build_ota_abort(self.session.announce_id)
            self.send(packet)
            
            logger.info(f"OTA session {self.session.announce_id} stopped")
            self.session.is_active = False
    
    def handle_message(self, header: PacketHeader, payload: bytes) -> bool:
        """
        Handle an incoming OTA-related message.
        
        Returns True if message was handled.
        """
        if not self.session or not self.session.is_active:
            return False
        
        uuid_str = uuid_to_str(header.uuid)
        
        if header.msg_type == MessageType.OTA_REQUEST:
            return self._handle_request(uuid_str, header.uuid, payload)
        elif header.msg_type == MessageType.OTA_CHUNK_ACK:
            return self._handle_chunk_ack(uuid_str, payload)
        elif header.msg_type == MessageType.OTA_CHUNK_NACK:
            return self._handle_chunk_nack(uuid_str, payload)
        elif header.msg_type == MessageType.OTA_COMPLETE:
            return self._handle_complete(uuid_str, payload)
        elif header.msg_type == MessageType.OTA_STATUS:
            return self._handle_status(uuid_str, payload)
        
        return False
    
    def get_progress(self) -> dict:
        """Get current OTA progress."""
        if not self.session:
            return {"active": False}
        
        devices_complete = sum(
            1 for d in self.session.devices.values()
            if d.state == DeviceOtaState.COMPLETE
        )
        devices_error = sum(
            1 for d in self.session.devices.values()
            if d.state == DeviceOtaState.ERROR
        )
        devices_receiving = sum(
            1 for d in self.session.devices.values()
            if d.state == DeviceOtaState.RECEIVING
        )
        
        return {
            "active": self.session.is_active,
            "announce_id": self.session.announce_id,
            "version": f"{self.session.version[0]}.{self.session.version[1]}.{self.session.version[2]}",
            "firmware_size": self.session.firmware_size,
            "total_chunks": self.session.total_chunks,
            "devices_total": len(self.session.devices),
            "devices_complete": devices_complete,
            "devices_error": devices_error,
            "devices_receiving": devices_receiving,
            "elapsed_sec": int(time.time() - self.session.start_time)
        }
    
    def get_device_status(self) -> List[dict]:
        """Get status of all devices in current session."""
        if not self.session:
            return []
        
        result = []
        for uuid_str, info in self.session.devices.items():
            progress = 0
            if self.session.total_chunks > 0 and info.last_chunk_acked >= 0:
                progress = int((info.last_chunk_acked + 1) * 100 / self.session.total_chunks)
            
            result.append({
                "uuid": uuid_str,
                "state": info.state.name,
                "current_version": f"{info.current_version[0]}.{info.current_version[1]}.{info.current_version[2]}",
                "progress": progress,
                "last_chunk": info.last_chunk_acked,
                "retry_count": info.retry_count,
                "error": info.error_message
            })
        
        return result
    
    def _announce_loop(self):
        """Background thread to periodically broadcast announcements."""
        while self._running and self.session and self.session.is_active:
            self._send_announce()
            
            # Also check for devices that need chunks
            self._process_pending_chunks()
            
            # Check for timeouts
            self._check_timeouts()
            
            # Sleep before next announce
            for _ in range(OTA_ANNOUNCE_INTERVAL_SEC * 10):
                if not self._running:
                    break
                time.sleep(0.1)
    
    def _send_announce(self):
        """Send OTA announcement broadcast."""
        if not self.session:
            return
        
        packet = self.protocol.build_ota_announce(
            target_device_type=self.session.target_device_type,
            version=self.session.version,
            firmware_size=self.session.firmware_size,
            firmware_crc=self.session.firmware_crc,
            announce_id=self.session.announce_id
        )
        
        self.send(packet)
        logger.debug(f"Sent OTA announce {self.session.announce_id}")
    
    def _handle_request(self, uuid_str: str, uuid: bytes, payload: bytes) -> bool:
        """Handle OTA request from device."""
        request = self.protocol.parse_ota_request(payload)
        if not request or request.announce_id != self.session.announce_id:
            return False
        
        with self._lock:
            # Create or update device info
            if uuid_str not in self.session.devices:
                self.session.devices[uuid_str] = DeviceOtaInfo(uuid=uuid)
            
            device = self.session.devices[uuid_str]
            device.current_version = (
                request.current_version_major,
                request.current_version_minor,
                request.current_version_patch
            )
            device.state = DeviceOtaState.REQUESTED
            device.last_activity = time.time()
            
            # Determine starting chunk
            if request.last_chunk_received == 0xFFFF:
                start_chunk = 0
            else:
                start_chunk = request.last_chunk_received + 1
            
            device.last_chunk_acked = start_chunk - 1
            
            logger.info(
                f"OTA request from {uuid_str[:16]}..., "
                f"v{device.current_version[0]}.{device.current_version[1]}.{device.current_version[2]}, "
                f"starting at chunk {start_chunk}"
            )
        
        # Send first chunk
        self._send_chunk(uuid_str, start_chunk)
        return True
    
    def _handle_chunk_ack(self, uuid_str: str, payload: bytes) -> bool:
        """Handle chunk ACK from device."""
        ack = self.protocol.parse_ota_chunk_ack(payload)
        if not ack or ack.announce_id != self.session.announce_id:
            return False
        
        with self._lock:
            if uuid_str not in self.session.devices:
                return False
            
            device = self.session.devices[uuid_str]
            
            if ack.status == 0:  # OK
                device.last_chunk_acked = ack.chunk_index
                device.state = DeviceOtaState.RECEIVING
                device.last_activity = time.time()
                device.retry_count = 0
                
                # Report progress
                if self.on_progress:
                    self.on_progress(uuid_str, ack.chunk_index + 1, self.session.total_chunks)
                
                logger.debug(
                    f"Chunk {ack.chunk_index + 1}/{self.session.total_chunks} "
                    f"ACKed by {uuid_str[:16]}..."
                )
                
                # Send next chunk if not done
                next_chunk = ack.chunk_index + 1
                if next_chunk < self.session.total_chunks:
                    self._send_chunk(uuid_str, next_chunk)
            else:
                # Error - will be retried
                logger.warning(f"Chunk {ack.chunk_index} error from {uuid_str[:16]}...: {ack.status}")
        
        return True
    
    def _handle_chunk_nack(self, uuid_str: str, payload: bytes) -> bool:
        """Handle chunk NACK (resend request) from device."""
        ack = self.protocol.parse_ota_chunk_ack(payload)
        if not ack or ack.announce_id != self.session.announce_id:
            return False
        
        with self._lock:
            if uuid_str not in self.session.devices:
                return False
            
            device = self.session.devices[uuid_str]
            device.last_activity = time.time()
            device.retry_count += 1
            
            if device.retry_count > OTA_MAX_RETRIES:
                device.state = DeviceOtaState.ERROR
                device.error_message = "Max retries exceeded"
                logger.error(f"Device {uuid_str[:16]}... exceeded max retries")
                return True
        
        # Resend the requested chunk
        logger.info(f"Resending chunk {ack.chunk_index} to {uuid_str[:16]}...")
        self._send_chunk(uuid_str, ack.chunk_index)
        return True
    
    def _handle_complete(self, uuid_str: str, payload: bytes) -> bool:
        """Handle OTA complete from device."""
        complete = self.protocol.parse_ota_complete(payload)
        if not complete or complete.announce_id != self.session.announce_id:
            return False
        
        with self._lock:
            if uuid_str not in self.session.devices:
                return False
            
            device = self.session.devices[uuid_str]
            device.last_activity = time.time()
            
            if complete.status == 0:  # CRC OK
                device.state = DeviceOtaState.COMPLETE
                logger.info(f"Device {uuid_str[:16]}... completed OTA successfully")
                
                if self.on_device_complete:
                    self.on_device_complete(uuid_str)
            else:
                device.state = DeviceOtaState.ERROR
                device.error_message = "CRC mismatch"
                logger.error(f"Device {uuid_str[:16]}... CRC mismatch")
            
            # Check if all devices are done
            self._check_session_complete()
        
        return True
    
    def _handle_status(self, uuid_str: str, payload: bytes) -> bool:
        """Handle OTA status from device."""
        status = self.protocol.parse_ota_status(payload)
        if not status or status.announce_id != self.session.announce_id:
            return False
        
        with self._lock:
            if uuid_str not in self.session.devices:
                return False
            
            device = self.session.devices[uuid_str]
            device.last_activity = time.time()
            
            logger.info(
                f"Status from {uuid_str[:16]}...: "
                f"{status.chunks_received}/{status.total_chunks} chunks, "
                f"state={status.state}, error={status.error_code}"
            )
        
        return True
    
    def _send_chunk(self, uuid_str: str, chunk_index: int):
        """Send a firmware chunk to a device."""
        if not self.session or chunk_index >= self.session.total_chunks:
            return
        
        # Calculate chunk data
        start = chunk_index * OTA_CHUNK_SIZE
        end = min(start + OTA_CHUNK_SIZE, self.session.firmware_size)
        chunk_data = self.session.firmware_data[start:end]
        
        # Build and send packet
        packet = self.protocol.build_ota_chunk(
            announce_id=self.session.announce_id,
            chunk_index=chunk_index,
            data=chunk_data
        )
        
        self.send(packet)
        
        with self._lock:
            if uuid_str in self.session.devices:
                self.session.devices[uuid_str].last_chunk_sent = chunk_index
    
    def _process_pending_chunks(self):
        """Send chunks to devices that are waiting."""
        if not self.session:
            return
        
        with self._lock:
            for uuid_str, device in self.session.devices.items():
                if device.state == DeviceOtaState.RECEIVING:
                    # Check if we need to send next chunk
                    next_chunk = device.last_chunk_acked + 1
                    if next_chunk < self.session.total_chunks:
                        if device.last_chunk_sent < next_chunk:
                            self._send_chunk(uuid_str, next_chunk)
    
    def _check_timeouts(self):
        """Check for device timeouts."""
        if not self.session:
            return
        
        now = time.time()
        
        with self._lock:
            for uuid_str, device in self.session.devices.items():
                if device.state in (DeviceOtaState.REQUESTED, DeviceOtaState.RECEIVING):
                    if now - device.last_activity > OTA_CHUNK_TIMEOUT_SEC:
                        device.retry_count += 1
                        
                        if device.retry_count > OTA_MAX_RETRIES:
                            device.state = DeviceOtaState.ERROR
                            device.error_message = "Timeout"
                            logger.error(f"Device {uuid_str[:16]}... timed out")
                        else:
                            # Resend last chunk
                            chunk = device.last_chunk_acked + 1
                            if chunk < self.session.total_chunks:
                                logger.info(f"Timeout, resending chunk {chunk} to {uuid_str[:16]}...")
                                device.last_activity = now
                                self._send_chunk(uuid_str, chunk)
    
    def _check_session_complete(self):
        """Check if all devices have completed."""
        if not self.session:
            return
        
        all_done = all(
            d.state in (DeviceOtaState.COMPLETE, DeviceOtaState.ERROR)
            for d in self.session.devices.values()
        )
        
        if all_done and len(self.session.devices) > 0:
            complete_count = sum(
                1 for d in self.session.devices.values()
                if d.state == DeviceOtaState.COMPLETE
            )
            error_count = sum(
                1 for d in self.session.devices.values()
                if d.state == DeviceOtaState.ERROR
            )
            
            logger.info(
                f"OTA session complete: {complete_count} success, {error_count} errors"
            )
            
            if self.on_session_complete:
                self.on_session_complete(complete_count, error_count)
            
            self.session.is_active = False
            self._running = False
