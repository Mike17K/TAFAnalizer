# MeasurementApp

## Overview
Coordinates MPU6050 data collection triggered by Bluetooth commands. Records sensor data in memory and transmits it efficiently over Bluetooth.

## Features
- **Bluetooth Command Control**: START/STOP recording via Bluetooth commands
- **Memory Storage**: Stores up to 1000 MPU samples in RAM
- **Efficient Data Format**: Binary transmission (24 bytes per sample)
- **Auto-Stop**: Stops recording on Bluetooth disconnect or buffer full

## Network Topics

### Subscriptions
- `bluetooth/connected` - Clears data buffer on new connection
- `bluetooth/disconnected` - Stops recording if active
- `bluetooth/command` - Listens for START/STOP commands
- `mpu/data` - Receives MPU sensor readings (28 bytes: timestamp + 6 floats)

### Publications
- `capture/start` - Triggers MPU to start capturing
- `bluetooth/transmit` - Sends recorded data back via Bluetooth

## Bluetooth Commands
- `START` - Clear buffer and begin recording MPU data
- `STOP` - Stop recording and transmit all data

## Data Format

### Recording
Each sample stores 6 float values (24 bytes):
- ax, ay, az (accelerometer x, y, z)
- gx, gy, gz (gyroscope x, y, z)

### Transmission
```
DATA_START:<sample_count>\n
<binary data chunks>
DATA_END\n
```

Binary data: Raw floats transmitted in 240-byte chunks (10 samples per chunk)

## Configuration
- `MAX_SAMPLES`: 1000 samples max (~24KB)
- `VALUES_PER_SAMPLE`: 6 floats per reading
- Chunk size: 240 bytes for Bluetooth transmission

## Usage Example
1. Connect to ESP32 via Bluetooth (ESP32-CAM-TAF)
2. Send `START\n` to begin recording
3. Move/rotate the device (MPU captures at 100Hz)
4. Send `STOP\n` to receive all recorded data
5. Parse binary data: 6 consecutive floats per sample

## RTOS Task
- **Stack**: 8192 bytes (needs space for data buffer)
- **Priority**: 3 (highest - data collection critical)
- **Update Rate**: 10Hz (100ms delay)
