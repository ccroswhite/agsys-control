import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../providers/ble_provider.dart';
import '../providers/dfu_provider.dart';
import 'scan_screen.dart';
import 'update_screen.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('AgSys BLE OTA'),
        actions: [
          IconButton(
            icon: const Icon(Icons.info_outline),
            onPressed: () => _showAboutDialog(context),
          ),
        ],
      ),
      body: Consumer2<BleProvider, DfuProvider>(
        builder: (context, bleProvider, dfuProvider, _) {
          // Show error if any
          if (bleProvider.error != null) {
            WidgetsBinding.instance.addPostFrameCallback((_) {
              _showError(context, bleProvider.error!);
              bleProvider.clearError();
            });
          }

          return Padding(
            padding: const EdgeInsets.all(16.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // Status card
                _buildStatusCard(context, bleProvider, dfuProvider),
                const SizedBox(height: 24),

                // Selected device card
                if (bleProvider.selectedDevice != null)
                  _buildDeviceCard(context, bleProvider),

                const SizedBox(height: 24),

                // Firmware card
                if (dfuProvider.firmwarePath != null)
                  _buildFirmwareCard(context, dfuProvider),

                const Spacer(),

                // Action buttons
                _buildActionButtons(context, bleProvider, dfuProvider),
              ],
            ),
          );
        },
      ),
    );
  }

  Widget _buildStatusCard(
    BuildContext context,
    BleProvider bleProvider,
    DfuProvider dfuProvider,
  ) {
    final theme = Theme.of(context);
    
    IconData icon;
    String status;
    Color color;

    if (dfuProvider.isUpdating) {
      icon = Icons.system_update;
      status = 'Update in progress...';
      color = Colors.orange;
    } else if (bleProvider.isConnected) {
      icon = Icons.bluetooth_connected;
      status = 'Connected';
      color = Colors.green;
    } else if (bleProvider.selectedDevice != null) {
      icon = Icons.bluetooth;
      status = 'Device selected';
      color = theme.colorScheme.primary;
    } else {
      icon = Icons.bluetooth_searching;
      status = 'No device selected';
      color = Colors.grey;
    }

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Row(
          children: [
            Icon(icon, size: 48, color: color),
            const SizedBox(width: 16),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    status,
                    style: theme.textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                  if (bleProvider.state != BleState.on)
                    Text(
                      'Bluetooth: ${bleProvider.state.name}',
                      style: theme.textTheme.bodySmall?.copyWith(
                        color: Colors.red,
                      ),
                    ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildDeviceCard(BuildContext context, BleProvider bleProvider) {
    final device = bleProvider.selectedDevice!;
    final theme = Theme.of(context);

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                const Icon(Icons.memory, size: 24),
                const SizedBox(width: 8),
                Text(
                  'Selected Device',
                  style: theme.textTheme.titleSmall?.copyWith(
                    color: Colors.grey,
                  ),
                ),
                const Spacer(),
                IconButton(
                  icon: const Icon(Icons.close, size: 20),
                  onPressed: () {
                    bleProvider.disconnect();
                    bleProvider.clearSelection();
                  },
                ),
              ],
            ),
            const SizedBox(height: 8),
            Text(
              device.name,
              style: theme.textTheme.titleLarge?.copyWith(
                fontWeight: FontWeight.bold,
              ),
            ),
            const SizedBox(height: 4),
            Text(
              'ID: ${device.id}',
              style: theme.textTheme.bodySmall,
            ),
            if (device.uuid != null)
              Text(
                'UUID: ${device.uuid}',
                style: theme.textTheme.bodySmall,
              ),
            const SizedBox(height: 8),
            Row(
              children: [
                Icon(
                  Icons.signal_cellular_alt,
                  size: 16,
                  color: _rssiColor(device.rssi),
                ),
                const SizedBox(width: 4),
                Text(
                  '${device.rssi} dBm',
                  style: theme.textTheme.bodySmall,
                ),
                const Spacer(),
                if (bleProvider.isConnected)
                  Chip(
                    label: const Text('Connected'),
                    backgroundColor: Colors.green.withOpacity(0.2),
                    labelStyle: const TextStyle(color: Colors.green),
                  )
                else if (bleProvider.isConnecting)
                  const SizedBox(
                    width: 16,
                    height: 16,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildFirmwareCard(BuildContext context, DfuProvider dfuProvider) {
    final theme = Theme.of(context);
    final fileName = dfuProvider.firmwarePath!.split('/').last;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                const Icon(Icons.folder_zip, size: 24),
                const SizedBox(width: 8),
                Text(
                  'Firmware',
                  style: theme.textTheme.titleSmall?.copyWith(
                    color: Colors.grey,
                  ),
                ),
                const Spacer(),
                IconButton(
                  icon: const Icon(Icons.close, size: 20),
                  onPressed: dfuProvider.clearFirmware,
                ),
              ],
            ),
            const SizedBox(height: 8),
            Text(
              fileName,
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.bold,
              ),
            ),
            if (dfuProvider.firmwareVersion != null)
              Text(
                'Version: ${dfuProvider.firmwareVersion}',
                style: theme.textTheme.bodySmall,
              ),
          ],
        ),
      ),
    );
  }

  Widget _buildActionButtons(
    BuildContext context,
    BleProvider bleProvider,
    DfuProvider dfuProvider,
  ) {
    final canStartUpdate = bleProvider.selectedDevice != null &&
        dfuProvider.firmwarePath != null &&
        !dfuProvider.isUpdating;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        // Scan button
        if (bleProvider.selectedDevice == null)
          FilledButton.icon(
            onPressed: bleProvider.state == BleState.on
                ? () => Navigator.push(
                      context,
                      MaterialPageRoute(builder: (_) => const ScanScreen()),
                    )
                : null,
            icon: const Icon(Icons.bluetooth_searching),
            label: const Text('Scan for Devices'),
          ),

        // Connect button
        if (bleProvider.selectedDevice != null && !bleProvider.isConnected)
          FilledButton.icon(
            onPressed: bleProvider.isConnecting ? null : bleProvider.connect,
            icon: bleProvider.isConnecting
                ? const SizedBox(
                    width: 20,
                    height: 20,
                    child: CircularProgressIndicator(
                      strokeWidth: 2,
                      color: Colors.white,
                    ),
                  )
                : const Icon(Icons.bluetooth_connected),
            label: Text(bleProvider.isConnecting ? 'Connecting...' : 'Connect'),
          ),

        const SizedBox(height: 12),

        // Start update button
        if (canStartUpdate)
          FilledButton.icon(
            onPressed: () => Navigator.push(
              context,
              MaterialPageRoute(
                builder: (_) => UpdateScreen(
                  deviceId: bleProvider.selectedDevice!.id,
                ),
              ),
            ),
            icon: const Icon(Icons.system_update),
            label: const Text('Start Firmware Update'),
            style: FilledButton.styleFrom(
              backgroundColor: Colors.green,
            ),
          ),
      ],
    );
  }

  Color _rssiColor(int rssi) {
    if (rssi >= -60) return Colors.green;
    if (rssi >= -80) return Colors.orange;
    return Colors.red;
  }

  void _showError(BuildContext context, String message) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(message),
        backgroundColor: Colors.red,
      ),
    );
  }

  void _showAboutDialog(BuildContext context) {
    showAboutDialog(
      context: context,
      applicationName: 'AgSys BLE OTA',
      applicationVersion: '1.0.0',
      applicationLegalese: '© 2024 AgSys',
      children: [
        const SizedBox(height: 16),
        const Text(
          'Bluetooth firmware update utility for AgSys IoT devices.\n\n'
          'Use this app to update device firmware via BLE when LoRa OTA is not available.',
        ),
      ],
    );
  }
}
