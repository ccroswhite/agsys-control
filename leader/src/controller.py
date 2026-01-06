"""
AgSys Leader

Main application for the agricultural IoT system leader.
Handles LoRa communication, device management, and OTA updates.
"""

import os
import sys
import time
import json
import logging
import threading
import sqlite3
from datetime import datetime
from typing import Optional, Dict, List
from dataclasses import dataclass

from lora_driver import LoRaDriver
from protocol import (
    Protocol, MessageType, DeviceType, PacketHeader,
    SensorReport, AckFlag, uuid_to_str
)
from ota_manager import OtaManager

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(name)s: %(message)s'
)
logger = logging.getLogger(__name__)


@dataclass
class DeviceInfo:
    """Information about a registered device."""
    uuid: str
    device_type: int
    first_seen: datetime
    last_seen: datetime
    firmware_version: str
    battery_mv: int
    rssi: int


class Controller:
    """Leader for AgSys IoT system. Multiple leaders can exist per organization."""
    
    def __init__(self, db_path: str = "agsys.db"):
        self.db_path = db_path
        self.lora: Optional[LoRaDriver] = None
        self.protocol = Protocol()
        self.ota_manager: Optional[OtaManager] = None
        self.devices: Dict[str, DeviceInfo] = {}
        self._running = False
        self._rx_thread: Optional[threading.Thread] = None
        
        # Initialize database
        self._init_database()
        
        # Load known devices
        self._load_devices()
    
    def start(self) -> bool:
        """Start the controller."""
        logger.info("Starting AgSys Controller...")
        
        # Initialize LoRa
        self.lora = LoRaDriver(
            frequency=915_000_000,
            spreading_factor=10,
            bandwidth=125_000,
            coding_rate=5,
            tx_power=20,
            sync_word=0x34
        )
        
        if not self.lora.begin():
            logger.error("Failed to initialize LoRa module")
            return False
        
        # Initialize OTA manager
        self.ota_manager = OtaManager(self.protocol, self._send_packet)
        self.ota_manager.on_device_complete = self._on_ota_device_complete
        self.ota_manager.on_session_complete = self._on_ota_session_complete
        self.ota_manager.on_progress = self._on_ota_progress
        
        # Start receive loop
        self._running = True
        self._rx_thread = threading.Thread(target=self._receive_loop, daemon=True)
        self._rx_thread.start()
        
        logger.info("Controller started successfully")
        return True
    
    def stop(self):
        """Stop the controller."""
        logger.info("Stopping controller...")
        self._running = False
        
        if self.ota_manager:
            self.ota_manager.stop_update()
        
        if self._rx_thread:
            self._rx_thread.join(timeout=2.0)
        
        if self.lora:
            self.lora.end()
        
        logger.info("Controller stopped")
    
    def start_ota_update(
        self,
        firmware_path: str,
        version: tuple,
        device_type: int = 0xFF
    ) -> int:
        """
        Start an OTA firmware update.
        
        Args:
            firmware_path: Path to firmware binary
            version: Tuple of (major, minor, patch)
            device_type: Target device type (0xFF = all)
        
        Returns:
            announce_id for tracking
        """
        if not self.ota_manager:
            raise RuntimeError("Controller not started")
        
        return self.ota_manager.start_update(firmware_path, version, device_type)
    
    def stop_ota_update(self):
        """Stop the current OTA update."""
        if self.ota_manager:
            self.ota_manager.stop_update()
    
    def get_ota_progress(self) -> dict:
        """Get OTA update progress."""
        if self.ota_manager:
            return self.ota_manager.get_progress()
        return {"active": False}
    
    def get_ota_device_status(self) -> List[dict]:
        """Get OTA status for all devices."""
        if self.ota_manager:
            return self.ota_manager.get_device_status()
        return []
    
    def get_devices(self) -> List[dict]:
        """Get list of all known devices."""
        return [
            {
                "uuid": d.uuid,
                "device_type": DeviceType(d.device_type).name if d.device_type in DeviceType._value2member_map_ else str(d.device_type),
                "first_seen": d.first_seen.isoformat(),
                "last_seen": d.last_seen.isoformat(),
                "firmware_version": d.firmware_version,
                "battery_mv": d.battery_mv,
                "rssi": d.rssi
            }
            for d in self.devices.values()
        ]
    
    def get_sensor_data(self, uuid: str, limit: int = 100) -> List[dict]:
        """Get sensor data for a device."""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT timestamp, moisture_raw, moisture_percent, battery_mv, temperature, rssi
            FROM sensor_data
            WHERE device_uuid = ?
            ORDER BY timestamp DESC
            LIMIT ?
        ''', (uuid, limit))
        
        rows = cursor.fetchall()
        conn.close()
        
        return [
            {
                "timestamp": row[0],
                "moisture_raw": row[1],
                "moisture_percent": row[2],
                "battery_mv": row[3],
                "temperature": row[4] / 10.0,  # Convert from 0.1°C
                "rssi": row[5]
            }
            for row in rows
        ]
    
    def _send_packet(self, data: bytes) -> bool:
        """Send a LoRa packet."""
        if self.lora:
            return self.lora.send(data)
        return False
    
    def _receive_loop(self):
        """Background receive loop."""
        while self._running:
            result = self.lora.receive(timeout_ms=100)
            if result:
                data, rssi = result
                self._handle_packet(data, rssi)
    
    def _handle_packet(self, data: bytes, rssi: int):
        """Handle a received packet."""
        result = self.protocol.parse_packet(data)
        if not result:
            logger.warning("Failed to parse packet")
            return
        
        header, payload = result
        uuid_str = uuid_to_str(header.uuid)
        
        logger.debug(f"RX from {uuid_str[:16]}...: type={header.msg_type}, len={len(payload)}")
        
        # Handle OTA messages
        if header.msg_type >= MessageType.OTA_ANNOUNCE:
            if self.ota_manager:
                self.ota_manager.handle_message(header, payload)
            return
        
        # Handle sensor report
        if header.msg_type == MessageType.SENSOR_REPORT:
            self._handle_sensor_report(header, payload, rssi)
    
    def _handle_sensor_report(self, header: PacketHeader, payload: bytes, rssi: int):
        """Handle a sensor report from a device."""
        report = self.protocol.parse_sensor_report(payload)
        if not report:
            logger.warning("Failed to parse sensor report")
            return
        
        uuid_str = uuid_to_str(header.uuid)
        now = datetime.now()
        
        # Update device info
        if uuid_str not in self.devices:
            self.devices[uuid_str] = DeviceInfo(
                uuid=uuid_str,
                device_type=header.device_type,
                first_seen=now,
                last_seen=now,
                firmware_version="unknown",
                battery_mv=report.battery_mv,
                rssi=rssi
            )
            self._save_device(self.devices[uuid_str])
            logger.info(f"New device registered: {uuid_str[:16]}...")
        else:
            device = self.devices[uuid_str]
            device.last_seen = now
            device.battery_mv = report.battery_mv
            device.rssi = rssi
            self._update_device(device)
        
        # Store sensor data
        self._store_sensor_data(uuid_str, report, rssi)
        
        logger.info(
            f"Sensor report from {uuid_str[:16]}...: "
            f"moisture={report.moisture_percent}%, "
            f"battery={report.battery_mv}mV, "
            f"rssi={rssi}dBm"
        )
        
        # Send ACK
        ack_flags = 0
        if report.flags & 0x08:  # HAS_PENDING
            ack_flags |= AckFlag.SEND_LOGS
        
        ack_packet = self.protocol.build_ack(header.sequence, 0, ack_flags)
        self._send_packet(ack_packet)
    
    def _init_database(self):
        """Initialize SQLite database."""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        # Devices table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS devices (
                uuid TEXT PRIMARY KEY,
                device_type INTEGER,
                first_seen TEXT,
                last_seen TEXT,
                firmware_version TEXT,
                battery_mv INTEGER,
                rssi INTEGER
            )
        ''')
        
        # Sensor data table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS sensor_data (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_uuid TEXT,
                timestamp TEXT,
                moisture_raw INTEGER,
                moisture_percent INTEGER,
                battery_mv INTEGER,
                temperature INTEGER,
                rssi INTEGER,
                FOREIGN KEY (device_uuid) REFERENCES devices(uuid)
            )
        ''')
        
        # OTA history table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS ota_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                announce_id INTEGER,
                firmware_path TEXT,
                version TEXT,
                start_time TEXT,
                end_time TEXT,
                devices_success INTEGER,
                devices_failed INTEGER
            )
        ''')
        
        conn.commit()
        conn.close()
        logger.info(f"Database initialized: {self.db_path}")
    
    def _load_devices(self):
        """Load known devices from database."""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute('SELECT * FROM devices')
        rows = cursor.fetchall()
        
        for row in rows:
            self.devices[row[0]] = DeviceInfo(
                uuid=row[0],
                device_type=row[1],
                first_seen=datetime.fromisoformat(row[2]),
                last_seen=datetime.fromisoformat(row[3]),
                firmware_version=row[4],
                battery_mv=row[5],
                rssi=row[6]
            )
        
        conn.close()
        logger.info(f"Loaded {len(self.devices)} devices from database")
    
    def _save_device(self, device: DeviceInfo):
        """Save a new device to database."""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
            INSERT INTO devices (uuid, device_type, first_seen, last_seen, firmware_version, battery_mv, rssi)
            VALUES (?, ?, ?, ?, ?, ?, ?)
        ''', (
            device.uuid,
            device.device_type,
            device.first_seen.isoformat(),
            device.last_seen.isoformat(),
            device.firmware_version,
            device.battery_mv,
            device.rssi
        ))
        
        conn.commit()
        conn.close()
    
    def _update_device(self, device: DeviceInfo):
        """Update device in database."""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
            UPDATE devices
            SET last_seen = ?, battery_mv = ?, rssi = ?
            WHERE uuid = ?
        ''', (
            device.last_seen.isoformat(),
            device.battery_mv,
            device.rssi,
            device.uuid
        ))
        
        conn.commit()
        conn.close()
    
    def _store_sensor_data(self, uuid: str, report: SensorReport, rssi: int):
        """Store sensor data in database."""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
            INSERT INTO sensor_data (device_uuid, timestamp, moisture_raw, moisture_percent, battery_mv, temperature, rssi)
            VALUES (?, ?, ?, ?, ?, ?, ?)
        ''', (
            uuid,
            datetime.now().isoformat(),
            report.moisture_raw,
            report.moisture_percent,
            report.battery_mv,
            report.temperature,
            rssi
        ))
        
        conn.commit()
        conn.close()
    
    def _on_ota_device_complete(self, uuid: str):
        """Callback when a device completes OTA."""
        logger.info(f"Device {uuid[:16]}... completed OTA update")
    
    def _on_ota_session_complete(self, success: int, failed: int):
        """Callback when OTA session completes."""
        logger.info(f"OTA session complete: {success} success, {failed} failed")
        
        # Store in history
        if self.ota_manager and self.ota_manager.session:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()
            
            session = self.ota_manager.session
            cursor.execute('''
                INSERT INTO ota_history (announce_id, firmware_path, version, start_time, end_time, devices_success, devices_failed)
                VALUES (?, ?, ?, ?, ?, ?, ?)
            ''', (
                session.announce_id,
                session.firmware_path,
                f"{session.version[0]}.{session.version[1]}.{session.version[2]}",
                datetime.fromtimestamp(session.start_time).isoformat(),
                datetime.now().isoformat(),
                success,
                failed
            ))
            
            conn.commit()
            conn.close()
    
    def _on_ota_progress(self, uuid: str, chunk: int, total: int):
        """Callback for OTA progress updates."""
        percent = int(chunk * 100 / total)
        if chunk % 50 == 0 or chunk == total:  # Log every 50 chunks
            logger.info(f"OTA progress {uuid[:16]}...: {chunk}/{total} ({percent}%)")


def main():
    """Main entry point."""
    controller = Controller()
    
    if not controller.start():
        sys.exit(1)
    
    try:
        # Keep running
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        controller.stop()


if __name__ == "__main__":
    main()
