"""
REST API for AgSys Controller

Provides HTTP endpoints for monitoring and controlling the IoT system.
"""

import os
import logging
from flask import Flask, jsonify, request
from flask_cors import CORS
from controller import Controller

logger = logging.getLogger(__name__)

app = Flask(__name__)
CORS(app)

# Global controller instance
controller: Controller = None


def init_controller():
    """Initialize the controller."""
    global controller
    controller = Controller()
    if not controller.start():
        raise RuntimeError("Failed to start controller")


@app.route('/api/health', methods=['GET'])
def health():
    """Health check endpoint."""
    return jsonify({"status": "ok"})


@app.route('/api/devices', methods=['GET'])
def get_devices():
    """Get list of all devices."""
    return jsonify(controller.get_devices())


@app.route('/api/devices/<uuid>/data', methods=['GET'])
def get_device_data(uuid: str):
    """Get sensor data for a device."""
    limit = request.args.get('limit', 100, type=int)
    return jsonify(controller.get_sensor_data(uuid, limit))


@app.route('/api/ota/start', methods=['POST'])
def start_ota():
    """
    Start an OTA update.
    
    Request body:
    {
        "firmware_path": "/path/to/firmware.bin",
        "version": [1, 2, 0],
        "device_type": 1  // Optional, 255 = all
    }
    """
    data = request.get_json()
    
    if not data or 'firmware_path' not in data or 'version' not in data:
        return jsonify({"error": "Missing required fields"}), 400
    
    firmware_path = data['firmware_path']
    version = tuple(data['version'])
    device_type = data.get('device_type', 0xFF)
    
    if not os.path.exists(firmware_path):
        return jsonify({"error": f"Firmware not found: {firmware_path}"}), 404
    
    try:
        announce_id = controller.start_ota_update(firmware_path, version, device_type)
        return jsonify({
            "status": "started",
            "announce_id": announce_id
        })
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route('/api/ota/stop', methods=['POST'])
def stop_ota():
    """Stop the current OTA update."""
    controller.stop_ota_update()
    return jsonify({"status": "stopped"})


@app.route('/api/ota/progress', methods=['GET'])
def get_ota_progress():
    """Get OTA update progress."""
    return jsonify(controller.get_ota_progress())


@app.route('/api/ota/devices', methods=['GET'])
def get_ota_devices():
    """Get OTA status for all devices."""
    return jsonify(controller.get_ota_device_status())


def run_api(host: str = '0.0.0.0', port: int = 5000, debug: bool = False):
    """Run the API server."""
    init_controller()
    app.run(host=host, port=port, debug=debug, threaded=True)


if __name__ == '__main__':
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s [%(levelname)s] %(name)s: %(message)s'
    )
    run_api(debug=True)
