import 'package:flutter/foundation.dart';

import '../models/firmware_version.dart';
import '../services/firmware_repository.dart';

/// Provider for firmware version management
class FirmwareProvider extends ChangeNotifier {
  final FirmwareRepository _repository;
  
  bool _isLoading = false;
  String? _error;
  String _deviceType = 'soil_moisture'; // Default device type

  FirmwareProvider({String? baseUrl})
      : _repository = FirmwareRepository(
          baseUrl: baseUrl ?? 'https://api.agsys.io',
        ) {
    _init();
  }

  // Getters
  bool get isLoading => _isLoading;
  String? get error => _error;
  List<FirmwareRelease> get availableReleases => _repository.availableReleases;
  List<CachedFirmware> get cachedFirmware => _repository.cachedFirmware;
  String get deviceType => _deviceType;

  Future<void> _init() async {
    await _repository.init();
    notifyListeners();
  }

  /// Set the device type for filtering releases
  void setDeviceType(String deviceType) {
    _deviceType = deviceType;
    notifyListeners();
  }

  /// Fetch available releases from server
  Future<void> fetchReleases({bool includeUnstable = false}) async {
    _isLoading = true;
    _error = null;
    notifyListeners();

    try {
      await _repository.fetchAvailableReleases(
        deviceType: _deviceType,
        includeUnstable: includeUnstable,
      );
    } catch (e) {
      _error = 'Failed to fetch releases: $e';
    } finally {
      _isLoading = false;
      notifyListeners();
    }
  }

  /// Download a specific firmware version
  Future<CachedFirmware?> downloadFirmware(
    FirmwareRelease release, {
    void Function(int received, int total)? onProgress,
  }) async {
    try {
      final cached = await _repository.downloadFirmware(
        release,
        onProgress: onProgress,
      );
      notifyListeners();
      return cached;
    } catch (e) {
      _error = 'Download failed: $e';
      notifyListeners();
      return null;
    }
  }

  /// Check if a version is cached
  bool isCached(FirmwareVersion version) {
    return _repository.isCached(_deviceType, version);
  }

  /// Get cached firmware for a version
  CachedFirmware? getCachedFirmware(FirmwareVersion version) {
    return _repository.getCachedFirmware(_deviceType, version);
  }

  /// Get recommended versions for current device
  List<RecommendedFirmware> getRecommendedVersions(FirmwareVersion currentVersion) {
    return _repository.getRecommendedVersions(
      deviceType: _deviceType,
      currentVersion: currentVersion,
    );
  }

  /// Pre-download recommended versions
  Future<void> preloadRecommendedVersions(
    FirmwareVersion currentVersion, {
    void Function(String version, int progress)? onProgress,
  }) async {
    await _repository.preloadRecommendedVersions(
      deviceType: _deviceType,
      currentVersion: currentVersion,
      onProgress: onProgress,
    );
    notifyListeners();
  }

  /// Remove a cached firmware version
  Future<void> removeCachedFirmware(FirmwareVersion version) async {
    await _repository.removeCachedFirmware(_deviceType, version);
    notifyListeners();
  }

  /// Clear all cached firmware
  Future<void> clearCache() async {
    await _repository.clearCache();
    notifyListeners();
  }

  /// Clear error
  void clearError() {
    _error = null;
    notifyListeners();
  }

  @override
  void dispose() {
    _repository.dispose();
    super.dispose();
  }
}
