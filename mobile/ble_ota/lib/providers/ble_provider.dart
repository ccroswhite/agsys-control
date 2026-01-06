import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

/// BLE device information for AgSys devices
class AgSysDevice {
  final BluetoothDevice device;
  final String name;
  final String? uuid;
  final int rssi;
  final DateTime lastSeen;

  AgSysDevice({
    required this.device,
    required this.name,
    this.uuid,
    required this.rssi,
    required this.lastSeen,
  });

  String get id => device.remoteId.str;

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is AgSysDevice &&
          runtimeType == other.runtimeType &&
          id == other.id;

  @override
  int get hashCode => id.hashCode;
}

/// BLE scanning and connection state
enum BleState {
  unknown,
  unavailable,
  unauthorized,
  turningOn,
  on,
  turningOff,
  off,
}

/// Provider for BLE scanning and device management
class BleProvider extends ChangeNotifier {
  BleState _state = BleState.unknown;
  bool _isScanning = false;
  final Map<String, AgSysDevice> _devices = {};
  AgSysDevice? _selectedDevice;
  BluetoothDevice? _connectedDevice;
  bool _isConnecting = false;
  String? _error;

  StreamSubscription<BluetoothAdapterState>? _adapterStateSubscription;
  StreamSubscription<List<ScanResult>>? _scanSubscription;
  StreamSubscription<BluetoothConnectionState>? _connectionSubscription;

  BleProvider() {
    _init();
  }

  // Getters
  BleState get state => _state;
  bool get isScanning => _isScanning;
  List<AgSysDevice> get devices => _devices.values.toList()
    ..sort((a, b) => b.rssi.compareTo(a.rssi));
  AgSysDevice? get selectedDevice => _selectedDevice;
  bool get isConnected => _connectedDevice != null;
  bool get isConnecting => _isConnecting;
  String? get error => _error;

  void _init() {
    // Listen to adapter state changes
    _adapterStateSubscription = FlutterBluePlus.adapterState.listen((state) {
      _state = _mapAdapterState(state);
      notifyListeners();
    });
  }

  BleState _mapAdapterState(BluetoothAdapterState state) {
    switch (state) {
      case BluetoothAdapterState.unknown:
        return BleState.unknown;
      case BluetoothAdapterState.unavailable:
        return BleState.unavailable;
      case BluetoothAdapterState.unauthorized:
        return BleState.unauthorized;
      case BluetoothAdapterState.turningOn:
        return BleState.turningOn;
      case BluetoothAdapterState.on:
        return BleState.on;
      case BluetoothAdapterState.turningOff:
        return BleState.turningOff;
      case BluetoothAdapterState.off:
        return BleState.off;
    }
  }

  /// Start scanning for AgSys devices
  Future<void> startScan({Duration timeout = const Duration(seconds: 10)}) async {
    if (_isScanning) return;
    if (_state != BleState.on) {
      _error = 'Bluetooth is not available';
      notifyListeners();
      return;
    }

    _error = null;
    _isScanning = true;
    _devices.clear();
    notifyListeners();

    try {
      // Start scanning
      await FlutterBluePlus.startScan(
        timeout: timeout,
        withServices: [
          // Nordic DFU Service UUID
          Guid('0000fe59-0000-1000-8000-00805f9b34fb'),
        ],
      );

      // Listen to scan results
      _scanSubscription = FlutterBluePlus.scanResults.listen((results) {
        for (final result in results) {
          final name = result.device.platformName;
          
          // Filter for AgSys devices
          if (name.startsWith('AgSys') || name.startsWith('DfuTarg')) {
            final device = AgSysDevice(
              device: result.device,
              name: name,
              uuid: _extractUuid(result.advertisementData),
              rssi: result.rssi,
              lastSeen: DateTime.now(),
            );
            _devices[device.id] = device;
            notifyListeners();
          }
        }
      });

      // Wait for scan to complete
      await Future.delayed(timeout);
    } catch (e) {
      _error = 'Scan failed: $e';
    } finally {
      _isScanning = false;
      notifyListeners();
    }
  }

  /// Stop scanning
  Future<void> stopScan() async {
    await FlutterBluePlus.stopScan();
    _scanSubscription?.cancel();
    _isScanning = false;
    notifyListeners();
  }

  /// Extract UUID from advertisement data if available
  String? _extractUuid(AdvertisementData data) {
    // Try to extract from manufacturer data
    if (data.manufacturerData.isNotEmpty) {
      final bytes = data.manufacturerData.values.first;
      if (bytes.length >= 16) {
        return bytes.sublist(0, 16).map((b) => b.toRadixString(16).padLeft(2, '0')).join();
      }
    }
    return null;
  }

  /// Select a device for firmware update
  void selectDevice(AgSysDevice device) {
    _selectedDevice = device;
    notifyListeners();
  }

  /// Clear device selection
  void clearSelection() {
    _selectedDevice = null;
    notifyListeners();
  }

  /// Connect to the selected device
  Future<bool> connect() async {
    if (_selectedDevice == null) {
      _error = 'No device selected';
      notifyListeners();
      return false;
    }

    _error = null;
    _isConnecting = true;
    notifyListeners();

    try {
      final device = _selectedDevice!.device;
      
      // Listen to connection state
      _connectionSubscription = device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          _connectedDevice = null;
          notifyListeners();
        }
      });

      // Connect
      await device.connect(timeout: const Duration(seconds: 15));
      _connectedDevice = device;
      _isConnecting = false;
      notifyListeners();
      return true;
    } catch (e) {
      _error = 'Connection failed: $e';
      _isConnecting = false;
      notifyListeners();
      return false;
    }
  }

  /// Disconnect from the current device
  Future<void> disconnect() async {
    if (_connectedDevice != null) {
      await _connectedDevice!.disconnect();
      _connectedDevice = null;
      _connectionSubscription?.cancel();
      notifyListeners();
    }
  }

  /// Clear any error
  void clearError() {
    _error = null;
    notifyListeners();
  }

  @override
  void dispose() {
    _adapterStateSubscription?.cancel();
    _scanSubscription?.cancel();
    _connectionSubscription?.cancel();
    disconnect();
    super.dispose();
  }
}
