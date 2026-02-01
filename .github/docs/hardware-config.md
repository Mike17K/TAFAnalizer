# Hardware Configuration

## ESP32-CAM Pinout

### Camera (DISABLED)
- **Status**: Physically connected but SOFTWARE DISABLED
- **Code**: All camera initialization is commented out in `main.cpp`
- **Reason**: Power consumption issues
- **DO NOT RE-ENABLE** without explicit permission

### I2C (MPU6050)
- **SDA**: GPIO 14
- **SCL**: GPIO 15
- **Initialization**: `Wire.begin(14, 15)`

### LED
- **GPIO**: 2 (built-in LED)
- **Active**: LOW (inverse logic)

### Bluetooth
- **Type**: Classic Bluetooth (not BLE)
- **Stack**: ESP32 built-in

## Power Management

### Brownout Detector
- **Status**: DISABLED in software
- **Code**: `WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);`
- **Reason**: Prevents resets due to voltage fluctuations
- **Location**: `main.cpp` setup()

### Power Requirements
- **Minimum**: 5V 2A power supply recommended
- **Issues**: ESP32-CAM sensitive to power quality
- **Solutions**: 
  - Use short, thick USB cables
  - Add decoupling capacitors
  - Disable unused peripherals (camera disabled for this reason)

## GPIO Usage Summary
| GPIO | Function | Status |
|------|----------|--------|
| 2    | LED      | Active |
| 14   | I2C SDA  | Active |
| 15   | I2C SCL  | Active |
| Camera pins | Camera | DISABLED |

## Serial Communication
- **Baud Rate**: 115200
- **Purpose**: Debugging, logging
- **Initialization**: `Serial.begin(115200)` in setup()

## Notes
- Avoid using camera GPIO pins for other purposes (hardware conflict risk)
- GPIO 2 has pull-down resistor - good for LED
- I2C pins can support multiple devices on same bus
