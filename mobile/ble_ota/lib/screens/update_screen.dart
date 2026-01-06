import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../providers/dfu_provider.dart';

class UpdateScreen extends StatefulWidget {
  final String deviceId;

  const UpdateScreen({
    super.key,
    required this.deviceId,
  });

  @override
  State<UpdateScreen> createState() => _UpdateScreenState();
}

class _UpdateScreenState extends State<UpdateScreen> {
  bool _updateStarted = false;

  @override
  void initState() {
    super.initState();
    // Reset DFU state and start update
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final dfuProvider = context.read<DfuProvider>();
      dfuProvider.reset();
      _startUpdate();
    });
  }

  Future<void> _startUpdate() async {
    if (_updateStarted) return;
    _updateStarted = true;

    final dfuProvider = context.read<DfuProvider>();
    await dfuProvider.startDfu(widget.deviceId);
  }

  @override
  Widget build(BuildContext context) {
    return WillPopScope(
      onWillPop: () async {
        final dfuProvider = context.read<DfuProvider>();
        if (dfuProvider.isUpdating) {
          final shouldAbort = await showDialog<bool>(
            context: context,
            builder: (context) => AlertDialog(
              title: const Text('Abort Update?'),
              content: const Text(
                'A firmware update is in progress. '
                'Aborting may leave the device in an inconsistent state.\n\n'
                'Are you sure you want to abort?',
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.pop(context, false),
                  child: const Text('Continue Update'),
                ),
                FilledButton(
                  onPressed: () => Navigator.pop(context, true),
                  style: FilledButton.styleFrom(
                    backgroundColor: Colors.red,
                  ),
                  child: const Text('Abort'),
                ),
              ],
            ),
          );

          if (shouldAbort == true) {
            await dfuProvider.abortDfu();
            return true;
          }
          return false;
        }
        return true;
      },
      child: Scaffold(
        appBar: AppBar(
          title: const Text('Firmware Update'),
          automaticallyImplyLeading: false,
        ),
        body: Consumer<DfuProvider>(
          builder: (context, dfuProvider, _) {
            return Padding(
              padding: const EdgeInsets.all(24.0),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  // Status icon
                  _buildStatusIcon(dfuProvider),
                  const SizedBox(height: 32),

                  // Status text
                  _buildStatusText(dfuProvider),
                  const SizedBox(height: 24),

                  // Progress indicator
                  if (dfuProvider.isUpdating || 
                      dfuProvider.state == DfuState.completed)
                    _buildProgressSection(dfuProvider),

                  const SizedBox(height: 32),

                  // Error message
                  if (dfuProvider.error != null)
                    _buildErrorMessage(dfuProvider),

                  const Spacer(),

                  // Action buttons
                  _buildActionButtons(context, dfuProvider),
                ],
              ),
            );
          },
        ),
      ),
    );
  }

  Widget _buildStatusIcon(DfuProvider dfuProvider) {
    IconData icon;
    Color color;
    double size = 80;

    switch (dfuProvider.state) {
      case DfuState.idle:
      case DfuState.preparing:
        icon = Icons.hourglass_empty;
        color = Colors.grey;
        break;
      case DfuState.connecting:
        icon = Icons.bluetooth_searching;
        color = Colors.blue;
        break;
      case DfuState.starting:
      case DfuState.uploading:
        icon = Icons.cloud_upload;
        color = Colors.orange;
        break;
      case DfuState.validating:
        icon = Icons.verified;
        color = Colors.blue;
        break;
      case DfuState.completed:
        icon = Icons.check_circle;
        color = Colors.green;
        break;
      case DfuState.aborted:
        icon = Icons.cancel;
        color = Colors.orange;
        break;
      case DfuState.error:
        icon = Icons.error;
        color = Colors.red;
        break;
    }

    Widget iconWidget = Icon(icon, size: size, color: color);

    // Add animation for uploading state
    if (dfuProvider.state == DfuState.uploading ||
        dfuProvider.state == DfuState.connecting ||
        dfuProvider.state == DfuState.starting) {
      iconWidget = Stack(
        alignment: Alignment.center,
        children: [
          SizedBox(
            width: size + 20,
            height: size + 20,
            child: CircularProgressIndicator(
              strokeWidth: 3,
              color: color.withOpacity(0.3),
            ),
          ),
          iconWidget,
        ],
      );
    }

    return iconWidget;
  }

  Widget _buildStatusText(DfuProvider dfuProvider) {
    String title;
    String? subtitle;

    switch (dfuProvider.state) {
      case DfuState.idle:
        title = 'Preparing...';
        break;
      case DfuState.preparing:
        title = 'Preparing Update';
        subtitle = 'Reading firmware file...';
        break;
      case DfuState.connecting:
        title = 'Connecting';
        subtitle = 'Establishing BLE connection...';
        break;
      case DfuState.starting:
        title = 'Starting DFU';
        subtitle = 'Entering bootloader mode...';
        break;
      case DfuState.uploading:
        title = 'Uploading Firmware';
        subtitle = 'Part ${dfuProvider.currentPart} of ${dfuProvider.totalParts}';
        break;
      case DfuState.validating:
        title = 'Validating';
        subtitle = 'Verifying firmware integrity...';
        break;
      case DfuState.completed:
        title = 'Update Complete!';
        subtitle = 'Device will restart with new firmware';
        break;
      case DfuState.aborted:
        title = 'Update Aborted';
        subtitle = 'The update was cancelled';
        break;
      case DfuState.error:
        title = 'Update Failed';
        subtitle = 'An error occurred during the update';
        break;
    }

    return Column(
      children: [
        Text(
          title,
          style: const TextStyle(
            fontSize: 24,
            fontWeight: FontWeight.bold,
          ),
          textAlign: TextAlign.center,
        ),
        if (subtitle != null) ...[
          const SizedBox(height: 8),
          Text(
            subtitle,
            style: const TextStyle(
              fontSize: 16,
              color: Colors.grey,
            ),
            textAlign: TextAlign.center,
          ),
        ],
      ],
    );
  }

  Widget _buildProgressSection(DfuProvider dfuProvider) {
    return Column(
      children: [
        // Progress bar
        ClipRRect(
          borderRadius: BorderRadius.circular(8),
          child: LinearProgressIndicator(
            value: dfuProvider.progress / 100,
            minHeight: 12,
            backgroundColor: Colors.grey.shade200,
          ),
        ),
        const SizedBox(height: 12),

        // Progress percentage
        Text(
          '${dfuProvider.progress}%',
          style: const TextStyle(
            fontSize: 32,
            fontWeight: FontWeight.bold,
          ),
        ),

        // Speed info
        if (dfuProvider.state == DfuState.uploading && dfuProvider.speed > 0)
          Padding(
            padding: const EdgeInsets.only(top: 8),
            child: Text(
              '${dfuProvider.speed.toStringAsFixed(1)} KB/s '
              '(avg: ${dfuProvider.avgSpeed} KB/s)',
              style: const TextStyle(color: Colors.grey),
            ),
          ),
      ],
    );
  }

  Widget _buildErrorMessage(DfuProvider dfuProvider) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: Colors.red.shade50,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: Colors.red.shade200),
      ),
      child: Row(
        children: [
          const Icon(Icons.error_outline, color: Colors.red),
          const SizedBox(width: 12),
          Expanded(
            child: Text(
              dfuProvider.error!,
              style: const TextStyle(color: Colors.red),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildActionButtons(BuildContext context, DfuProvider dfuProvider) {
    if (dfuProvider.state == DfuState.completed) {
      return FilledButton.icon(
        onPressed: () {
          dfuProvider.reset();
          Navigator.of(context).popUntil((route) => route.isFirst);
        },
        icon: const Icon(Icons.check),
        label: const Text('Done'),
        style: FilledButton.styleFrom(
          backgroundColor: Colors.green,
          minimumSize: const Size(double.infinity, 48),
        ),
      );
    }

    if (dfuProvider.state == DfuState.error ||
        dfuProvider.state == DfuState.aborted) {
      return Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          FilledButton.icon(
            onPressed: () {
              dfuProvider.reset();
              _updateStarted = false;
              _startUpdate();
            },
            icon: const Icon(Icons.refresh),
            label: const Text('Retry'),
          ),
          const SizedBox(height: 12),
          OutlinedButton(
            onPressed: () {
              dfuProvider.reset();
              Navigator.pop(context);
            },
            child: const Text('Cancel'),
          ),
        ],
      );
    }

    // Update in progress - show abort button
    if (dfuProvider.isUpdating) {
      return OutlinedButton.icon(
        onPressed: () async {
          final shouldAbort = await showDialog<bool>(
            context: context,
            builder: (context) => AlertDialog(
              title: const Text('Abort Update?'),
              content: const Text(
                'Aborting may leave the device in an inconsistent state.',
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.pop(context, false),
                  child: const Text('Continue'),
                ),
                FilledButton(
                  onPressed: () => Navigator.pop(context, true),
                  style: FilledButton.styleFrom(backgroundColor: Colors.red),
                  child: const Text('Abort'),
                ),
              ],
            ),
          );

          if (shouldAbort == true) {
            await dfuProvider.abortDfu();
          }
        },
        icon: const Icon(Icons.cancel),
        label: const Text('Abort Update'),
        style: OutlinedButton.styleFrom(
          foregroundColor: Colors.red,
        ),
      );
    }

    return const SizedBox.shrink();
  }
}
