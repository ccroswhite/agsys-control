import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:file_picker/file_picker.dart';

import '../providers/ble_provider.dart';
import '../providers/dfu_provider.dart';

class ScanScreen extends StatefulWidget {
  const ScanScreen({super.key});

  @override
  State<ScanScreen> createState() => _ScanScreenState();
}

class _ScanScreenState extends State<ScanScreen> {
  @override
  void initState() {
    super.initState();
    // Start scanning when screen opens
    WidgetsBinding.instance.addPostFrameCallback((_) {
      context.read<BleProvider>().startScan();
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Select Device'),
        actions: [
          Consumer<BleProvider>(
            builder: (context, provider, _) {
              if (provider.isScanning) {
                return const Padding(
                  padding: EdgeInsets.all(16.0),
                  child: SizedBox(
                    width: 20,
                    height: 20,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  ),
                );
              }
              return IconButton(
                icon: const Icon(Icons.refresh),
                onPressed: () => provider.startScan(),
              );
            },
          ),
        ],
      ),
      body: Consumer<BleProvider>(
        builder: (context, provider, _) {
          if (provider.devices.isEmpty && !provider.isScanning) {
            return Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  const Icon(
                    Icons.bluetooth_disabled,
                    size: 64,
                    color: Colors.grey,
                  ),
                  const SizedBox(height: 16),
                  const Text(
                    'No AgSys devices found',
                    style: TextStyle(
                      fontSize: 18,
                      color: Colors.grey,
                    ),
                  ),
                  const SizedBox(height: 8),
                  const Text(
                    'Make sure the device is in OTA mode\n(press the OTA button on the device)',
                    textAlign: TextAlign.center,
                    style: TextStyle(color: Colors.grey),
                  ),
                  const SizedBox(height: 24),
                  FilledButton.icon(
                    onPressed: () => provider.startScan(),
                    icon: const Icon(Icons.refresh),
                    label: const Text('Scan Again'),
                  ),
                ],
              ),
            );
          }

          return Column(
            children: [
              if (provider.isScanning)
                const LinearProgressIndicator(),
              
              Expanded(
                child: ListView.builder(
                  itemCount: provider.devices.length,
                  itemBuilder: (context, index) {
                    final device = provider.devices[index];
                    return _DeviceListTile(
                      device: device,
                      onTap: () async {
                        provider.selectDevice(device);
                        
                        // Ask to select firmware
                        final shouldSelectFirmware = await showDialog<bool>(
                          context: context,
                          builder: (context) => AlertDialog(
                            title: const Text('Select Firmware?'),
                            content: Text(
                              'Device "${device.name}" selected.\n\n'
                              'Would you like to select a firmware file now?',
                            ),
                            actions: [
                              TextButton(
                                onPressed: () => Navigator.pop(context, false),
                                child: const Text('Later'),
                              ),
                              FilledButton(
                                onPressed: () => Navigator.pop(context, true),
                                child: const Text('Select Firmware'),
                              ),
                            ],
                          ),
                        );

                        if (shouldSelectFirmware == true && context.mounted) {
                          await _selectFirmware(context);
                        }

                        if (context.mounted) {
                          Navigator.pop(context);
                        }
                      },
                    );
                  },
                ),
              ),
            ],
          );
        },
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: () => _selectFirmware(context),
        icon: const Icon(Icons.folder_open),
        label: const Text('Select Firmware'),
      ),
    );
  }

  Future<void> _selectFirmware(BuildContext context) async {
    final result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['zip', 'bin'],
      dialogTitle: 'Select Firmware File',
    );

    if (result != null && result.files.single.path != null) {
      final dfuProvider = context.read<DfuProvider>();
      
      // Copy to app storage
      final storedPath = await dfuProvider.copyFirmwareToStorage(
        result.files.single.path!,
      );

      if (storedPath != null) {
        // Try to extract version from filename
        final fileName = result.files.single.name;
        String? version;
        final versionMatch = RegExp(r'v?(\d+\.\d+\.\d+)').firstMatch(fileName);
        if (versionMatch != null) {
          version = versionMatch.group(1);
        }

        dfuProvider.setFirmware(storedPath, version: version);

        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text('Firmware selected: ${result.files.single.name}'),
              backgroundColor: Colors.green,
            ),
          );
        }
      }
    }
  }
}

class _DeviceListTile extends StatelessWidget {
  final AgSysDevice device;
  final VoidCallback onTap;

  const _DeviceListTile({
    required this.device,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);

    return Card(
      margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
      child: ListTile(
        leading: Icon(
          Icons.memory,
          color: theme.colorScheme.primary,
          size: 32,
        ),
        title: Text(
          device.name,
          style: const TextStyle(fontWeight: FontWeight.bold),
        ),
        subtitle: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              device.id,
              style: theme.textTheme.bodySmall,
            ),
            if (device.uuid != null)
              Text(
                'UUID: ${device.uuid!.substring(0, 16)}...',
                style: theme.textTheme.bodySmall,
              ),
          ],
        ),
        trailing: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(
              Icons.signal_cellular_alt,
              color: _rssiColor(device.rssi),
              size: 20,
            ),
            const SizedBox(width: 4),
            Text(
              '${device.rssi}',
              style: theme.textTheme.bodySmall,
            ),
            const SizedBox(width: 8),
            const Icon(Icons.chevron_right),
          ],
        ),
        onTap: onTap,
      ),
    );
  }

  Color _rssiColor(int rssi) {
    if (rssi >= -60) return Colors.green;
    if (rssi >= -80) return Colors.orange;
    return Colors.red;
  }
}
