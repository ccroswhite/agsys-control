#!/usr/bin/env python3
"""
AgSys Firmware Signing Key Generator

Generates an Ed25519 keypair for firmware signing.
- Private key: Used by build system to sign firmware (keep secret!)
- Public key: Embedded in bootloader to verify firmware

Usage:
    python3 generate_signing_key.py [output_dir]

Output:
    signing_key.pem     - Private key (PEM format)
    signing_key.pub     - Public key (PEM format)
    signing_key_pub.h   - Public key as C header for bootloader
"""

import os
import sys
from datetime import datetime

try:
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    from cryptography.hazmat.primitives import serialization
except ImportError:
    print("Error: cryptography package required")
    print("Install with: pip3 install cryptography")
    sys.exit(1)


def generate_keypair(output_dir: str):
    """Generate Ed25519 keypair and save to files."""
    
    # Generate private key
    private_key = Ed25519PrivateKey.generate()
    public_key = private_key.public_key()
    
    # Get raw public key bytes (32 bytes for Ed25519)
    public_key_bytes = public_key.public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw
    )
    
    # Serialize private key (PEM format)
    private_pem = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption()
    )
    
    # Serialize public key (PEM format)
    public_pem = public_key.public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo
    )
    
    # Create output directory if needed
    os.makedirs(output_dir, exist_ok=True)
    
    # Save private key
    private_key_path = os.path.join(output_dir, "signing_key.pem")
    with open(private_key_path, "wb") as f:
        f.write(private_pem)
    os.chmod(private_key_path, 0o600)  # Read/write only for owner
    
    # Save public key (PEM)
    public_key_path = os.path.join(output_dir, "signing_key.pub")
    with open(public_key_path, "wb") as f:
        f.write(public_pem)
    
    # Generate C header for bootloader
    header_path = os.path.join(output_dir, "signing_key_pub.h")
    with open(header_path, "w") as f:
        f.write(f"""/**
 * AgSys Firmware Signing Public Key
 * 
 * Generated: {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}
 * Algorithm: Ed25519
 * 
 * WARNING: Do not modify this file manually!
 * Regenerate with: python3 generate_signing_key.py
 */

#ifndef AGSYS_SIGNING_KEY_PUB_H
#define AGSYS_SIGNING_KEY_PUB_H

#include <stdint.h>

#define AGSYS_ED25519_PUBLIC_KEY_SIZE 32
#define AGSYS_ED25519_SIGNATURE_SIZE  64

/**
 * Ed25519 public key for firmware signature verification.
 * This key is embedded in the bootloader (read-only).
 */
static const uint8_t agsys_signing_public_key[AGSYS_ED25519_PUBLIC_KEY_SIZE] = {{
    {', '.join(f'0x{b:02x}' for b in public_key_bytes[:16])},
    {', '.join(f'0x{b:02x}' for b in public_key_bytes[16:])}
}};

#endif /* AGSYS_SIGNING_KEY_PUB_H */
""")
    
    print(f"Generated Ed25519 keypair in {output_dir}/")
    print(f"  Private key: signing_key.pem (KEEP SECRET!)")
    print(f"  Public key:  signing_key.pub")
    print(f"  C header:    signing_key_pub.h")
    print()
    print("Public key (hex):")
    print(f"  {public_key_bytes.hex()}")
    print()
    print("IMPORTANT: Add signing_key.pem to .gitignore!")


def main():
    if len(sys.argv) > 1:
        output_dir = sys.argv[1]
    else:
        # Default to keys directory in freertos-common
        script_dir = os.path.dirname(os.path.abspath(__file__))
        output_dir = os.path.join(script_dir, "..", "keys")
    
    # Check if keys already exist
    private_key_path = os.path.join(output_dir, "signing_key.pem")
    if os.path.exists(private_key_path):
        print(f"Warning: {private_key_path} already exists!")
        response = input("Overwrite? (yes/no): ")
        if response.lower() != "yes":
            print("Aborted.")
            sys.exit(0)
    
    generate_keypair(output_dir)


if __name__ == "__main__":
    main()
