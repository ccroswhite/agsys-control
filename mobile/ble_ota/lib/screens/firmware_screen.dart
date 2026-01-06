import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/firmware_version.dart';
import '../providers/firmware_provider.dart';
import '../providers/ble_provider.dart';

class FirmwareScreen extends StatefulWidget {
  final String? currentVersionString;

  const FirmwareScreen({
    super.key,
    this.currentVersionString,
  });

  @override
  State<FirmwareScreen> createState() => _FirmwareScreenState();
}

class _FirmwareScreenState extends State<FirmwareScreen> {
  bool _showAllVersions = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      context.read<FirmwareProvider>().fetchReleases();
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Select Firmware'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: () => context.read<FirmwareProvider>().fetchReleases(),
          ),
        ],
      ),
      body: Consumer<FirmwareProvider>(
        builder: (context, provider, _) {
          if (provider.isLoading) {
            return const Center(child: CircularProgressIndicator());
          }

          if (provider.error != null) {
            return _buildErrorState(provider);
          }

          return _buildContent(context, provider);
        },
      ),
    );
  }

  Widget _buildErrorState(FirmwareProvider provider) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32.0),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(Icons.cloud_off, size: 64, color: Colors.grey),
            const SizedBox(height: 16),
            Text(
              provider.error!,
              textAlign: TextAlign.center,
              style: const TextStyle(color: Colors.grey),
            ),
            const SizedBox(height: 24),
            if (provider.cachedFirmware.isNotEmpty) ...[
              const Text(
                'Cached firmware available:',
                style: TextStyle(fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 16),
              ...provider.cachedFirmware.map(
                (cached) => _buildCachedTile(context, cached),
              ),
            ],
            const SizedBox(height: 24),
            FilledButton.icon(
              onPressed: () => provider.fetchReleases(),
              icon: const Icon(Icons.refresh),
              label: const Text('Retry'),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildContent(BuildContext context, FirmwareProvider provider) {
    final currentVersion = widget.currentVersionString != null
        ? FirmwareVersion.parse(widget.currentVersionString!)
        : null;

    final recommendations = currentVersion != null
        ? provider.getRecommendedVersions(currentVersion)
        : <RecommendedFirmware>[];

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        // Current version info
        if (currentVersion != null) _buildCurrentVersionCard(currentVersion),

        // Recommended versions
        if (recommendations.isNotEmpty) ...[
          const SizedBox(height: 16),
          _buildSectionHeader('Recommended'),
          ...recommendations.map(
            (rec) => _buildRecommendedTile(context, rec, currentVersion),
          ),
        ],

        // All versions toggle
        const SizedBox(height: 24),
        Row(
          children: [
            _buildSectionHeader('All Versions'),
            const Spacer(),
            TextButton(
              onPressed: () => setState(() => _showAllVersions = !_showAllVersions),
              child: Text(_showAllVersions ? 'Hide' : 'Show All'),
            ),
          ],
        ),

        if (_showAllVersions) ...[
          ...provider.availableReleases.map(
            (release) => _buildReleaseTile(context, release, currentVersion),
          ),
        ],

        // Cached versions
        if (provider.cachedFirmware.isNotEmpty) ...[
          const SizedBox(height: 24),
          _buildSectionHeader('Downloaded (Available Offline)'),
          ...provider.cachedFirmware.map(
            (cached) => _buildCachedTile(context, cached),
          ),
        ],

        const SizedBox(height: 32),
      ],
    );
  }

  Widget _buildCurrentVersionCard(FirmwareVersion version) {
    return Card(
      color: Theme.of(context).colorScheme.primaryContainer,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Row(
          children: [
            const Icon(Icons.info_outline),
            const SizedBox(width: 12),
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'Current Device Version',
                  style: TextStyle(fontSize: 12),
                ),
                Text(
                  'v$version',
                  style: const TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildSectionHeader(String title) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 8),
      child: Text(
        title,
        style: const TextStyle(
          fontSize: 14,
          fontWeight: FontWeight.bold,
          color: Colors.grey,
        ),
      ),
    );
  }

  Widget _buildRecommendedTile(
    BuildContext context,
    RecommendedFirmware rec,
    FirmwareVersion? currentVersion,
  ) {
    final release = rec.release;
    final compatibility = currentVersion != null
        ? release.checkCompatibility(currentVersion: currentVersion)
        : null;

    Color? badgeColor;
    IconData badgeIcon;
    switch (rec.reason) {
      case RecommendReason.latest:
        badgeColor = Colors.green;
        badgeIcon = Icons.arrow_upward;
        break;
      case RecommendReason.rollback:
        badgeColor = Colors.orange;
        badgeIcon = Icons.arrow_downward;
        break;
      case RecommendReason.reinstall:
        badgeColor = Colors.blue;
        badgeIcon = Icons.refresh;
        break;
      case RecommendReason.critical:
        badgeColor = Colors.red;
        badgeIcon = Icons.warning;
        break;
    }

    return Card(
      child: ListTile(
        leading: CircleAvatar(
          backgroundColor: badgeColor?.withOpacity(0.2),
          child: Icon(badgeIcon, color: badgeColor),
        ),
        title: Row(
          children: [
            Text(
              'v${release.version}',
              style: const TextStyle(fontWeight: FontWeight.bold),
            ),
            const SizedBox(width: 8),
            if (rec.isCached)
              const Icon(Icons.download_done, size: 16, color: Colors.green),
          ],
        ),
        subtitle: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(rec.reasonLabel),
            if (compatibility != null && !compatibility.canInstall)
              Text(
                compatibility.issues.first,
                style: const TextStyle(color: Colors.red, fontSize: 12),
              ),
          ],
        ),
        trailing: const Icon(Icons.chevron_right),
        onTap: compatibility?.canInstall != false
            ? () => _selectFirmware(context, release)
            : null,
      ),
    );
  }

  Widget _buildReleaseTile(
    BuildContext context,
    FirmwareRelease release,
    FirmwareVersion? currentVersion,
  ) {
    final provider = context.read<FirmwareProvider>();
    final isCached = provider.isCached(release.version);
    final compatibility = currentVersion != null
        ? release.checkCompatibility(currentVersion: currentVersion)
        : null;

    return Card(
      child: ListTile(
        leading: CircleAvatar(
          backgroundColor: release.isStable
              ? Colors.green.withOpacity(0.2)
              : Colors.orange.withOpacity(0.2),
          child: Icon(
            release.isStable ? Icons.verified : Icons.science,
            color: release.isStable ? Colors.green : Colors.orange,
          ),
        ),
        title: Row(
          children: [
            Text('v${release.version}'),
            const SizedBox(width: 8),
            if (!release.isStable)
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                decoration: BoxDecoration(
                  color: Colors.orange.withOpacity(0.2),
                  borderRadius: BorderRadius.circular(4),
                ),
                child: const Text(
                  'Beta',
                  style: TextStyle(fontSize: 10, color: Colors.orange),
                ),
              ),
            if (isCached)
              const Padding(
                padding: EdgeInsets.only(left: 8),
                child: Icon(Icons.download_done, size: 16, color: Colors.green),
              ),
          ],
        ),
        subtitle: Text(
          _formatDate(release.releaseDate),
          style: const TextStyle(fontSize: 12),
        ),
        trailing: compatibility != null
            ? Text(
                compatibility.upgradeTypeLabel,
                style: TextStyle(
                  color: compatibility.upgradeType == UpgradeType.downgrade
                      ? Colors.orange
                      : Colors.green,
                  fontSize: 12,
                ),
              )
            : null,
        onTap: compatibility?.canInstall != false
            ? () => _selectFirmware(context, release)
            : null,
      ),
    );
  }

  Widget _buildCachedTile(BuildContext context, CachedFirmware cached) {
    return Card(
      child: ListTile(
        leading: const CircleAvatar(
          backgroundColor: Colors.green,
          child: Icon(Icons.folder, color: Colors.white),
        ),
        title: Text('v${cached.release.version}'),
        subtitle: Text(
          'Downloaded ${_formatDate(cached.downloadedAt)}',
          style: const TextStyle(fontSize: 12),
        ),
        trailing: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            IconButton(
              icon: const Icon(Icons.delete_outline),
              onPressed: () => _confirmDelete(context, cached),
            ),
            const Icon(Icons.chevron_right),
          ],
        ),
        onTap: () => _selectCachedFirmware(context, cached),
      ),
    );
  }

  String _formatDate(DateTime date) {
    return '${date.year}-${date.month.toString().padLeft(2, '0')}-${date.day.toString().padLeft(2, '0')}';
  }

  Future<void> _selectFirmware(BuildContext context, FirmwareRelease release) async {
    final provider = context.read<FirmwareProvider>();

    // Show confirmation dialog
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => _FirmwareConfirmDialog(release: release),
    );

    if (confirmed != true || !context.mounted) return;

    // Download if not cached
    if (!provider.isCached(release.version)) {
      final downloaded = await showDialog<bool>(
        context: context,
        barrierDismissible: false,
        builder: (context) => _DownloadDialog(release: release),
      );

      if (downloaded != true || !context.mounted) return;
    }

    // Get cached firmware and return
    final cached = provider.getCachedFirmware(release.version);
    if (cached != null && context.mounted) {
      Navigator.pop(context, cached);
    }
  }

  void _selectCachedFirmware(BuildContext context, CachedFirmware cached) {
    Navigator.pop(context, cached);
  }

  Future<void> _confirmDelete(BuildContext context, CachedFirmware cached) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Delete Cached Firmware?'),
        content: Text(
          'Delete v${cached.release.version} from local storage?\n\n'
          'You can download it again later.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            style: FilledButton.styleFrom(backgroundColor: Colors.red),
            child: const Text('Delete'),
          ),
        ],
      ),
    );

    if (confirmed == true && context.mounted) {
      await context.read<FirmwareProvider>().removeCachedFirmware(cached.release.version);
    }
  }
}

class _FirmwareConfirmDialog extends StatelessWidget {
  final FirmwareRelease release;

  const _FirmwareConfirmDialog({required this.release});

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: Text('Install v${release.version}?'),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _buildInfoRow('Released', _formatDate(release.releaseDate)),
          _buildInfoRow('Size', '${(release.fileSize / 1024).round()} KB'),
          _buildInfoRow('Type', release.isStable ? 'Stable' : 'Beta'),
          if (release.releaseNotes != null) ...[
            const SizedBox(height: 16),
            const Text(
              'Release Notes:',
              style: TextStyle(fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 8),
            Container(
              padding: const EdgeInsets.all(8),
              decoration: BoxDecoration(
                color: Colors.grey.shade100,
                borderRadius: BorderRadius.circular(4),
              ),
              constraints: const BoxConstraints(maxHeight: 150),
              child: SingleChildScrollView(
                child: Text(
                  release.releaseNotes!,
                  style: const TextStyle(fontSize: 12),
                ),
              ),
            ),
          ],
          if (release.isCritical) ...[
            const SizedBox(height: 16),
            Container(
              padding: const EdgeInsets.all(8),
              decoration: BoxDecoration(
                color: Colors.red.shade50,
                borderRadius: BorderRadius.circular(4),
                border: Border.all(color: Colors.red.shade200),
              ),
              child: const Row(
                children: [
                  Icon(Icons.warning, color: Colors.red, size: 20),
                  SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      'This is a critical security update.',
                      style: TextStyle(color: Colors.red, fontSize: 12),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ],
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context, false),
          child: const Text('Cancel'),
        ),
        FilledButton(
          onPressed: () => Navigator.pop(context, true),
          child: const Text('Select'),
        ),
      ],
    );
  }

  Widget _buildInfoRow(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        children: [
          SizedBox(
            width: 80,
            child: Text(
              label,
              style: const TextStyle(color: Colors.grey),
            ),
          ),
          Text(value),
        ],
      ),
    );
  }

  String _formatDate(DateTime date) {
    return '${date.year}-${date.month.toString().padLeft(2, '0')}-${date.day.toString().padLeft(2, '0')}';
  }
}

class _DownloadDialog extends StatefulWidget {
  final FirmwareRelease release;

  const _DownloadDialog({required this.release});

  @override
  State<_DownloadDialog> createState() => _DownloadDialogState();
}

class _DownloadDialogState extends State<_DownloadDialog> {
  int _progress = 0;
  String? _error;
  bool _downloading = true;

  @override
  void initState() {
    super.initState();
    _startDownload();
  }

  Future<void> _startDownload() async {
    try {
      await context.read<FirmwareProvider>().downloadFirmware(
        widget.release,
        onProgress: (received, total) {
          setState(() {
            _progress = (received * 100 / total).round();
          });
        },
      );
      if (mounted) {
        Navigator.pop(context, true);
      }
    } catch (e) {
      setState(() {
        _error = e.toString();
        _downloading = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('Downloading Firmware'),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          if (_downloading) ...[
            LinearProgressIndicator(value: _progress / 100),
            const SizedBox(height: 16),
            Text('$_progress%'),
          ],
          if (_error != null) ...[
            const Icon(Icons.error, color: Colors.red, size: 48),
            const SizedBox(height: 16),
            Text(_error!, style: const TextStyle(color: Colors.red)),
          ],
        ],
      ),
      actions: [
        if (_error != null) ...[
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () {
              setState(() {
                _error = null;
                _downloading = true;
                _progress = 0;
              });
              _startDownload();
            },
            child: const Text('Retry'),
          ),
        ],
      ],
    );
  }
}
