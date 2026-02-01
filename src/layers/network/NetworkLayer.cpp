#include "NetworkLayer.h"
#include <algorithm>
#include <Arduino.h> // For Serial debugging

NetworkLayer::NetworkLayer() :
    subscribersMutex_(nullptr),
    initialized_(false) {
    Serial.println("[NetworkLayer] Topic-based message broker created");
}

NetworkLayer::~NetworkLayer() {
    if (initialized_) {
        // Clean up RTOS resources
        if (subscribersMutex_ != nullptr) {
            vSemaphoreDelete(subscribersMutex_);
            subscribersMutex_ = nullptr;
        }

        initialized_ = false;
        Serial.println("[NetworkLayer] Cleaned up RTOS resources");
    }
}

bool NetworkLayer::init() {
    if (initialized_) {
        Serial.println("[NetworkLayer] Already initialized");
        return true;
    }

    // Create mutex for subscriber management
    subscribersMutex_ = xSemaphoreCreateMutex();
    if (subscribersMutex_ == nullptr) {
        Serial.println("[NetworkLayer] Failed to create subscribers mutex");
        return false;
    }

    initialized_ = true;
    Serial.println("[NetworkLayer] Initialized with direct callback delivery");
    return true;
}

bool NetworkLayer::subscribe(const std::string& topic, const std::string& appName, MessageCallback callback) {
    if (topic.empty() || appName.empty() || !callback) {
        return false;
    }

    if (!initialized_) {
        Serial.println("[NetworkLayer] Not initialized - call init() first");
        return false;
    }

    // Take mutex to protect subscriber list
    if (xSemaphoreTake(subscribersMutex_, portMAX_DELAY) != pdTRUE) {
        Serial.println("[NetworkLayer] Failed to take subscribers mutex");
        return false;
    }

    // Add or update subscriber
    subscribers_[topic][appName] = callback;

    // Get count directly without calling getSubscriberCount() to avoid nested mutex lock
    size_t count = subscribers_[topic].size();

    // Serial.printf("[NetworkLayer] %s subscribed to %s (total: %d)\n",
    //               appName.c_str(), topic.c_str(), count);

    xSemaphoreGive(subscribersMutex_);
    return true;
}

bool NetworkLayer::unsubscribe(const std::string& topic, const std::string& appName) {
    if (topic.empty() || appName.empty()) {
        return false;
    }

    if (!initialized_) {
        return false;
    }

    // Take mutex to protect subscriber list
    if (xSemaphoreTake(subscribersMutex_, portMAX_DELAY) != pdTRUE) {
        Serial.println("[NetworkLayer] Failed to take subscribers mutex");

        return false;
    }

    auto topicIt = subscribers_.find(topic);
    if (topicIt != subscribers_.end()) {
        size_t beforeCount = topicIt->second.size();
        topicIt->second.erase(appName);

        if (topicIt->second.empty()) {
            subscribers_.erase(topicIt);
        }

        // Serial.printf("[NetworkLayer] %s unsubscribed from %s\n", appName.c_str(), topic.c_str());
    }

    xSemaphoreGive(subscribersMutex_);
    return true;
}

bool NetworkLayer::publish(const std::string& topic, const uint8_t* data, size_t len, const std::string& publisher) {
    if (topic.empty() || !data || len == 0) {
        return false;
    }

    if (!initialized_) {
        Serial.println("[NetworkLayer] Not initialized - call init() first");
        return false;
    }

    // Debug: Show what we're publishing
    if (topic == "bluetooth/command") {
        Serial.printf("[NetworkLayer] Publishing to %s: ", topic.c_str());
        for (size_t i = 0; i < len && i < 10; i++) {
            Serial.printf("%02X ", data[i]);
        }
        Serial.printf("(%d bytes)\n", len);
    }

    // Copy data to heap so it remains valid for the delivery task
    uint8_t* dataCopy = new uint8_t[len];
    memcpy(dataCopy, data, len);

    // Deliver message to all subscribers asynchronously with rtos
    xTaskCreate(
        [](void* param) {
            auto* args = static_cast<std::tuple<NetworkLayer*, std::string, uint8_t*, size_t>*>(param);
            NetworkLayer* nl = std::get<0>(*args);
            std::string topic = std::get<1>(*args);
            uint8_t* dataPtr = std::get<2>(*args);
            size_t len = std::get<3>(*args);

            nl->deliverMessage(topic, dataPtr, len);

            // Free the copied payload and the args tuple
            delete[] dataPtr;
            delete args;
            vTaskDelete(nullptr);
        },
        "MsgDeliverTask",
        4096,
        new std::tuple<NetworkLayer*, std::string, uint8_t*, size_t>(this, topic, dataCopy, len),
        1,
        nullptr
    );

    // Serial.printf("[NetworkLayer] Delivered %d bytes to topic %s\n", len, topic.c_str());
    return true;
}

bool NetworkLayer::hasSubscribers(const std::string& topic) const {
    if (!initialized_) {
        return false;
    }

    // Take mutex to protect subscriber list
    if (xSemaphoreTake(subscribersMutex_, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    bool result = subscribers_.find(topic) != subscribers_.end() &&
                  !subscribers_.at(topic).empty();

    xSemaphoreGive(subscribersMutex_);
    return result;
}

size_t NetworkLayer::getSubscriberCount(const std::string& topic) const {
    if (!initialized_) {
        return 0;
    }

    // Take mutex to protect subscriber list
    if (xSemaphoreTake(subscribersMutex_, portMAX_DELAY) != pdTRUE) {
        return 0;
    }

    size_t count = 0;
    auto it = subscribers_.find(topic);
    if (it != subscribers_.end()) {
        count = it->second.size();
    }

    xSemaphoreGive(subscribersMutex_);
    return count;
}

std::vector<std::string> NetworkLayer::getTopics() const {
    std::vector<std::string> topics;

    if (!initialized_) {
        return topics;
    }

    // Take mutex to protect subscriber list
    if (xSemaphoreTake(subscribersMutex_, portMAX_DELAY) != pdTRUE) {
        return topics;
    }

    for (const auto& pair : subscribers_) {
        topics.push_back(pair.first);
    }

    xSemaphoreGive(subscribersMutex_);
    return topics;
}

void NetworkLayer::deliverMessage(const std::string& topic, const uint8_t* data, size_t len) {
    // Take mutex to protect subscriber list during iteration
    if (xSemaphoreTake(subscribersMutex_, portMAX_DELAY) != pdTRUE) {
        Serial.println("[NetworkLayer] Failed to take subscribers mutex for delivery");
        return;
    }

    auto topicIt = subscribers_.find(topic);
    if (topicIt != subscribers_.end()) {
        // Create a copy of subscribers to avoid issues if callbacks modify subscriptions
        auto subscribersCopy = topicIt->second;

        // Debug: Show what we're delivering
        if (topic == "bluetooth/command") {
            Serial.printf("[NetworkLayer] Delivering to %d subscribers of %s: ", subscribersCopy.size(), topic.c_str());
            for (size_t i = 0; i < len && i < 10; i++) {
                Serial.printf("%02X ", data[i]);
            }
            Serial.printf("(%d bytes)\n", len);
        }

        // Serial.printf("[NetworkLayer] Delivering to %d subscribers of %s\n",
        //               subscribersCopy.size(), topic.c_str());

        // Release mutex before calling callbacks
        xSemaphoreGive(subscribersMutex_);

        // Call all subscribers synchronously
        for (const auto& pair : subscribersCopy) {
            try {
                pair.second(data, len, topic);
            } catch (const std::exception& e) {
                Serial.printf("[NetworkLayer] Exception in callback for %s: %s\n",
                              pair.first.c_str(), e.what());
            }
        }
    } else {
        xSemaphoreGive(subscribersMutex_);
        // Serial.printf("[NetworkLayer] No subscribers for topic %s (message still delivered for future subscribers)\n", topic.c_str());
    }
}
