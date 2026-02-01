#include "Bluetooth.h"
#include <Arduino.h>

Bluetooth::Bluetooth()
    : initialized_(false) {
    Serial.println("[Bluetooth] Created");
}

Bluetooth::~Bluetooth() {
    if (initialized_) {
        // Unsubscribe from topics
        networkLayer_->unsubscribe("bluetooth/transmit", "Bluetooth");
        Serial.println("[Bluetooth] Cleaned up");
    }
}

bool Bluetooth::setup() {
    if (!networkLayer_ || !dataLayer_) {
        Serial.println("[Bluetooth] ERROR: Missing layer dependencies");
        return false;
    }

    // Initialize Bluetooth
    SerialBT.begin("ESP32-CAM-TAF");
    Serial.println("[Bluetooth] Bluetooth started. Pair with ESP32-CAM-TAF");

    // Subscribe to transmit topic for sending data over Bluetooth
    auto transmitCallback = [this](const uint8_t* data, size_t len, const std::string& topic) {
        this->onTransmitData(data, len, topic);
    };

    // Serial.println("[Bluetooth] Subscribing to bluetooth/transmit topic");

    if (!networkLayer_->subscribe("bluetooth/transmit", "Bluetooth", transmitCallback)) {
        Serial.println("[Bluetooth] Failed to subscribe to bluetooth/transmit");
        return false;
    }

    initialized_ = true;
    Serial.println("[Bluetooth] Setup complete");
    return true;
}

void Bluetooth::update() {
    if (!initialized_) {
        return;
    }
    
    // Monitor and publish connection status changes
    logConnectionStatus();

    // Read incoming Bluetooth data and publish as commands
    if (SerialBT.available()) {
        // Read available data into a buffer
        size_t available = SerialBT.available();
        if (available > 100) available = 100; // Limit buffer size
        
        uint8_t buffer[101];
        size_t bytesRead = SerialBT.readBytes(buffer, available);
        buffer[bytesRead] = '\0'; // Null terminate for safety
        
        if (bytesRead > 0) {
            // Try to extract command from buffer
            String command = "";
            bool foundNewline = false;
            
            for (size_t i = 0; i < bytesRead; i++) {
                char c = (char)buffer[i];
                if (c == '\n' || c == '\r') {
                    foundNewline = true;
                    break;
                }
                if (c >= 32 && c <= 126) { // Printable ASCII
                    command += c;
                }
            }
            
            // If no newline found, use entire buffer as command
            if (!foundNewline && bytesRead > 0) {
                command = String((char*)buffer);
                command.trim();
            }
            
            Serial.printf("[Bluetooth] Read %d bytes, extracted command: '%s'\n", bytesRead, command.c_str());
            
            if (command.length() > 0) {
                Serial.printf("[Bluetooth] Publishing command: '%s' (%d bytes)\n", command.c_str(), command.length());
                
                // Debug: Show bytes being published
                Serial.printf("[Bluetooth] Publishing bytes: ");
                const uint8_t* cmdBytes = (const uint8_t*)command.c_str();
                for (size_t i = 0; i < command.length(); i++) {
                    Serial.printf("%02X ", cmdBytes[i]);
                }
                Serial.println();
                
                // Publish command to network for other apps to process
                networkLayer_->publish("bluetooth/command", 
                                      (const uint8_t*)command.c_str(), 
                                      command.length());
            } else {
                Serial.printf("[Bluetooth] No valid command found in %d bytes\n", bytesRead);
            }
        }
    }
}bool Bluetooth::isConnected() {
    return SerialBT.connected();
}

void Bluetooth::sendData(const String& data) {
    Serial.printf("[Bluetooth] sendData called (len=%d), connected=%d\n", data.length(), SerialBT.connected());
    if (SerialBT.connected()) {
        // Debug dump
        Serial.printf("[Bluetooth] sendData bytes: ");
        const uint8_t* bytes = (const uint8_t*)data.c_str();
        for (size_t i = 0; i < data.length() && i < 64; i++) Serial.printf("%02X ", bytes[i]);
        Serial.println();

        SerialBT.println(data);
        Serial.println("[Bluetooth] sendData: sent via SerialBT.println");
    } else {
        Serial.println("[Bluetooth] sendData: not connected");
    }
}

void Bluetooth::sendBinaryData(const uint8_t* data, size_t len) {
    Serial.printf("[Bluetooth] sendBinaryData called (len=%d), connected=%d\n", len, SerialBT.connected());
    if (SerialBT.connected()) {
        Serial.printf("[Bluetooth] sendBinaryData bytes: ");
        for (size_t i = 0; i < len && i < 64; i++) Serial.printf("%02X ", data[i]);
        Serial.println();

        SerialBT.write(data, len);
        Serial.println("[Bluetooth] sendBinaryData: wrote bytes to SerialBT");
    } else {
        Serial.println("[Bluetooth] sendBinaryData: not connected");
    }
}

String Bluetooth::getConnectionStatus() {
    if (!initialized_) {
        return "NOT_INITIALIZED";
    }
    return SerialBT.connected() ? "CONNECTED" : "DISCONNECTED";
}

void Bluetooth::onTransmitData(const uint8_t* data, size_t len, const std::string& topic) {
    Serial.printf("[Bluetooth] onTransmitData called for topic %s (len=%d), connected=%d\n", topic.c_str(), len, SerialBT.connected());
    if (!data || len == 0) {
        Serial.println("[Bluetooth] onTransmitData: no data to send");
        return;
    }

    // Dump a short sample of bytes for debugging
    Serial.printf("[Bluetooth] onTransmitData bytes: ");
    for (size_t i = 0; i < len && i < 128; i++) {
        Serial.printf("%02X ", data[i]);
    }
    Serial.println();

    if (SerialBT.connected()) {
        // If the payload looks like printable text, use println for convenience
        bool printable = true;
        for (size_t i = 0; i < len; i++) {
            if (data[i] < 32 && data[i] != '\n' && data[i] != '\r') { printable = false; break; }
        }

        if (printable) {
            // send as string
            String s((const char*)data, len);
            SerialBT.println(s);
            Serial.println("[Bluetooth] onTransmitData: sent as text via println");
        } else {
            SerialBT.write(data, len);
            Serial.println("[Bluetooth] onTransmitData: sent as binary via write");
        }
    } else {
        Serial.println("[Bluetooth] Cannot transmit - not connected");
    }
}

void Bluetooth::logConnectionStatus() {
    static bool lastConnected = false;
    bool currentlyConnected = SerialBT.connected();

    if (currentlyConnected != lastConnected) {
        Serial.printf("[Bluetooth] Connection status changed: %s\n",
                     currentlyConnected ? "CONNECTED" : "DISCONNECTED");

        // Publish connection state change to network
        std::string topic = currentlyConnected ? "bluetooth/connected" : "bluetooth/disconnected";
        uint8_t stateData = currentlyConnected ? 1 : 0;
        networkLayer_->publish(topic, &stateData, 1);

        lastConnected = currentlyConnected;
    }
}