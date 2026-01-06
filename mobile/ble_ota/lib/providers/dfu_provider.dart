import 'dart:async';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:nordic_dfu/nordic_dfu.dart';
import 'package:path_provider/path_provider.dart';

/// DFU update state
enum DfuState {
  idle,
  preparing,
  connecting,
  starting,
  uploading,
  validating,
  completed,
  aborted,
  error,
}

/// Provider for Nordic DFU firmware updates
class DfuProvider extends ChangeNotifier {
  DfuState _state = DfuState.idle;
  int _progress = 0;
  double _speed = 0;
  int _avgSpeed = 0;
  int _currentPart = 0;
  int _totalParts = 0;
  String? _error;
  String? _firmwarePath;
  String? _firmwareVersion;

  // Getters
  DfuState get state => _state;
  int get progress => _progress;
  double get speed => _speed;
  int get avgSpeed => _avgSpeed;
  int get currentPart => _currentPart;
  int get totalParts => _totalParts;
  String? get error => _error;
  String? get firmwarePath => _firmwarePath;
  String? get firmwareVersion => _firmwareVersion;
  bool get isUpdating => _state != DfuState.idle && 
                         _state != DfuState.completed && 
                         _state != DfuState.error &&
                         _state != DfuState.aborted;

  /// Set the firmware file to upload
  void setFirmware(String path, {String? version}) {
    _firmwarePath = path;
    _firmwareVersion = version;
    notifyListeners();
  }

  /// Clear the firmware selection
  void clearFirmware() {
    _firmwarePath = null;
    _firmwareVersion = null;
    notifyListeners();
  }

  /// Start DFU update
  Future<bool> startDfu(String deviceId) async {
    if (_firmwarePath == null) {
      _error = 'No firmware file selected';
      notifyListeners();
      return false;
    }

    // Verify file exists
    final file = File(_firmwarePath!);
    if (!await file.exists()) {
      _error = 'Firmware file not found';
      notifyListeners();
      return false;
    }

    _state = DfuState.preparing;
    _progress = 0;
    _error = null;
    notifyListeners();

    try {
      await NordicDfu().startDfu(
        deviceId,
        _firmwarePath!,
        fileInAsset: false,
        forceDfu: false,
        enableUnsafeExperimentalButtonlessServiceInSecureDfu: true,
        onDeviceConnecting: (deviceAddress) {
          _state = DfuState.connecting;
          notifyListeners();
        },
        onDeviceConnected: (deviceAddress) {
          _state = DfuState.starting;
          notifyListeners();
        },
        onDfuProcessStarting: (deviceAddress) {
          _state = DfuState.starting;
          notifyListeners();
        },
        onDfuProcessStarted: (deviceAddress) {
          _state = DfuState.uploading;
          notifyListeners();
        },
        onEnablingDfuMode: (deviceAddress) {
          _state = DfuState.starting;
          notifyListeners();
        },
        onProgressChanged: (
          deviceAddress,
          percent,
          speed,
          avgSpeed,
          currentPart,
          partsTotal,
        ) {
          _state = DfuState.uploading;
          _progress = percent;
          _speed = speed;
          _avgSpeed = avgSpeed;
          _currentPart = currentPart;
          _totalParts = partsTotal;
          notifyListeners();
        },
        onFirmwareValidating: (deviceAddress) {
          _state = DfuState.validating;
          notifyListeners();
        },
        onDeviceDisconnecting: (deviceAddress) {
          // Device disconnects after successful update
        },
        onDeviceDisconnected: (deviceAddress) {
          // Normal after update
        },
        onDfuCompleted: (deviceAddress) {
          _state = DfuState.completed;
          _progress = 100;
          notifyListeners();
        },
        onDfuAborted: (deviceAddress) {
          _state = DfuState.aborted;
          _error = 'DFU aborted';
          notifyListeners();
        },
        onError: (deviceAddress, error, errorType, message) {
          _state = DfuState.error;
          _error = message ?? 'DFU error: $errorType';
          notifyListeners();
        },
      );

      return _state == DfuState.completed;
    } catch (e) {
      _state = DfuState.error;
      _error = 'DFU failed: $e';
      notifyListeners();
      return false;
    }
  }

  /// Abort the current DFU update
  Future<void> abortDfu() async {
    try {
      await NordicDfu().abortDfu();
      _state = DfuState.aborted;
      notifyListeners();
    } catch (e) {
      // Ignore abort errors
    }
  }

  /// Reset state for a new update
  void reset() {
    _state = DfuState.idle;
    _progress = 0;
    _speed = 0;
    _avgSpeed = 0;
    _currentPart = 0;
    _totalParts = 0;
    _error = null;
    notifyListeners();
  }

  /// Clear error
  void clearError() {
    _error = null;
    notifyListeners();
  }

  /// Get the app's documents directory for storing firmware files
  static Future<String> getFirmwareDirectory() async {
    final dir = await getApplicationDocumentsDirectory();
    final firmwareDir = Directory('${dir.path}/firmware');
    if (!await firmwareDir.exists()) {
      await firmwareDir.create(recursive: true);
    }
    return firmwareDir.path;
  }

  /// Copy firmware file to app storage
  Future<String?> copyFirmwareToStorage(String sourcePath) async {
    try {
      final sourceFile = File(sourcePath);
      final fileName = sourcePath.split('/').last;
      final destDir = await getFirmwareDirectory();
      final destPath = '$destDir/$fileName';
      
      await sourceFile.copy(destPath);
      return destPath;
    } catch (e) {
      _error = 'Failed to copy firmware: $e';
      notifyListeners();
      return null;
    }
  }
}
