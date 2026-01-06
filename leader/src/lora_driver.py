"""
LoRa Driver for RFM95C Module on Raspberry Pi

Handles low-level SPI communication with the LoRa module.
"""

import spidev
import RPi.GPIO as GPIO
import time
import threading
from typing import Optional, Callable, List

# RFM95C Register Addresses
REG_FIFO = 0x00
REG_OP_MODE = 0x01
REG_FRF_MSB = 0x06
REG_FRF_MID = 0x07
REG_FRF_LSB = 0x08
REG_PA_CONFIG = 0x09
REG_LNA = 0x0C
REG_FIFO_ADDR_PTR = 0x0D
REG_FIFO_TX_BASE_ADDR = 0x0E
REG_FIFO_RX_BASE_ADDR = 0x0F
REG_FIFO_RX_CURRENT_ADDR = 0x10
REG_IRQ_FLAGS_MASK = 0x11
REG_IRQ_FLAGS = 0x12
REG_RX_NB_BYTES = 0x13
REG_PKT_SNR_VALUE = 0x19
REG_PKT_RSSI_VALUE = 0x1A
REG_MODEM_CONFIG_1 = 0x1D
REG_MODEM_CONFIG_2 = 0x1E
REG_PREAMBLE_MSB = 0x20
REG_PREAMBLE_LSB = 0x21
REG_PAYLOAD_LENGTH = 0x22
REG_MODEM_CONFIG_3 = 0x26
REG_DETECTION_OPTIMIZE = 0x31
REG_DETECTION_THRESHOLD = 0x37
REG_SYNC_WORD = 0x39
REG_DIO_MAPPING_1 = 0x40
REG_VERSION = 0x42
REG_PA_DAC = 0x4D

# Operating Modes
MODE_SLEEP = 0x00
MODE_STDBY = 0x01
MODE_TX = 0x03
MODE_RX_CONTINUOUS = 0x05
MODE_RX_SINGLE = 0x06

# IRQ Flags
IRQ_TX_DONE = 0x08
IRQ_RX_DONE = 0x40
IRQ_PAYLOAD_CRC_ERROR = 0x20

# PA Config
PA_BOOST = 0x80


class LoRaDriver:
    """Low-level LoRa driver for RFM95C on Raspberry Pi."""
    
    def __init__(
        self,
        spi_bus: int = 0,
        spi_device: int = 0,
        cs_pin: int = 8,      # CE0
        reset_pin: int = 25,
        dio0_pin: int = 24,
        frequency: int = 915_000_000,
        tx_power: int = 20,
        spreading_factor: int = 10,
        bandwidth: int = 125_000,
        coding_rate: int = 5,
        sync_word: int = 0x34
    ):
        self.spi_bus = spi_bus
        self.spi_device = spi_device
        self.cs_pin = cs_pin
        self.reset_pin = reset_pin
        self.dio0_pin = dio0_pin
        self.frequency = frequency
        self.tx_power = tx_power
        self.spreading_factor = spreading_factor
        self.bandwidth = bandwidth
        self.coding_rate = coding_rate
        self.sync_word = sync_word
        
        self.spi: Optional[spidev.SpiDev] = None
        self._rx_callback: Optional[Callable[[bytes, int], None]] = None
        self._rx_thread: Optional[threading.Thread] = None
        self._running = False
        self._lock = threading.Lock()
        
    def begin(self) -> bool:
        """Initialize the LoRa module."""
        # Setup GPIO
        GPIO.setmode(GPIO.BCM)
        GPIO.setwarnings(False)
        GPIO.setup(self.reset_pin, GPIO.OUT)
        GPIO.setup(self.dio0_pin, GPIO.IN)
        
        # Reset module
        GPIO.output(self.reset_pin, GPIO.LOW)
        time.sleep(0.01)
        GPIO.output(self.reset_pin, GPIO.HIGH)
        time.sleep(0.01)
        
        # Setup SPI
        self.spi = spidev.SpiDev()
        self.spi.open(self.spi_bus, self.spi_device)
        self.spi.max_speed_hz = 1_000_000
        self.spi.mode = 0
        
        # Check version
        version = self._read_register(REG_VERSION)
        if version != 0x12:
            print(f"LoRa: Unexpected version 0x{version:02X}")
            return False
        
        # Put in sleep mode for configuration
        self._set_mode(MODE_SLEEP)
        time.sleep(0.01)
        
        # Set LoRa mode (bit 7)
        self._write_register(REG_OP_MODE, 0x80 | MODE_SLEEP)
        time.sleep(0.01)
        
        # Set frequency
        self._set_frequency(self.frequency)
        
        # Set FIFO base addresses
        self._write_register(REG_FIFO_TX_BASE_ADDR, 0x00)
        self._write_register(REG_FIFO_RX_BASE_ADDR, 0x00)
        
        # Set LNA boost
        self._write_register(REG_LNA, self._read_register(REG_LNA) | 0x03)
        
        # Set auto AGC
        self._write_register(REG_MODEM_CONFIG_3, 0x04)
        
        # Set TX power
        self._set_tx_power(self.tx_power)
        
        # Set spreading factor
        self._set_spreading_factor(self.spreading_factor)
        
        # Set bandwidth
        self._set_bandwidth(self.bandwidth)
        
        # Set coding rate
        self._set_coding_rate(self.coding_rate)
        
        # Set sync word
        self._write_register(REG_SYNC_WORD, self.sync_word)
        
        # Enable CRC
        config2 = self._read_register(REG_MODEM_CONFIG_2)
        self._write_register(REG_MODEM_CONFIG_2, config2 | 0x04)
        
        # Set preamble length (8 symbols)
        self._write_register(REG_PREAMBLE_MSB, 0x00)
        self._write_register(REG_PREAMBLE_LSB, 0x08)
        
        # Go to standby
        self._set_mode(MODE_STDBY)
        
        print(f"LoRa: Initialized at {self.frequency/1e6:.1f} MHz, SF{self.spreading_factor}")
        return True
    
    def end(self):
        """Shutdown the LoRa module."""
        self._running = False
        if self._rx_thread:
            self._rx_thread.join(timeout=1.0)
        if self.spi:
            self._set_mode(MODE_SLEEP)
            self.spi.close()
        GPIO.cleanup()
    
    def send(self, data: bytes) -> bool:
        """Send a packet."""
        if len(data) > 255:
            return False
        
        with self._lock:
            # Go to standby
            self._set_mode(MODE_STDBY)
            
            # Set FIFO pointer to TX base
            self._write_register(REG_FIFO_ADDR_PTR, 0x00)
            
            # Write data to FIFO
            for byte in data:
                self._write_register(REG_FIFO, byte)
            
            # Set payload length
            self._write_register(REG_PAYLOAD_LENGTH, len(data))
            
            # Clear IRQ flags
            self._write_register(REG_IRQ_FLAGS, 0xFF)
            
            # Start transmission
            self._set_mode(MODE_TX)
            
            # Wait for TX done
            start = time.time()
            while time.time() - start < 5.0:
                flags = self._read_register(REG_IRQ_FLAGS)
                if flags & IRQ_TX_DONE:
                    self._write_register(REG_IRQ_FLAGS, IRQ_TX_DONE)
                    self._set_mode(MODE_STDBY)
                    return True
                time.sleep(0.001)
            
            self._set_mode(MODE_STDBY)
            return False
    
    def receive(self, timeout_ms: int = 1000) -> Optional[tuple]:
        """Receive a packet with timeout. Returns (data, rssi) or None."""
        with self._lock:
            # Clear IRQ flags
            self._write_register(REG_IRQ_FLAGS, 0xFF)
            
            # Set to RX single mode
            self._set_mode(MODE_RX_SINGLE)
            
            start = time.time()
            timeout_sec = timeout_ms / 1000.0
            
            while time.time() - start < timeout_sec:
                flags = self._read_register(REG_IRQ_FLAGS)
                
                if flags & IRQ_RX_DONE:
                    # Check CRC
                    if flags & IRQ_PAYLOAD_CRC_ERROR:
                        self._write_register(REG_IRQ_FLAGS, 0xFF)
                        self._set_mode(MODE_STDBY)
                        return None
                    
                    # Get packet length
                    length = self._read_register(REG_RX_NB_BYTES)
                    
                    # Set FIFO pointer to RX current address
                    self._write_register(
                        REG_FIFO_ADDR_PTR,
                        self._read_register(REG_FIFO_RX_CURRENT_ADDR)
                    )
                    
                    # Read data
                    data = bytes([self._read_register(REG_FIFO) for _ in range(length)])
                    
                    # Get RSSI
                    rssi = self._read_register(REG_PKT_RSSI_VALUE) - 157
                    
                    self._write_register(REG_IRQ_FLAGS, 0xFF)
                    self._set_mode(MODE_STDBY)
                    
                    return (data, rssi)
                
                time.sleep(0.001)
            
            self._set_mode(MODE_STDBY)
            return None
    
    def start_receive(self, callback: Callable[[bytes, int], None]):
        """Start continuous receive mode with callback."""
        self._rx_callback = callback
        self._running = True
        self._rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
        self._rx_thread.start()
    
    def stop_receive(self):
        """Stop continuous receive mode."""
        self._running = False
        if self._rx_thread:
            self._rx_thread.join(timeout=1.0)
            self._rx_thread = None
    
    def _rx_loop(self):
        """Background receive loop."""
        while self._running:
            result = self.receive(timeout_ms=100)
            if result and self._rx_callback:
                data, rssi = result
                self._rx_callback(data, rssi)
    
    def _read_register(self, address: int) -> int:
        """Read a single register."""
        response = self.spi.xfer2([address & 0x7F, 0x00])
        return response[1]
    
    def _write_register(self, address: int, value: int):
        """Write a single register."""
        self.spi.xfer2([address | 0x80, value])
    
    def _set_mode(self, mode: int):
        """Set operating mode."""
        self._write_register(REG_OP_MODE, 0x80 | mode)
    
    def _set_frequency(self, freq: int):
        """Set carrier frequency."""
        frf = int((freq << 19) / 32_000_000)
        self._write_register(REG_FRF_MSB, (frf >> 16) & 0xFF)
        self._write_register(REG_FRF_MID, (frf >> 8) & 0xFF)
        self._write_register(REG_FRF_LSB, frf & 0xFF)
    
    def _set_tx_power(self, power: int):
        """Set TX power (2-20 dBm)."""
        power = max(2, min(20, power))
        if power > 17:
            self._write_register(REG_PA_DAC, 0x87)  # High power mode
            power = min(power, 20)
            self._write_register(REG_PA_CONFIG, PA_BOOST | (power - 5))
        else:
            self._write_register(REG_PA_DAC, 0x84)  # Normal mode
            self._write_register(REG_PA_CONFIG, PA_BOOST | (power - 2))
    
    def _set_spreading_factor(self, sf: int):
        """Set spreading factor (6-12)."""
        sf = max(6, min(12, sf))
        
        if sf == 6:
            self._write_register(REG_DETECTION_OPTIMIZE, 0xC5)
            self._write_register(REG_DETECTION_THRESHOLD, 0x0C)
        else:
            self._write_register(REG_DETECTION_OPTIMIZE, 0xC3)
            self._write_register(REG_DETECTION_THRESHOLD, 0x0A)
        
        config2 = self._read_register(REG_MODEM_CONFIG_2)
        self._write_register(REG_MODEM_CONFIG_2, (config2 & 0x0F) | (sf << 4))
    
    def _set_bandwidth(self, bw: int):
        """Set bandwidth."""
        bw_map = {
            7800: 0, 10400: 1, 15600: 2, 20800: 3,
            31250: 4, 41700: 5, 62500: 6, 125000: 7,
            250000: 8, 500000: 9
        }
        bw_val = bw_map.get(bw, 7)  # Default 125kHz
        
        config1 = self._read_register(REG_MODEM_CONFIG_1)
        self._write_register(REG_MODEM_CONFIG_1, (config1 & 0x0F) | (bw_val << 4))
    
    def _set_coding_rate(self, cr: int):
        """Set coding rate (5-8 for 4/5 to 4/8)."""
        cr = max(5, min(8, cr))
        
        config1 = self._read_register(REG_MODEM_CONFIG_1)
        self._write_register(REG_MODEM_CONFIG_1, (config1 & 0xF1) | ((cr - 4) << 1))
