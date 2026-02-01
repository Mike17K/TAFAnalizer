# Troubleshooting Guide

## Brownout Detector Triggered
**Symptom**: Continuous resets with message "Brownout detector was triggered"

**Causes**:
- Insufficient power supply
- Poor USB cable quality
- Too many peripherals active simultaneously
- Voltage drops during high current operations

**Solutions**:
1. ✅ Brownout detector already disabled in code (`main.cpp`)
2. Use 5V 2A or better power supply
3. Use short, high-quality USB cable (avoid long/thin cables)
4. Add 100µF capacitor across 5V and GND near ESP32
5. Disable unused peripherals (camera already disabled)

## Camera Errors (If Re-enabled)
**Symptom**: Camera initialization fails, system crashes

**Reminder**: Camera is intentionally DISABLED
- Do not uncomment camera code without addressing power issues first
- Camera draws significant current during initialization

## I2C Communication Failures
**Symptom**: MPU6050 not responding

**Check**:
- Correct pins: SDA=14, SCL=15
- Pull-up resistors on I2C lines (4.7kΩ)
- MPU6050 power connection
- I2C address (usually 0x68 or 0x69)

## Bluetooth Connection Issues
**Symptom**: Cannot pair or connect via Bluetooth

**Solutions**:
- Check if Bluetooth is initialized properly
- Verify device name is broadcasting
- Restart ESP32
- Clear paired devices on phone/computer

## Application Not Starting
**Symptom**: Application setup() returns false

**Debug Steps**:
1. Check Serial output for error messages
2. Verify NetworkLayer and DataLayer initialized first
3. Check nullptr after object creation
4. Verify sufficient heap memory available

## RTOS Task Crashes
**Symptom**: Task watchdog triggered, stack overflow

**Solutions**:
- Increase stack size in `createTask()` (try 8192 instead of 4096)
- Check for infinite loops without delays
- Verify no stack-heavy operations (large local arrays)
- Use heap allocation for large buffers

## Memory Issues
**Symptom**: Random crashes, heap allocation failures

**Check**:
- Total memory usage (all tasks + objects)
- Memory leaks (delete objects properly)
- Large stack allocations (move to heap)
- Fragment heap (use PSRAM if available)

## Upload/Monitor Issues
**Symptom**: Cannot upload code or see serial output

**Solutions**:
- Hold BOOT button during upload
- Check USB cable connection
- Verify correct port in platformio.ini
- Try different USB cable/port
- Reset ESP32 after upload
