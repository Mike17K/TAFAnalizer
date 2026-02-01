#include "MPU.h"
#include <Arduino.h>

MPU::MPU()
    : initialized_(false),
      capturing_(false),
      lastReadingTime_(0),
      last_ax_(0), last_ay_(0), last_az_(0), last_gx_(0), last_gy_(0), last_gz_(0) {
    Serial.println("[MPU] Created");
}

MPU::~MPU() {
    if (initialized_) {
        // Unsubscribe from topics
        networkLayer_->unsubscribe("capture/start", "MPU");
        networkLayer_->unsubscribe("capture/stop", "MPU");
        networkLayer_->unsubscribe("mpu/data_request", "MPU");
        Serial.println("[MPU] Cleaned up");
    }
}

bool MPU::setup() {
    if (!networkLayer_ || !dataLayer_) {
        Serial.println("[MPU] ERROR: Missing layer dependencies");
        return false;
    }

    // Initialize MPU hardware
    if (!initMPU()) {
        Serial.println("[MPU] Failed to initialize MPU6050 sensor");
        return false;
    }

    // Subscribe to network topics
    auto startCallback = [this](const uint8_t* data, size_t len, const std::string& topic) {
        this->onStartCapture(data, len, topic);
    };

    auto stopCallback = [this](const uint8_t* data, size_t len, const std::string& topic) {
        this->onStopCapture(data, len, topic);
    };

    auto dataRequestCallback = [this](const uint8_t* data, size_t len, const std::string& topic) {
        this->onDataRequest(data, len, topic);
    };

    if (!networkLayer_->subscribe("capture/start", "MPU", startCallback)) {
        Serial.println("[MPU] Failed to subscribe to capture/start");
        return false;
    }

    if (!networkLayer_->subscribe("capture/stop", "MPU", stopCallback)) {
        Serial.println("[MPU] Failed to subscribe to capture/stop");
        return false;
    }

    if (!networkLayer_->subscribe("mpu/data_request", "MPU", dataRequestCallback)) {
        Serial.println("[MPU] Failed to subscribe to mpu/data_request");
        return false;
    }

    initialized_ = true;
    Serial.println("[MPU] Setup complete - MPU6050 ready for data collection");
    return true;
}

void MPU::update() {
    if (!initialized_) {
        return;
    }

    // Read and publish data if capturing
    if (capturing_) {
        unsigned long currentTime = millis();
        if (currentTime - lastReadingTime_ >= READING_INTERVAL_MS) {
            readAndPublishData();
            lastReadingTime_ = currentTime;
        }
    }
}

void MPU::startCapture() {
    if (!initialized_) {
        Serial.println("[MPU] Cannot start capture - not initialized");
        return;
    }

    capturing_ = true;
    lastReadingTime_ = millis();
    Serial.println("[MPU] Capture started");

    // Publish capture started event
    std::vector<uint8_t> eventData = {'S', 'T', 'A', 'R', 'T', 'E', 'D'};
    networkLayer_->publish("mpu/status", eventData.data(), eventData.size());
}

void MPU::stopCapture() {
    if (!initialized_) {
        return;
    }

    capturing_ = false;
    Serial.println("[MPU] Capture stopped");

    // Publish capture stopped event
    std::vector<uint8_t> eventData = {'S', 'T', 'O', 'P', 'P', 'E', 'D'};
    networkLayer_->publish("mpu/status", eventData.data(), eventData.size());
}

bool MPU::isCapturing() const {
    return capturing_;
}

bool MPU::getLastReading(float& ax, float& ay, float& az, float& gx, float& gy, float& gz) const {
    if (!initialized_) {
        return false;
    }

    ax = last_ax_;
    ay = last_ay_;
    az = last_az_;
    gx = last_gx_;
    gy = last_gy_;
    gz = last_gz_;

    return true;
}

void MPU::onStartCapture(const uint8_t* data, size_t len, const std::string& topic) {
    Serial.println("[MPU] Received start capture command via network");
    startCapture();
}

void MPU::onStopCapture(const uint8_t* data, size_t len, const std::string& topic) {
    Serial.println("[MPU] Received stop capture command via network");
    stopCapture();
}

void MPU::onDataRequest(const uint8_t* data, size_t len, const std::string& topic) {
    Serial.println("[MPU] Data request received");

    // Send current sensor reading
    float ax, ay, az, gx, gy, gz;
    if (getLastReading(ax, ay, az, gx, gy, gz)) {
        publishSensorData(ax, ay, az, gx, gy, gz);
    } else {
        Serial.println("[MPU] Failed to get sensor reading for data request");
    }
}

void MPU::readAndPublishData() {
    float ax, ay, az, gx, gy, gz;

    if (readMPUData(ax, ay, az, gx, gy, gz)) {
        publishSensorData(ax, ay, az, gx, gy, gz);
        logSensorData(ax, ay, az, gx, gy, gz);
    } else {
        Serial.println("[MPU] Failed to read sensor data");
    }
}

void MPU::publishSensorData(float ax, float ay, float az, float gx, float gy, float gz) {
    // Create binary data packet: timestamp(4) + ax(4) + ay(4) + az(4) + gx(4) + gy(4) + gz(4) = 28 bytes
    uint8_t data[28];
    uint32_t timestamp = millis();

    memcpy(data, &timestamp, 4);
    memcpy(data + 4, &ax, 4);
    memcpy(data + 8, &ay, 4);
    memcpy(data + 12, &az, 4);
    memcpy(data + 16, &gx, 4);
    memcpy(data + 20, &gy, 4);
    memcpy(data + 24, &gz, 4);

    // Publish to network
    networkLayer_->publish("mpu/data", data, sizeof(data));

    // Store in data layer for other applications to access
    std::vector<uint8_t> dataVector(data, data + sizeof(data));
    dataLayer_->set("mpu/last_reading", dataVector, 1000); // 1 second TTL
}

void MPU::logSensorData(float ax, float ay, float az, float gx, float gy, float gz) {
    static unsigned long lastLogTime = 0;
    unsigned long currentTime = millis();

    // Log every 500ms to avoid spam
    if (currentTime - lastLogTime >= 500) {
        Serial.printf("[MPU] Accel: %.2f, %.2f, %.2f | Gyro: %.2f, %.2f, %.2f\n",
                     ax, ay, az, gx, gy, gz);
        lastLogTime = currentTime;
    }
}

// MPU hardware implementation
bool MPU::initMPU() {
    if (!mpu_.begin()) {
        Serial.println("[MPU] Failed to find MPU6050 chip");
        return false;
    }

    // Configure for maximum speed - disable motion detection for faster readings
    // mpu_.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
    // mpu_.setMotionDetectionThreshold(1);
    // mpu_.setMotionDetectionDuration(20);
    // mpu_.setInterruptPinLatch(true);
    // mpu_.setInterruptPinPolarity(true);
    // mpu_.setMotionInterrupt(true);

    // Set accelerometer range for optimal performance
    mpu_.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu_.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu_.setFilterBandwidth(MPU6050_BAND_260_HZ);

    Serial.println("[MPU] MPU6050 initialized for stable Bluetooth operation (50Hz)");
    return true;
}

bool MPU::readMPUData(float& ax, float& ay, float& az, float& gx, float& gy, float& gz) {
    sensors_event_t a, g, temp;
    mpu_.getEvent(&a, &g, &temp);

    ax = a.acceleration.x;
    ay = a.acceleration.y;
    az = a.acceleration.z;
    gx = g.gyro.x;
    gy = g.gyro.y;
    gz = g.gyro.z;

    // Store last reading
    last_ax_ = ax;
    last_ay_ = ay;
    last_az_ = az;
    last_gx_ = gx;
    last_gy_ = gy;
    last_gz_ = gz;

    return true;
}