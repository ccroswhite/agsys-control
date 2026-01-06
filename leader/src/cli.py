#!/usr/bin/env python3
"""
CLI for AgSys Controller

Command-line interface for managing the IoT system and OTA updates.
"""

import argparse
import sys
import time
import logging
from controller import Controller


def cmd_run(args):
    """Run the controller in foreground."""
    controller = Controller(db_path=args.db)
    
    if not controller.start():
        print("Failed to start controller")
        sys.exit(1)
    
    print("Controller running. Press Ctrl+C to stop.")
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        controller.stop()


def cmd_devices(args):
    """List all known devices."""
    controller = Controller(db_path=args.db)
    devices = controller.get_devices()
    
    if not devices:
        print("No devices registered")
        return
    
    print(f"{'UUID':<36} {'Type':<15} {'Last Seen':<20} {'Battery':<10} {'RSSI':<8}")
    print("-" * 95)
    
    for d in devices:
        print(f"{d['uuid']:<36} {d['device_type']:<15} {d['last_seen'][:19]:<20} {d['battery_mv']:<10} {d['rssi']:<8}")


def cmd_ota_start(args):
    """Start an OTA update."""
    controller = Controller(db_path=args.db)
    
    if not controller.start():
        print("Failed to start controller")
        sys.exit(1)
    
    version = tuple(map(int, args.version.split('.')))
    
    print(f"Starting OTA update...")
    print(f"  Firmware: {args.firmware}")
    print(f"  Version:  {args.version}")
    print(f"  Target:   {'All devices' if args.device_type == 0xFF else f'Type {args.device_type}'}")
    
    try:
        announce_id = controller.start_ota_update(
            firmware_path=args.firmware,
            version=version,
            device_type=args.device_type
        )
        print(f"  Announce ID: {announce_id}")
        print()
        print("OTA update started. Monitoring progress...")
        print("Press Ctrl+C to stop.")
        print()
        
        last_progress = {}
        while True:
            progress = controller.get_ota_progress()
            
            if not progress.get('active'):
                print("\nOTA session completed.")
                break
            
            # Print progress update
            if progress != last_progress:
                print(
                    f"\rDevices: {progress['devices_receiving']} receiving, "
                    f"{progress['devices_complete']} complete, "
                    f"{progress['devices_error']} errors | "
                    f"Elapsed: {progress['elapsed_sec']}s",
                    end='', flush=True
                )
                last_progress = progress
            
            time.sleep(1)
        
        # Print final status
        print("\nFinal device status:")
        for d in controller.get_ota_device_status():
            status = "✓" if d['state'] == 'COMPLETE' else "✗" if d['state'] == 'ERROR' else "?"
            print(f"  {status} {d['uuid'][:16]}... {d['progress']}% - {d['state']}")
        
    except KeyboardInterrupt:
        print("\n\nStopping OTA update...")
        controller.stop_ota_update()
    finally:
        controller.stop()


def cmd_ota_status(args):
    """Get OTA update status."""
    controller = Controller(db_path=args.db)
    
    if not controller.start():
        print("Failed to start controller")
        sys.exit(1)
    
    try:
        progress = controller.get_ota_progress()
        
        if not progress.get('active'):
            print("No OTA update in progress")
            return
        
        print("OTA Update Status")
        print("-" * 40)
        print(f"  Announce ID:    {progress['announce_id']}")
        print(f"  Version:        {progress['version']}")
        print(f"  Firmware Size:  {progress['firmware_size']} bytes")
        print(f"  Total Chunks:   {progress['total_chunks']}")
        print(f"  Elapsed:        {progress['elapsed_sec']} seconds")
        print()
        print(f"  Devices Total:     {progress['devices_total']}")
        print(f"  Devices Receiving: {progress['devices_receiving']}")
        print(f"  Devices Complete:  {progress['devices_complete']}")
        print(f"  Devices Error:     {progress['devices_error']}")
        print()
        
        print("Device Details:")
        for d in controller.get_ota_device_status():
            print(f"  {d['uuid'][:16]}... {d['progress']:3}% {d['state']:<12} {d['error']}")
    
    finally:
        controller.stop()


def cmd_data(args):
    """Get sensor data for a device."""
    controller = Controller(db_path=args.db)
    data = controller.get_sensor_data(args.uuid, args.limit)
    
    if not data:
        print(f"No data for device {args.uuid}")
        return
    
    print(f"{'Timestamp':<20} {'Moisture':<10} {'Battery':<10} {'Temp':<8} {'RSSI':<8}")
    print("-" * 60)
    
    for d in data:
        print(
            f"{d['timestamp'][:19]:<20} "
            f"{d['moisture_percent']:>3}% ({d['moisture_raw']:<4}) "
            f"{d['battery_mv']:<10} "
            f"{d['temperature']:<8.1f} "
            f"{d['rssi']:<8}"
        )


def main():
    parser = argparse.ArgumentParser(
        description='AgSys Controller CLI',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        '--db', default='agsys.db',
        help='Database path (default: agsys.db)'
    )
    parser.add_argument(
        '-v', '--verbose', action='store_true',
        help='Enable verbose logging'
    )
    
    subparsers = parser.add_subparsers(dest='command', help='Commands')
    
    # run command
    run_parser = subparsers.add_parser('run', help='Run the controller')
    run_parser.set_defaults(func=cmd_run)
    
    # devices command
    devices_parser = subparsers.add_parser('devices', help='List devices')
    devices_parser.set_defaults(func=cmd_devices)
    
    # data command
    data_parser = subparsers.add_parser('data', help='Get sensor data')
    data_parser.add_argument('uuid', help='Device UUID')
    data_parser.add_argument('--limit', type=int, default=20, help='Number of records')
    data_parser.set_defaults(func=cmd_data)
    
    # ota start command
    ota_start_parser = subparsers.add_parser('ota-start', help='Start OTA update')
    ota_start_parser.add_argument('firmware', help='Path to firmware binary')
    ota_start_parser.add_argument('version', help='Version string (e.g., 1.2.0)')
    ota_start_parser.add_argument(
        '--device-type', type=int, default=0xFF,
        help='Target device type (default: 255 = all)'
    )
    ota_start_parser.set_defaults(func=cmd_ota_start)
    
    # ota status command
    ota_status_parser = subparsers.add_parser('ota-status', help='Get OTA status')
    ota_status_parser.set_defaults(func=cmd_ota_status)
    
    args = parser.parse_args()
    
    if args.verbose:
        logging.basicConfig(
            level=logging.DEBUG,
            format='%(asctime)s [%(levelname)s] %(name)s: %(message)s'
        )
    else:
        logging.basicConfig(
            level=logging.WARNING,
            format='%(message)s'
        )
    
    if not args.command:
        parser.print_help()
        sys.exit(1)
    
    args.func(args)


if __name__ == '__main__':
    main()
