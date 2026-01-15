#!/usr/bin/env python3
"""
patch_app_header.py - Post-build script to patch application header

This script:
1. Reads the compiled binary
2. Locates the application header at offset 0x200
3. Calculates firmware size and CRC
4. Patches the header with correct values
5. Recalculates header CRC

Usage:
    python3 patch_app_header.py <input.bin> [output.bin]
    
If output is not specified, input is modified in place.

The header must already contain the magic number (0x59534741 "AGSY").
Fields fw_size, fw_crc32, and header_crc32 should be 0xFFFFFFFF as placeholders.
"""

import sys
import struct
import argparse
from pathlib import Path

# Header constants
APP_HEADER_MAGIC = 0x59534741  # "AGSY" little-endian
APP_HEADER_MAGIC_BYTES = struct.pack('<I', APP_HEADER_MAGIC)
APP_HEADER_SIZE = 48

# CRC32 lookup table (same as firmware)
CRC_TABLE = [
    0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
    0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
    0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
    0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
]

def crc32(data: bytes) -> int:
    """Calculate CRC32 using same algorithm as bootloader."""
    crc = 0xFFFFFFFF
    for byte in data:
        crc = CRC_TABLE[(crc ^ byte) & 0x0F] ^ (crc >> 4)
        crc = CRC_TABLE[(crc ^ (byte >> 4)) & 0x0F] ^ (crc >> 4)
    return crc ^ 0xFFFFFFFF

def parse_header(data: bytes) -> dict:
    """Parse application header from binary data."""
    if len(data) < APP_HEADER_SIZE:
        raise ValueError(f"Header data too short: {len(data)} < {APP_HEADER_SIZE}")
    
    # Unpack header fields
    fields = struct.unpack('<I B B B B B B B B I I I I 16s I', data[:APP_HEADER_SIZE])
    
    return {
        'magic': fields[0],
        'header_version': fields[1],
        'device_type': fields[2],
        'hw_revision_min': fields[3],
        'hw_revision_max': fields[4],
        'fw_version_major': fields[5],
        'fw_version_minor': fields[6],
        'fw_version_patch': fields[7],
        'fw_flags': fields[8],
        'fw_size': fields[9],
        'fw_crc32': fields[10],
        'fw_load_addr': fields[11],
        'build_timestamp': fields[12],
        'build_id': fields[13].rstrip(b'\x00').decode('ascii', errors='replace'),
        'header_crc32': fields[14],
    }

def pack_header(hdr: dict) -> bytes:
    """Pack header dictionary back to binary."""
    build_id = hdr['build_id'].encode('ascii')[:16].ljust(16, b'\x00')
    
    return struct.pack('<I B B B B B B B B I I I I 16s I',
        hdr['magic'],
        hdr['header_version'],
        hdr['device_type'],
        hdr['hw_revision_min'],
        hdr['hw_revision_max'],
        hdr['fw_version_major'],
        hdr['fw_version_minor'],
        hdr['fw_version_patch'],
        hdr['fw_flags'],
        hdr['fw_size'],
        hdr['fw_crc32'],
        hdr['fw_load_addr'],
        hdr['build_timestamp'],
        build_id,
        hdr['header_crc32'],
    )

def find_header_offset(data: bytes) -> int:
    """Find app header by scanning for magic number."""
    # Search for magic number (align to 4 bytes)
    offset = 0
    while offset <= len(data) - APP_HEADER_SIZE:
        if data[offset:offset+4] == APP_HEADER_MAGIC_BYTES:
            return offset
        offset += 4
    return -1

def patch_binary(input_path: Path, output_path: Path, verbose: bool = True) -> bool:
    """Patch application header in binary file."""
    
    # Read binary
    data = bytearray(input_path.read_bytes())
    
    # Find header by scanning for magic
    header_offset = find_header_offset(bytes(data))
    
    if header_offset < 0:
        print(f"Error: App header magic not found in binary", file=sys.stderr)
        print("Make sure the firmware includes app_header.c with the header in .app_header section",
              file=sys.stderr)
        return False
    
    if len(data) < header_offset + APP_HEADER_SIZE:
        print(f"Error: Binary too small ({len(data)} bytes)", file=sys.stderr)
        return False
    
    # Parse header
    header_data = bytes(data[header_offset:header_offset + APP_HEADER_SIZE])
    hdr = parse_header(header_data)
    
    if verbose:
        print(f"Found app header at offset 0x{header_offset:X}")
        print(f"  Device type: {hdr['device_type']}")
        print(f"  Version: {hdr['fw_version_major']}.{hdr['fw_version_minor']}.{hdr['fw_version_patch']}")
        print(f"  Build ID: {hdr['build_id']}")
    
    # Calculate firmware size (entire binary)
    fw_size = len(data)
    hdr['fw_size'] = fw_size
    
    # Calculate firmware CRC (entire binary, but with header CRCs as 0xFFFFFFFF)
    # First, temporarily set CRC fields to placeholder
    temp_data = bytearray(data)
    temp_hdr = hdr.copy()
    temp_hdr['fw_crc32'] = 0xFFFFFFFF
    temp_hdr['header_crc32'] = 0xFFFFFFFF
    temp_data[header_offset:header_offset + APP_HEADER_SIZE] = pack_header(temp_hdr)
    
    fw_crc = crc32(bytes(temp_data))
    hdr['fw_crc32'] = fw_crc
    
    # Calculate header CRC (excludes header_crc32 field itself)
    header_for_crc = pack_header(hdr)[:-4]  # Exclude last 4 bytes (header_crc32)
    header_crc = crc32(header_for_crc)
    hdr['header_crc32'] = header_crc
    
    if verbose:
        print(f"  Firmware size: {fw_size} bytes")
        print(f"  Firmware CRC: 0x{fw_crc:08X}")
        print(f"  Header CRC: 0x{header_crc:08X}")
    
    # Patch header in binary
    data[header_offset:header_offset + APP_HEADER_SIZE] = pack_header(hdr)
    
    # Write output
    output_path.write_bytes(bytes(data))
    
    if verbose:
        print(f"Patched binary written to {output_path}")
    
    return True

def main():
    parser = argparse.ArgumentParser(
        description='Patch AgSys application header with firmware size and CRC'
    )
    parser.add_argument('input', type=Path, help='Input binary file')
    parser.add_argument('output', type=Path, nargs='?', help='Output binary file (default: modify in place)')
    parser.add_argument('-q', '--quiet', action='store_true', help='Suppress output')
    
    args = parser.parse_args()
    
    if not args.input.exists():
        print(f"Error: Input file not found: {args.input}", file=sys.stderr)
        sys.exit(1)
    
    output = args.output or args.input
    
    if patch_binary(args.input, output, verbose=not args.quiet):
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == '__main__':
    main()
