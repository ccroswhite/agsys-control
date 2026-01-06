/// Firmware version information
class FirmwareVersion implements Comparable<FirmwareVersion> {
  final int major;
  final int minor;
  final int patch;
  final String? buildMetadata;

  const FirmwareVersion({
    required this.major,
    required this.minor,
    required this.patch,
    this.buildMetadata,
  });

  /// Parse version string like "1.2.3" or "1.2.3+build123"
  factory FirmwareVersion.parse(String version) {
    final parts = version.split('+');
    final versionPart = parts[0];
    final buildPart = parts.length > 1 ? parts[1] : null;

    final segments = versionPart.split('.');
    if (segments.length != 3) {
      throw FormatException('Invalid version format: $version');
    }

    return FirmwareVersion(
      major: int.parse(segments[0]),
      minor: int.parse(segments[1]),
      patch: int.parse(segments[2]),
      buildMetadata: buildPart,
    );
  }

  @override
  String toString() {
    final base = '$major.$minor.$patch';
    return buildMetadata != null ? '$base+$buildMetadata' : base;
  }

  @override
  int compareTo(FirmwareVersion other) {
    if (major != other.major) return major.compareTo(other.major);
    if (minor != other.minor) return minor.compareTo(other.minor);
    return patch.compareTo(other.patch);
  }

  bool operator >(FirmwareVersion other) => compareTo(other) > 0;
  bool operator <(FirmwareVersion other) => compareTo(other) < 0;
  bool operator >=(FirmwareVersion other) => compareTo(other) >= 0;
  bool operator <=(FirmwareVersion other) => compareTo(other) <= 0;

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is FirmwareVersion &&
          major == other.major &&
          minor == other.minor &&
          patch == other.patch;

  @override
  int get hashCode => Object.hash(major, minor, patch);
}

/// Firmware release information
class FirmwareRelease {
  final String id;
  final FirmwareVersion version;
  final String deviceType;
  final DateTime releaseDate;
  final String? releaseNotes;
  final String downloadUrl;
  final int fileSize;
  final String checksum;
  final bool isStable;
  final bool isCritical;
  final FirmwareVersion? minBootloaderVersion;
  final FirmwareVersion? minSoftdeviceVersion;
  final List<FirmwareVersion>? incompatibleVersions;

  const FirmwareRelease({
    required this.id,
    required this.version,
    required this.deviceType,
    required this.releaseDate,
    this.releaseNotes,
    required this.downloadUrl,
    required this.fileSize,
    required this.checksum,
    this.isStable = true,
    this.isCritical = false,
    this.minBootloaderVersion,
    this.minSoftdeviceVersion,
    this.incompatibleVersions,
  });

  factory FirmwareRelease.fromJson(Map<String, dynamic> json) {
    return FirmwareRelease(
      id: json['id'] as String,
      version: FirmwareVersion.parse(json['version'] as String),
      deviceType: json['device_type'] as String,
      releaseDate: DateTime.parse(json['release_date'] as String),
      releaseNotes: json['release_notes'] as String?,
      downloadUrl: json['download_url'] as String,
      fileSize: json['file_size'] as int,
      checksum: json['checksum'] as String,
      isStable: json['is_stable'] as bool? ?? true,
      isCritical: json['is_critical'] as bool? ?? false,
      minBootloaderVersion: json['min_bootloader_version'] != null
          ? FirmwareVersion.parse(json['min_bootloader_version'] as String)
          : null,
      minSoftdeviceVersion: json['min_softdevice_version'] != null
          ? FirmwareVersion.parse(json['min_softdevice_version'] as String)
          : null,
      incompatibleVersions: (json['incompatible_versions'] as List<dynamic>?)
          ?.map((v) => FirmwareVersion.parse(v as String))
          .toList(),
    );
  }

  Map<String, dynamic> toJson() => {
        'id': id,
        'version': version.toString(),
        'device_type': deviceType,
        'release_date': releaseDate.toIso8601String(),
        'release_notes': releaseNotes,
        'download_url': downloadUrl,
        'file_size': fileSize,
        'checksum': checksum,
        'is_stable': isStable,
        'is_critical': isCritical,
        'min_bootloader_version': minBootloaderVersion?.toString(),
        'min_softdevice_version': minSoftdeviceVersion?.toString(),
        'incompatible_versions':
            incompatibleVersions?.map((v) => v.toString()).toList(),
      };

  /// Check if this release can be installed on a device with the given current version
  UpgradeCompatibility checkCompatibility({
    required FirmwareVersion currentVersion,
    FirmwareVersion? bootloaderVersion,
    FirmwareVersion? softdeviceVersion,
  }) {
    final issues = <String>[];
    var canInstall = true;

    // Check bootloader compatibility
    if (minBootloaderVersion != null && bootloaderVersion != null) {
      if (bootloaderVersion < minBootloaderVersion!) {
        issues.add(
          'Requires bootloader ${minBootloaderVersion} or newer '
          '(current: $bootloaderVersion)',
        );
        canInstall = false;
      }
    }

    // Check softdevice compatibility
    if (minSoftdeviceVersion != null && softdeviceVersion != null) {
      if (softdeviceVersion < minSoftdeviceVersion!) {
        issues.add(
          'Requires SoftDevice ${minSoftdeviceVersion} or newer '
          '(current: $softdeviceVersion)',
        );
        canInstall = false;
      }
    }

    // Check for known incompatible versions
    if (incompatibleVersions != null &&
        incompatibleVersions!.contains(currentVersion)) {
      issues.add(
        'Cannot upgrade directly from version $currentVersion. '
        'Please upgrade to an intermediate version first.',
      );
      canInstall = false;
    }

    // Determine upgrade type
    UpgradeType upgradeType;
    if (version > currentVersion) {
      upgradeType = UpgradeType.upgrade;
    } else if (version < currentVersion) {
      upgradeType = UpgradeType.downgrade;
    } else {
      upgradeType = UpgradeType.reinstall;
    }

    return UpgradeCompatibility(
      canInstall: canInstall,
      upgradeType: upgradeType,
      issues: issues,
      requiresBootloaderUpdate: minBootloaderVersion != null &&
          bootloaderVersion != null &&
          bootloaderVersion < minBootloaderVersion!,
    );
  }
}

/// Type of firmware change
enum UpgradeType {
  upgrade,
  downgrade,
  reinstall,
}

/// Result of compatibility check
class UpgradeCompatibility {
  final bool canInstall;
  final UpgradeType upgradeType;
  final List<String> issues;
  final bool requiresBootloaderUpdate;

  const UpgradeCompatibility({
    required this.canInstall,
    required this.upgradeType,
    required this.issues,
    this.requiresBootloaderUpdate = false,
  });

  String get upgradeTypeLabel {
    switch (upgradeType) {
      case UpgradeType.upgrade:
        return 'Upgrade';
      case UpgradeType.downgrade:
        return 'Downgrade';
      case UpgradeType.reinstall:
        return 'Reinstall';
    }
  }
}
