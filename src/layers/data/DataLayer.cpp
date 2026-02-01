#include "DataLayer.h"
#include <Arduino.h> // For Serial debugging and millis()
#include <algorithm>

DataLayer::DataLayer() :
    dataMutex_(nullptr),
    cleanupTask_(nullptr),
    cleanupIntervalMs_(5000),
    initialized_(false) {
    Serial.println("[DataLayer] Redis-like key-value store created");
}

DataLayer::~DataLayer() {
    if (initialized_) {
        // Stop the cleanup task
        if (cleanupTask_ != nullptr) {
            vTaskDelete(cleanupTask_);
            cleanupTask_ = nullptr;
        }

        // Clean up RTOS resources
        if (dataMutex_ != nullptr) {
            vSemaphoreDelete(dataMutex_);
            dataMutex_ = nullptr;
        }

        initialized_ = false;
        Serial.println("[DataLayer] Cleaned up RTOS resources");
    }
}

bool DataLayer::init(uint32_t cleanupIntervalMs, UBaseType_t taskPriority, uint32_t taskStackSize) {
    if (initialized_) {
        Serial.println("[DataLayer] Already initialized");
        return true;
    }

    cleanupIntervalMs_ = cleanupIntervalMs;

    // Create mutex for data protection
    dataMutex_ = xSemaphoreCreateMutex();
    if (dataMutex_ == nullptr) {
        Serial.println("[DataLayer] Failed to create data mutex");
        return false;
    }

    // Create cleanup task
    BaseType_t result = xTaskCreate(
        cleanupTask,             // Task function
        "DataCleanup",           // Task name
        taskStackSize,           // Stack size
        this,                    // Task parameter
        taskPriority,            // Priority
        &cleanupTask_            // Task handle
    );

    if (result != pdPASS) {
        Serial.println("[DataLayer] Failed to create cleanup task");
        vSemaphoreDelete(dataMutex_);
        dataMutex_ = nullptr;
        return false;
    }

    initialized_ = true;
    Serial.printf("[DataLayer] Initialized with cleanup interval %d ms, task priority %d\n",
                  cleanupIntervalMs_, taskPriority);
    return true;
}

bool DataLayer::set(const std::string& key, const std::vector<uint8_t>& value, uint32_t ttlMs) {
    if (key.empty() || !initialized_) {
        return false;
    }

    // Take mutex to protect data
    if (xSemaphoreTake(dataMutex_, portMAX_DELAY) != pdTRUE) {
        Serial.println("[DataLayer] Failed to take data mutex in set");
        return false;
    }

    uint32_t currentTime = getCurrentTimeMs();
    DataEntry entry;
    entry.value = value;
    entry.createdTime = currentTime;
    entry.expiryTime = (ttlMs > 0) ? (currentTime + ttlMs) : 0;

    data_[key] = entry;

    xSemaphoreGive(dataMutex_);

    // Don't log frequent MPU readings to avoid spam
    if (key != "mpu/last_reading") {
        Serial.printf("[DataLayer] Set key '%s' with %d bytes%s\n",
                      key.c_str(), value.size(),
                      (ttlMs > 0) ? (", TTL: " + std::to_string(ttlMs) + "ms").c_str() : "");
    }
    return true;
}

bool DataLayer::get(const std::string& key, std::vector<uint8_t>& value) {
    if (key.empty() || !initialized_) {
        return false;
    }

    // Take mutex to protect data
    if (xSemaphoreTake(dataMutex_, portMAX_DELAY) != pdTRUE) {
        Serial.println("[DataLayer] Failed to take data mutex in get");
        return false;
    }

    auto it = data_.find(key);
    if (it == data_.end()) {
        xSemaphoreGive(dataMutex_);
        return false; // Key doesn't exist
    }

    // Check if expired
    uint32_t currentTime = getCurrentTimeMs();
    if (it->second.expiryTime > 0 && currentTime >= it->second.expiryTime) {
        // Key has expired, remove it
        data_.erase(it);
        xSemaphoreGive(dataMutex_);
        Serial.printf("[DataLayer] Key '%s' has expired\n", key.c_str());
        return false;
    }

    value = it->second.value;
    xSemaphoreGive(dataMutex_);

    Serial.printf("[DataLayer] Got key '%s' with %d bytes\n", key.c_str(), value.size());
    return true;
}

bool DataLayer::del(const std::string& key) {
    if (key.empty() || !initialized_) {
        return false;
    }

    // Take mutex to protect data
    if (xSemaphoreTake(dataMutex_, portMAX_DELAY) != pdTRUE) {
        Serial.println("[DataLayer] Failed to take data mutex in del");
        return false;
    }

    auto it = data_.find(key);
    if (it == data_.end()) {
        xSemaphoreGive(dataMutex_);
        return false; // Key doesn't exist
    }

    data_.erase(it);
    xSemaphoreGive(dataMutex_);

    Serial.printf("[DataLayer] Deleted key '%s'\n", key.c_str());
    return true;
}

bool DataLayer::exists(const std::string& key) {
    if (key.empty() || !initialized_) {
        return false;
    }

    // Take mutex to protect data
    if (xSemaphoreTake(dataMutex_, portMAX_DELAY) != pdTRUE) {
        Serial.println("[DataLayer] Failed to take data mutex in exists");
        return false;
    }

    auto it = data_.find(key);
    if (it == data_.end()) {
        xSemaphoreGive(dataMutex_);
        return false; // Key doesn't exist
    }

    // Check if expired
    uint32_t currentTime = getCurrentTimeMs();
    if (it->second.expiryTime > 0 && currentTime >= it->second.expiryTime) {
        // Key has expired, remove it
        data_.erase(it);
        xSemaphoreGive(dataMutex_);
        return false;
    }

    xSemaphoreGive(dataMutex_);
    return true;
}

std::vector<std::string> DataLayer::keys() {
    if (!initialized_) {
        return {};
    }

    // Take mutex to protect data
    if (xSemaphoreTake(dataMutex_, portMAX_DELAY) != pdTRUE) {
        Serial.println("[DataLayer] Failed to take data mutex in keys");
        return {};
    }

    std::vector<std::string> result;
    uint32_t currentTime = getCurrentTimeMs();

    for (auto it = data_.begin(); it != data_.end(); ) {
        // Check if expired
        if (it->second.expiryTime > 0 && currentTime >= it->second.expiryTime) {
            // Remove expired key
            it = data_.erase(it);
        } else {
            result.push_back(it->first);
            ++it;
        }
    }

    xSemaphoreGive(dataMutex_);
    return result;
}

bool DataLayer::expire(const std::string& key, uint32_t ttlMs) {
    if (key.empty() || !initialized_) {
        return false;
    }

    // Take mutex to protect data
    if (xSemaphoreTake(dataMutex_, portMAX_DELAY) != pdTRUE) {
        Serial.println("[DataLayer] Failed to take data mutex in expire");
        return false;
    }

    auto it = data_.find(key);
    if (it == data_.end()) {
        xSemaphoreGive(dataMutex_);
        return false; // Key doesn't exist
    }

    uint32_t currentTime = getCurrentTimeMs();
    it->second.expiryTime = currentTime + ttlMs;

    xSemaphoreGive(dataMutex_);

    Serial.printf("[DataLayer] Set TTL for key '%s' to %d ms\n", key.c_str(), ttlMs);
    return true;
}

int32_t DataLayer::ttl(const std::string& key) {
    if (key.empty() || !initialized_) {
        return -2; // Key doesn't exist
    }

    // Take mutex to protect data
    if (xSemaphoreTake(dataMutex_, portMAX_DELAY) != pdTRUE) {
        Serial.println("[DataLayer] Failed to take data mutex in ttl");
        return -2;
    }

    auto it = data_.find(key);
    if (it == data_.end()) {
        xSemaphoreGive(dataMutex_);
        return -2; // Key doesn't exist
    }

    // Check if expired
    uint32_t currentTime = getCurrentTimeMs();
    if (it->second.expiryTime > 0 && currentTime >= it->second.expiryTime) {
        // Key has expired, remove it
        data_.erase(it);
        xSemaphoreGive(dataMutex_);
        return -2; // Key doesn't exist (expired)
    }

    if (it->second.expiryTime == 0) {
        xSemaphoreGive(dataMutex_);
        return -1; // No TTL set
    }

    int32_t remaining = it->second.expiryTime - currentTime;
    xSemaphoreGive(dataMutex_);

    return remaining;
}

size_t DataLayer::size() const {
    if (!initialized_) {
        return 0;
    }

    // Take mutex to protect data
    if (xSemaphoreTake(dataMutex_, portMAX_DELAY) != pdTRUE) {
        return 0;
    }

    size_t count = data_.size();
    xSemaphoreGive(dataMutex_);

    return count;
}

// RTOS Task function - runs periodically to clean up expired data
void DataLayer::cleanupTask(void* parameter) {
    DataLayer* dataLayer = static_cast<DataLayer*>(parameter);
    if (dataLayer == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    Serial.println("[DataLayer] Cleanup task started");

    while (true) {
        // Wait for cleanup interval
        vTaskDelay(pdMS_TO_TICKS(dataLayer->cleanupIntervalMs_));

        // Perform cleanup
        dataLayer->performCleanup();
    }
}

// Internal cleanup function
void DataLayer::performCleanup() {
    if (!initialized_) {
        return;
    }

    // Take mutex to protect data
    if (xSemaphoreTake(dataMutex_, portMAX_DELAY) != pdTRUE) {
        Serial.println("[DataLayer] Failed to take data mutex in performCleanup");
        return;
    }

    uint32_t currentTime = getCurrentTimeMs();
    size_t removed = 0;

    for (auto it = data_.begin(); it != data_.end(); ) {
        // Check if expired
        if (it->second.expiryTime > 0 && currentTime >= it->second.expiryTime) {
            Serial.printf("[DataLayer] Cleanup: Removing expired key '%s'\n", it->first.c_str());
            it = data_.erase(it);
            removed++;
        } else {
            ++it;
        }
    }

    xSemaphoreGive(dataMutex_);

    if (removed > 0) {
        Serial.printf("[DataLayer] Cleanup completed, removed %d expired keys\n", removed);
    }
}

// Get current time in milliseconds
uint32_t DataLayer::getCurrentTimeMs() {
    return millis();
}