#ifndef MEASUREMENT_APP_H
#define MEASUREMENT_APP_H

#include "../ApplicationInterface.h"
#include <Arduino.h>
#include <vector>

// MeasurementApp
// Coordinates MPU data collection triggered by Bluetooth commands
// Stores MPU readings in memory on START, transmits compressed data on STOP
class MeasurementApp : public ApplicationInterface {
public:
    MeasurementApp();
    ~MeasurementApp();

    bool setup();
    void update();

    // State queries
    bool isRecording() const;
    size_t getRecordedSamplesCount() const;

private:
    // State
    bool initialized_;
    bool recording_;
    std::vector<float> recordedData_; // Stores [ax, ay, az, gx, gy, gz, ax, ay, ...]
    unsigned long recordingStartTime_;
    size_t sampleCount_;

    // Configuration
    static const size_t MAX_SAMPLES = 1000; // Max samples to store (prevents overflow)
    static const size_t VALUES_PER_SAMPLE = 6; // ax, ay, az, gx, gy, gz

    // Network callbacks
    void onBluetoothConnected(const uint8_t* data, size_t len, const std::string& topic);
    void onBluetoothDisconnected(const uint8_t* data, size_t len, const std::string& topic);
    void onBluetoothCommand(const uint8_t* data, size_t len, const std::string& topic);
    void onMpuData(const uint8_t* data, size_t len, const std::string& topic);

    // Command handlers
    void handleStartCommand();
    void handleStopCommand();
    void handleDataCommand();

    // Data transmission
    void transmitRecordedData();
    void compressAndTransmit();
    void clearRecordedData();

    // Helper methods
    void logRecordingStatus();
};

#endif // MEASUREMENT_APP_H
