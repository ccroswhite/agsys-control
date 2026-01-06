import 'dart:convert';
import 'dart:io';
import 'package:crypto/crypto.dart';
import 'package:http/http.dart' as http;
import 'package:path_provider/path_provider.dart';

import '../models/firmware_version.dart';

/// Repository for managing firmware versions
/// 
/// Handles downloading, caching, and managing multiple firmware versions
/// to support both upgrades and downgrades.
class FirmwareRepository {
  static const String _cacheSubdir = 'firmware_cache';
  static const String _manifestFile = 'manifest.json';
  static const int _maxCachedVersions = 5;

  final String baseUrl;
  final http.Client _client;

  String? _cacheDir;
  List<FirmwareRelease>? _availableReleases;
  Map<String, CachedFirmware>? _cachedFirmware;

  FirmwareRepository({
    required this.baseUrl,
    http.Client? client,
  }) : _client = client ?? http.Client();

  /// Initialize the repository
  Future<void> init() async {
    final appDir = await getApplicationDocumentsDirectory();
    _cacheDir = '${appDir.path}/$_cacheSubdir';
    
    // Create cache directory if needed
    final cacheDir = Directory(_cacheDir!);
    if (!await cacheDir.exists()) {
      await cacheDir.create(recursive: true);
    }

    // Load cached firmware manifest
    await _loadCachedManifest();
  }

  /// Get the cache directory path
  String get cacheDirectory => _cacheDir ?? '';

  /// Fetch available firmware releases from server
  Future<List<FirmwareRelease>> fetchAvailableReleases({
    String? deviceType,
    bool includeUnstable = false,
  }) async {
    try {
      final uri = Uri.parse('$baseUrl/api/firmware/releases');
      final response = await _client.get(uri);

      if (response.statusCode != 200) {
        throw Exception('Failed to fetch releases: ${response.statusCode}');
      }

      final List<dynamic> data = json.decode(response.body);
      var releases = data.map((j) => FirmwareRelease.fromJson(j)).toList();

      // Filter by device type
      if (deviceType != null) {
        releases = releases.where((r) => r.deviceType == deviceType).toList();
      }

      // Filter unstable if not requested
      if (!includeUnstable) {
        releases = releases.where((r) => r.isStable).toList();
      }

      // Sort by version (newest first)
      releases.sort((a, b) => b.version.compareTo(a.version));

      _availableReleases = releases;
      return releases;
    } catch (e) {
      // Return cached releases if available
      if (_availableReleases != null) {
        return _availableReleases!;
      }
      rethrow;
    }
  }

  /// Get available releases (from cache if not fetched)
  List<FirmwareRelease> get availableReleases => _availableReleases ?? [];

  /// Get cached firmware versions
  List<CachedFirmware> get cachedFirmware =>
      _cachedFirmware?.values.toList() ?? [];

  /// Download a specific firmware version
  Future<CachedFirmware> downloadFirmware(
    FirmwareRelease release, {
    void Function(int received, int total)? onProgress,
  }) async {
    if (_cacheDir == null) {
      throw StateError('Repository not initialized');
    }

    // Check if already cached
    final cacheKey = '${release.deviceType}_${release.version}';
    if (_cachedFirmware?.containsKey(cacheKey) == true) {
      final cached = _cachedFirmware![cacheKey]!;
      if (await _verifyCachedFirmware(cached)) {
        return cached;
      }
      // Cached file is corrupted, re-download
      await _removeCachedFirmware(cacheKey);
    }

    // Download firmware
    final request = http.Request('GET', Uri.parse(release.downloadUrl));
    final streamedResponse = await _client.send(request);

    if (streamedResponse.statusCode != 200) {
      throw Exception('Download failed: ${streamedResponse.statusCode}');
    }

    final totalBytes = streamedResponse.contentLength ?? release.fileSize;
    final bytes = <int>[];
    var receivedBytes = 0;

    await for (final chunk in streamedResponse.stream) {
      bytes.addAll(chunk);
      receivedBytes += chunk.length;
      onProgress?.call(receivedBytes, totalBytes);
    }

    // Verify checksum
    final checksum = sha256.convert(bytes).toString();
    if (checksum != release.checksum) {
      throw Exception('Checksum mismatch: expected ${release.checksum}, got $checksum');
    }

    // Save to cache
    final fileName = 'firmware_${release.deviceType}_${release.version}.zip';
    final filePath = '$_cacheDir/$fileName';
    await File(filePath).writeAsBytes(bytes);

    // Create cached firmware entry
    final cached = CachedFirmware(
      release: release,
      localPath: filePath,
      downloadedAt: DateTime.now(),
      checksum: checksum,
    );

    // Update manifest
    _cachedFirmware ??= {};
    _cachedFirmware![cacheKey] = cached;
    await _saveCachedManifest();

    // Cleanup old versions if needed
    await _cleanupOldVersions(release.deviceType);

    return cached;
  }

  /// Get cached firmware for a specific version
  CachedFirmware? getCachedFirmware(String deviceType, FirmwareVersion version) {
    final cacheKey = '${deviceType}_$version';
    return _cachedFirmware?[cacheKey];
  }

  /// Check if a version is cached
  bool isCached(String deviceType, FirmwareVersion version) {
    return getCachedFirmware(deviceType, version) != null;
  }

  /// Get recommended versions for a device
  /// 
  /// Returns:
  /// - Latest stable version
  /// - Previous stable version (for rollback)
  /// - Current version (if different, for reinstall)
  List<RecommendedFirmware> getRecommendedVersions({
    required String deviceType,
    required FirmwareVersion currentVersion,
  }) {
    final releases = _availableReleases
            ?.where((r) => r.deviceType == deviceType && r.isStable)
            .toList() ??
        [];

    if (releases.isEmpty) return [];

    final recommendations = <RecommendedFirmware>[];

    // Latest version
    final latest = releases.first;
    if (latest.version > currentVersion) {
      recommendations.add(RecommendedFirmware(
        release: latest,
        reason: RecommendReason.latest,
        isCached: isCached(deviceType, latest.version),
      ));
    }

    // Previous stable (for rollback)
    final previousStable = releases.where((r) => r.version < currentVersion).toList();
    if (previousStable.isNotEmpty) {
      final previous = previousStable.first;
      recommendations.add(RecommendedFirmware(
        release: previous,
        reason: RecommendReason.rollback,
        isCached: isCached(deviceType, previous.version),
      ));
    }

    // Current version (for reinstall)
    final current = releases.where((r) => r.version == currentVersion).toList();
    if (current.isNotEmpty) {
      recommendations.add(RecommendedFirmware(
        release: current.first,
        reason: RecommendReason.reinstall,
        isCached: isCached(deviceType, current.first.version),
      ));
    }

    return recommendations;
  }

  /// Pre-download recommended versions for offline use
  Future<void> preloadRecommendedVersions({
    required String deviceType,
    required FirmwareVersion currentVersion,
    void Function(String version, int progress)? onProgress,
  }) async {
    final recommendations = getRecommendedVersions(
      deviceType: deviceType,
      currentVersion: currentVersion,
    );

    for (final rec in recommendations) {
      if (!rec.isCached) {
        await downloadFirmware(
          rec.release,
          onProgress: (received, total) {
            final percent = (received * 100 / total).round();
            onProgress?.call(rec.release.version.toString(), percent);
          },
        );
      }
    }
  }

  /// Remove a cached firmware version
  Future<void> removeCachedFirmware(String deviceType, FirmwareVersion version) async {
    final cacheKey = '${deviceType}_$version';
    await _removeCachedFirmware(cacheKey);
  }

  /// Clear all cached firmware
  Future<void> clearCache() async {
    if (_cacheDir == null) return;

    final cacheDir = Directory(_cacheDir!);
    if (await cacheDir.exists()) {
      await cacheDir.delete(recursive: true);
      await cacheDir.create();
    }

    _cachedFirmware?.clear();
    await _saveCachedManifest();
  }

  // Private methods

  Future<void> _loadCachedManifest() async {
    final manifestPath = '$_cacheDir/$_manifestFile';
    final file = File(manifestPath);

    if (await file.exists()) {
      try {
        final content = await file.readAsString();
        final Map<String, dynamic> data = json.decode(content);
        
        _cachedFirmware = {};
        for (final entry in data.entries) {
          _cachedFirmware![entry.key] = CachedFirmware.fromJson(entry.value);
        }

        // Verify all cached files still exist
        final toRemove = <String>[];
        for (final entry in _cachedFirmware!.entries) {
          if (!await File(entry.value.localPath).exists()) {
            toRemove.add(entry.key);
          }
        }
        for (final key in toRemove) {
          _cachedFirmware!.remove(key);
        }
      } catch (e) {
        _cachedFirmware = {};
      }
    } else {
      _cachedFirmware = {};
    }
  }

  Future<void> _saveCachedManifest() async {
    final manifestPath = '$_cacheDir/$_manifestFile';
    final data = _cachedFirmware?.map(
      (key, value) => MapEntry(key, value.toJson()),
    );
    await File(manifestPath).writeAsString(json.encode(data ?? {}));
  }

  Future<bool> _verifyCachedFirmware(CachedFirmware cached) async {
    final file = File(cached.localPath);
    if (!await file.exists()) return false;

    final bytes = await file.readAsBytes();
    final checksum = sha256.convert(bytes).toString();
    return checksum == cached.checksum;
  }

  Future<void> _removeCachedFirmware(String cacheKey) async {
    final cached = _cachedFirmware?[cacheKey];
    if (cached != null) {
      final file = File(cached.localPath);
      if (await file.exists()) {
        await file.delete();
      }
      _cachedFirmware!.remove(cacheKey);
      await _saveCachedManifest();
    }
  }

  Future<void> _cleanupOldVersions(String deviceType) async {
    if (_cachedFirmware == null) return;

    // Get all cached versions for this device type
    final deviceVersions = _cachedFirmware!.entries
        .where((e) => e.key.startsWith('${deviceType}_'))
        .toList();

    // Sort by download date (oldest first)
    deviceVersions.sort(
      (a, b) => a.value.downloadedAt.compareTo(b.value.downloadedAt),
    );

    // Remove oldest versions if over limit
    while (deviceVersions.length > _maxCachedVersions) {
      final oldest = deviceVersions.removeAt(0);
      await _removeCachedFirmware(oldest.key);
    }
  }

  void dispose() {
    _client.close();
  }
}

/// Cached firmware information
class CachedFirmware {
  final FirmwareRelease release;
  final String localPath;
  final DateTime downloadedAt;
  final String checksum;

  const CachedFirmware({
    required this.release,
    required this.localPath,
    required this.downloadedAt,
    required this.checksum,
  });

  factory CachedFirmware.fromJson(Map<String, dynamic> json) {
    return CachedFirmware(
      release: FirmwareRelease.fromJson(json['release']),
      localPath: json['local_path'] as String,
      downloadedAt: DateTime.parse(json['downloaded_at'] as String),
      checksum: json['checksum'] as String,
    );
  }

  Map<String, dynamic> toJson() => {
        'release': release.toJson(),
        'local_path': localPath,
        'downloaded_at': downloadedAt.toIso8601String(),
        'checksum': checksum,
      };
}

/// Reason for recommending a firmware version
enum RecommendReason {
  latest,
  rollback,
  reinstall,
  critical,
}

/// Recommended firmware with reason
class RecommendedFirmware {
  final FirmwareRelease release;
  final RecommendReason reason;
  final bool isCached;

  const RecommendedFirmware({
    required this.release,
    required this.reason,
    required this.isCached,
  });

  String get reasonLabel {
    switch (reason) {
      case RecommendReason.latest:
        return 'Latest Version';
      case RecommendReason.rollback:
        return 'Previous Stable (Rollback)';
      case RecommendReason.reinstall:
        return 'Current Version (Reinstall)';
      case RecommendReason.critical:
        return 'Critical Update';
    }
  }
}
