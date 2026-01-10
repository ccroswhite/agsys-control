"""
PlatformIO script to copy Wire.h stub to Adafruit BusIO library.

The Adafruit BusIO library includes I2C support that requires Wire.h,
but the nRF52 framework's Wire library has a TinyUSB dependency issue.
Since we only use SPI devices, we provide a stub Wire.h instead.

This script runs immediately when platformio.ini is parsed.
"""

Import("env")
import os
import shutil

project_dir = env.get("PROJECT_DIR")
libdeps_dir = os.path.join(project_dir, ".pio", "libdeps")
stub_src = os.path.join(project_dir, "include", "Wire.h")

if os.path.exists(stub_src) and os.path.exists(libdeps_dir):
    # Find all Adafruit BusIO directories across all environments
    for env_name in os.listdir(libdeps_dir):
        busio_dir = os.path.join(libdeps_dir, env_name, "Adafruit BusIO")
        if os.path.isdir(busio_dir):
            stub_dst = os.path.join(busio_dir, "Wire.h")
            if not os.path.exists(stub_dst):
                shutil.copy(stub_src, stub_dst)
                print(f"Copied Wire.h stub to {stub_dst}")
