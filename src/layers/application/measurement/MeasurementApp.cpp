#include "MeasurementApp.h"
#include <Arduino.h>

MeasurementApp::MeasurementApp()
    : initialized_(false),
      recording_(false),
      recordingStartTime_(0),
      sampleCount_(0) {
    Serial.println("[MeasurementApp] Created");
    recordedData_.reserve(MAX_SAMPLES * VALUES_PER_SAMPLE);
}

MeasurementApp::~MeasurementApp() {
    if (initialized_) {
        // Unsubscribe from topics
        networkLayer_->unsubscribe("bluetooth/connected", "MeasurementApp");
        networkLayer_->unsubscribe("bluetooth/disconnected", "MeasurementApp");
        networkLayer_->unsubscribe("bluetooth/command", "MeasurementApp");
        networkLayer_->unsubscribe("mpu/data", "MeasurementApp");
        Serial.println("[MeasurementApp] Cleaned up");
    }
}

bool MeasurementApp::setup() {
    if (!networkLayer_ || !dataLayer_) {
        Serial.println("[MeasurementApp] ERROR: Missing layer dependencies");
        return false;
    }

    // Subscribe to Bluetooth connection events
    auto connectedCallback = [this](const uint8_t* data, size_t len, const std::string& topic) {
        this->onBluetoothConnected(data, len, topic);
    };

    auto disconnectedCallback = [this](const uint8_t* data, size_t len, const std::string& topic) {
        this->onBluetoothDisconnected(data, len, topic);
    };

    auto commandCallback = [this](const uint8_t* data, size_t len, const std::string& topic) {
        this->onBluetoothCommand(data, len, topic);
    };

    auto mpuDataCallback = [this](const uint8_t* data, size_t len, const std::string& topic) {
        this->onMpuData(data, len, topic);
    };

    if (!networkLayer_->subscribe("bluetooth/connected", "MeasurementApp", connectedCallback)) {
        Serial.println("[MeasurementApp] Failed to subscribe to bluetooth/connected");
        return false;
    }

    if (!networkLayer_->subscribe("bluetooth/disconnected", "MeasurementApp", disconnectedCallback)) {
        Serial.println("[MeasurementApp] Failed to subscribe to bluetooth/disconnected");
        return false;
    }

    if (!networkLayer_->subscribe("bluetooth/command", "MeasurementApp", commandCallback)) {
        Serial.println("[MeasurementApp] Failed to subscribe to bluetooth/command");
        return false;
    }

    if (!networkLayer_->subscribe("mpu/data", "MeasurementApp", mpuDataCallback)) {
        Serial.println("[MeasurementApp] Failed to subscribe to mpu/data");
        return false;
    }

    initialized_ = true;
    Serial.println("[MeasurementApp] Setup complete - Ready to record MPU data on Bluetooth commands");
    return true;
}

void MeasurementApp::update() {
    if (!initialized_) {
        return;
    }

    // Log status periodically
    logRecordingStatus();
}

bool MeasurementApp::isRecording() const {
    return recording_;
}

size_t MeasurementApp::getRecordedSamplesCount() const {
    return sampleCount_;
}

void MeasurementApp::onBluetoothConnected(const uint8_t* data, size_t len, const std::string& topic) {
    Serial.println("[MeasurementApp] Bluetooth connected - Ready for commands");
    
    // Clear any previous recording data on new connection
    clearRecordedData();
}

void MeasurementApp::onBluetoothDisconnected(const uint8_t* data, size_t len, const std::string& topic) {
    Serial.println("[MeasurementApp] Bluetooth disconnected");
    
    // Just disable recording - don't stop MPU capturing
    if (recording_) {
        Serial.println("[MeasurementApp] Disabling recording due to disconnection");
        recording_ = false;
    }
}

void MeasurementApp::onBluetoothCommand(const uint8_t* data, size_t len, const std::string& topic) {
    if (len == 0 || len > 50) { // Sanity check
        Serial.printf("[MeasurementApp] Invalid command length: %d\n", len);
        return;
    }

    // Debug: Show raw bytes
    Serial.printf("[MeasurementApp] Raw bytes (%d): ", len);
    for (size_t i = 0; i < len && i < 10; i++) {
        Serial.printf("%02X ", data[i]);
    }
    Serial.println();

    // Convert to string and clean it up
    String command = "";
    for (size_t i = 0; i < len; i++) {
        char c = (char)data[i];
        if (c >= 32 && c <= 126) { // Only printable ASCII
            command += c;
        } else if (c == '\0') { // Stop at null terminator
            break;
        }
    }
    
    command.trim();
    command.toUpperCase(); // Make case insensitive

    Serial.printf("[MeasurementApp] Processing command: '%s' (len=%d)\n", command.c_str(), command.length());

    if (command == "START") {
        handleStartCommand();
    } else if (command == "STOP") {
        handleStopCommand();
    } else if (command == "DATA") {
        handleDataCommand();
    } else {
        Serial.printf("[MeasurementApp] Unknown command: '%s'\n", command.c_str());
    }
}

void MeasurementApp::onMpuData(const uint8_t* data, size_t len, const std::string& topic) {
    // Fast path: return immediately if not recording
    if (!recording_) {
        return;
    }

    // Ultra-fast data storage when recording - no validation for maximum speed
    // MPU data format: timestamp(4) + ax(4) + ay(4) + az(4) + gx(4) + gy(4) + gz(4) = 28 bytes

    // Check buffer limits (fast check)
    if (sampleCount_ >= MAX_SAMPLES) {
        // Signal to stop recording due to buffer full
        recording_ = false;
        Serial.println("[MeasurementApp] Buffer full, stopping recording");
        return;
    }

    // Direct memory copy for maximum speed - skip timestamp, store only sensor values
    // Skip first 4 bytes (timestamp), copy 24 bytes of sensor data (6 floats)
    const size_t sensorDataOffset = 4;
    const size_t sensorDataSize = 24; // 6 floats * 4 bytes each

    if (len >= sensorDataOffset + sensorDataSize) {
        // Store sensor values directly (ax, ay, az, gx, gy, gz)
        const float* sensorValues = reinterpret_cast<const float*>(data + sensorDataOffset);

        // Use push_back for safety instead of direct indexing
        recordedData_.push_back(sensorValues[0]); // ax
        recordedData_.push_back(sensorValues[1]); // ay
        recordedData_.push_back(sensorValues[2]); // az
        recordedData_.push_back(sensorValues[3]); // gx
        recordedData_.push_back(sensorValues[4]); // gy
        recordedData_.push_back(sensorValues[5]); // gz

        sampleCount_++;

        // Debug: Log every 50 samples
        if (sampleCount_ % 50 == 0) {
            Serial.printf("[MeasurementApp] Recorded %d samples so far\n", sampleCount_);
        }
    } else {
        Serial.printf("[MeasurementApp] Invalid MPU data size: %d bytes\n", len);
    }
}

void MeasurementApp::handleStartCommand() {
    if (recording_) {
        Serial.println("[MeasurementApp] Already recording, ignoring START command");
        return;
    }

    Serial.println("[MeasurementApp] Starting recording");
    
    // Clear previous data
    clearRecordedData();
    
    // Enable recording mode - MPU is already capturing at maximum speed
    recording_ = true;
    recordingStartTime_ = millis();

    // Notify via Bluetooth
    String response = "RECORDING_STARTED";
    networkLayer_->publish("bluetooth/transmit", (const uint8_t*)response.c_str(), response.length());
}

void MeasurementApp::handleDataCommand() {
    Serial.println("[MeasurementApp] DATA command received - transmitting recorded data");
    transmitRecordedData();
}

void MeasurementApp::handleStopCommand() {
    if (!recording_) {
        Serial.println("[MeasurementApp] Not recording, ignoring STOP command");
        return;
    }

    Serial.println("[MeasurementApp] Stopping recording");
    recording_ = false;

    unsigned long recordingDuration = millis() - recordingStartTime_;
    Serial.printf("[MeasurementApp] Recording stopped. Duration: %lu ms, Samples: %d\n",
                 recordingDuration, sampleCount_);

    // Transmit recorded data
    transmitRecordedData();
}

void MeasurementApp::transmitRecordedData() {
    if (sampleCount_ == 0) {
        Serial.println("[MeasurementApp] No data to transmit");
        String noDataMsg = "NO_DATA\n";
        networkLayer_->publish("bluetooth/transmit", (const uint8_t*)noDataMsg.c_str(), noDataMsg.length());
        return;
    }

    Serial.printf("[MeasurementApp] Transmitting %d samples as CSV (%d floats)\n", sampleCount_, recordedData_.size());

    // Send header with sample count and CSV format info
    String header = "DATA_START:" + String(sampleCount_) + "\n";
    header += "Format: ax,ay,az,gx,gy,gz\n";
    networkLayer_->publish("bluetooth/transmit", (const uint8_t*)header.c_str(), header.length());

    // Send data as CSV lines (6 floats per sample)
    const size_t floatsPerSample = 6;
    char csvLine[128]; // Buffer for one CSV line
    
    for (size_t sample = 0; sample < sampleCount_; sample++) {
        size_t baseIndex = sample * floatsPerSample;
        
        // Format: ax,ay,az,gx,gy,gz
        snprintf(csvLine, sizeof(csvLine), "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                recordedData_[baseIndex],     // ax
                recordedData_[baseIndex + 1], // ay
                recordedData_[baseIndex + 2], // az
                recordedData_[baseIndex + 3], // gx
                recordedData_[baseIndex + 4], // gy
                recordedData_[baseIndex + 5]); // gz
        
        // Send the CSV line
        networkLayer_->publish("bluetooth/transmit", 
                              (const uint8_t*)csvLine, 
                              strlen(csvLine));
        
        // Small delay between lines to prevent Bluetooth buffer overflow
        delay(5);
    }

    // Send end marker
    String endMarker = "DATA_END\n";
    networkLayer_->publish("bluetooth/transmit", (const uint8_t*)endMarker.c_str(), endMarker.length());

    Serial.printf("[MeasurementApp] CSV data transmission complete (%d samples sent)\n", sampleCount_);
}

void MeasurementApp::clearRecordedData() {
    recordedData_.clear();
    sampleCount_ = 0;
    Serial.println("[MeasurementApp] Cleared recorded data");
}

void MeasurementApp::logRecordingStatus() {
    static unsigned long lastLogTime = 0;
    unsigned long currentTime = millis();

    // Log every 2 seconds when recording (main app runs at 10Hz)
    if (recording_ && (currentTime - lastLogTime >= 2000)) {
        unsigned long duration = currentTime - recordingStartTime_;
        Serial.printf("[MeasurementApp] Recording... Duration: %lu ms, Samples: %d\n",
                     duration, sampleCount_);
        lastLogTime = currentTime;
    }
}
