#!/usr/bin/env python3
"""
AgSys Firmware Signing Tool

Signs a firmware binary using Ed25519 and creates a release package.

Usage:
    python3 sign_firmware.py <firmware.bin> <private_key.pem> [output_dir]

Output:
    <output_dir>/
        firmware.bin        - Original firmware binary
        firmware.sig        - Ed25519 signature (64 bytes)
        firmware.sha256     - SHA256 checksum
        manifest.json       - Release metadata

The signature is computed over SHA256(firmware.bin).
"""

import hashlib
import json
import os
import shutil
import sys
from datetime import datetime, timezone

try:
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    from cryptography.hazmat.primitives import serialization
except ImportError:
    print("Error: cryptography package required")
    print("Install with: pip3 install cryptography")
    sys.exit(1)


def load_private_key(key_path: str) -> Ed25519PrivateKey:
    """Load Ed25519 private key from PEM file."""
    with open(key_path, "rb") as f:
        private_key = serialization.load_pem_private_key(f.read(), password=None)
    
    if not isinstance(private_key, Ed25519PrivateKey):
        raise ValueError("Key is not an Ed25519 private key")
    
    return private_key


def compute_sha256(data: bytes) -> str:
    """Compute SHA256 hash and return hex string."""
    return hashlib.sha256(data).hexdigest()


def compute_crc32(data: bytes) -> int:
    """Compute CRC32 checksum."""
    import binascii
    return binascii.crc32(data) & 0xffffffff


def sign_firmware(firmware_path: str, key_path: str, output_dir: str):
    """Sign firmware and create release package."""
    
    # Load firmware
    with open(firmware_path, "rb") as f:
        firmware_data = f.read()
    
    firmware_size = len(firmware_data)
    firmware_sha256 = compute_sha256(firmware_data)
    firmware_crc32 = compute_crc32(firmware_data)
    
    # Load private key
    private_key = load_private_key(key_path)
    
    # Sign the firmware (Ed25519 signs the message directly, but we sign the hash for consistency)
    # Actually, Ed25519 internally hashes, so we sign the raw firmware
    signature = private_key.sign(firmware_data)
    
    # Get public key for verification info
    public_key = private_key.public_key()
    public_key_bytes = public_key.public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw
    )
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    # Get base name from firmware file
    firmware_basename = os.path.basename(firmware_path)
    name_without_ext = os.path.splitext(firmware_basename)[0]
    
    # Copy firmware
    output_firmware = os.path.join(output_dir, firmware_basename)
    shutil.copy2(firmware_path, output_firmware)
    
    # Write signature
    sig_path = os.path.join(output_dir, f"{name_without_ext}.sig")
    with open(sig_path, "wb") as f:
        f.write(signature)
    
    # Write SHA256 checksum file
    sha256_path = os.path.join(output_dir, f"{name_without_ext}.sha256")
    with open(sha256_path, "w") as f:
        f.write(f"{firmware_sha256}  {firmware_basename}\n")
    
    # Create manifest
    manifest = {
        "version": "1.0",
        "signed_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "algorithm": "Ed25519",
        "firmware": {
            "file": firmware_basename,
            "size": firmware_size,
            "sha256": firmware_sha256,
            "crc32": firmware_crc32,
        },
        "signature": {
            "file": f"{name_without_ext}.sig",
            "size": len(signature),
            "hex": signature.hex(),
        },
        "public_key": {
            "hex": public_key_bytes.hex(),
        }
    }
    
    manifest_path = os.path.join(output_dir, "manifest.json")
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)
    
    print(f"Signed firmware package created in {output_dir}/")
    print(f"  Firmware:  {firmware_basename} ({firmware_size} bytes)")
    print(f"  SHA256:    {firmware_sha256}")
    print(f"  CRC32:     0x{firmware_crc32:08x}")
    print(f"  Signature: {name_without_ext}.sig ({len(signature)} bytes)")
    print()
    print("Signature (hex):")
    print(f"  {signature.hex()}")
    
    return True


def verify_signature(firmware_path: str, sig_path: str, public_key_hex: str) -> bool:
    """Verify a firmware signature (for testing)."""
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
    
    # Load firmware
    with open(firmware_path, "rb") as f:
        firmware_data = f.read()
    
    # Load signature
    with open(sig_path, "rb") as f:
        signature = f.read()
    
    # Load public key
    public_key_bytes = bytes.fromhex(public_key_hex)
    public_key = Ed25519PublicKey.from_public_bytes(public_key_bytes)
    
    # Verify
    try:
        public_key.verify(signature, firmware_data)
        return True
    except Exception:
        return False


def main():
    if len(sys.argv) < 3:
        print("Usage: sign_firmware.py <firmware.bin> <private_key.pem> [output_dir]")
        print()
        print("Arguments:")
        print("  firmware.bin    - Firmware binary to sign")
        print("  private_key.pem - Ed25519 private key")
        print("  output_dir      - Output directory (default: same as firmware)")
        sys.exit(1)
    
    firmware_path = sys.argv[1]
    key_path = sys.argv[2]
    
    if len(sys.argv) > 3:
        output_dir = sys.argv[3]
    else:
        # Default: create directory next to firmware with same name
        firmware_basename = os.path.basename(firmware_path)
        name_without_ext = os.path.splitext(firmware_basename)[0]
        output_dir = os.path.join(os.path.dirname(firmware_path), name_without_ext)
    
    # Validate inputs
    if not os.path.exists(firmware_path):
        print(f"Error: Firmware file not found: {firmware_path}")
        sys.exit(1)
    
    if not os.path.exists(key_path):
        print(f"Error: Private key not found: {key_path}")
        sys.exit(1)
    
    # Sign
    success = sign_firmware(firmware_path, key_path, output_dir)
    
    if success:
        # Verify the signature we just created
        manifest_path = os.path.join(output_dir, "manifest.json")
        with open(manifest_path, "r") as f:
            manifest = json.load(f)
        
        firmware_in_pkg = os.path.join(output_dir, manifest["firmware"]["file"])
        sig_in_pkg = os.path.join(output_dir, manifest["signature"]["file"])
        public_key_hex = manifest["public_key"]["hex"]
        
        if verify_signature(firmware_in_pkg, sig_in_pkg, public_key_hex):
            print("\nSignature verified successfully!")
        else:
            print("\nWARNING: Signature verification failed!")
            sys.exit(1)


if __name__ == "__main__":
    main()
