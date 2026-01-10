#!/bin/bash
# Clean all build artifacts for all devices
# Usage: ./scripts/clean-all.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEVICES_DIR="$SCRIPT_DIR/../devices"

echo "=== Cleaning all device builds ==="

# PlatformIO-based devices
PIO_DEVICES=(
    "soilmoisture"
    "valveactuator"
    "valvecontrol"
    "watermeter"
    "integration_tests/can_bus"
)

for device in "${PIO_DEVICES[@]}"; do
    device_path="$DEVICES_DIR/$device"
    if [ -f "$device_path/platformio.ini" ]; then
        echo ""
        echo "--- Cleaning $device (PlatformIO) ---"
        cd "$device_path"
        pio run -t clean || echo "Warning: clean failed for $device"
    fi
done

# Go-based property controller
echo ""
echo "--- Cleaning property-controller (Go) ---"
cd "$DEVICES_DIR/property-controller"
make clean || echo "Warning: clean failed for property-controller"

echo ""
echo "=== All devices cleaned ==="
