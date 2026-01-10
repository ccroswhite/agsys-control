#!/bin/bash
# Build all devices (debug by default)
# Usage: ./scripts/build-all.sh [debug|release]

set -e

BUILD_TYPE="${1:-debug}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEVICES_DIR="$SCRIPT_DIR/../devices"

echo "=== Building all devices ($BUILD_TYPE) ==="

# PlatformIO-based devices
PIO_DEVICES=(
    "soilmoisture"
    "valveactuator"
    "valvecontrol"
    "watermeter"
)

FAILED=()
SUCCEEDED=()

for device in "${PIO_DEVICES[@]}"; do
    device_path="$DEVICES_DIR/$device"
    if [ -f "$device_path/platformio.ini" ]; then
        echo ""
        echo "--- Building $device (PlatformIO) ---"
        cd "$device_path"
        if pio run -e "$BUILD_TYPE"; then
            SUCCEEDED+=("$device")
        else
            FAILED+=("$device")
        fi
    fi
done

# Go-based property controller
echo ""
echo "--- Building property-controller (Go) ---"
cd "$DEVICES_DIR/property-controller"
if make build; then
    SUCCEEDED+=("property-controller")
else
    FAILED+=("property-controller")
fi

echo ""
echo "=== Build Summary ==="
echo "Succeeded: ${#SUCCEEDED[@]}"
for d in "${SUCCEEDED[@]}"; do
    echo "  ✓ $d"
done

if [ ${#FAILED[@]} -gt 0 ]; then
    echo "Failed: ${#FAILED[@]}"
    for d in "${FAILED[@]}"; do
        echo "  ✗ $d"
    done
    exit 1
fi

echo ""
echo "=== All devices built successfully ==="
