#include <Arduino.h>
#include <Wire.h>
#include <soc/rtc_cntl_reg.h>
#include <stdexcept>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "layers/network/NetworkLayer.h"
#include "layers/data/DataLayer.h"
// #include "layers/application/camera/Camera.h"
#include "layers/application/ApplicationInterface.h"
#include "layers/application/bluetooth/Bluetooth.h"
#include "layers/application/mpu/MPU.h"
#include "layers/application/led/LED.h"
#include "layers/application/measurement/MeasurementApp.h"
// Network instances
NetworkLayer *networkLayer = nullptr;
// Data Layer instance
DataLayer *dataLayer = nullptr;

// Application instances

// Camera* cameraApp = nullptr;
ApplicationInterface *bluetoothApp = nullptr;
ApplicationInterface *mpuApp = nullptr;
ApplicationInterface *ledApp = nullptr;
ApplicationInterface *measurementApp = nullptr;

void setup()
{
  Serial.begin(115200);

  // Disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  // Init I2C for MPU6050 (will be handled by MPU application later)
  Wire.begin(14, 15); // SDA 14, SCL 15
  Wire.setClock(400000); // Set I2C to 400kHz for maximum speed
  networkLayer = new NetworkLayer();
  if (!networkLayer || !networkLayer->init())
  {
    Serial.println("[ApplicationManager] Failed to initialize Network Layer");
    throw std::runtime_error("Failed to initialize Network Layer");
  }

  // Create and initialize Data Layer
  dataLayer = new DataLayer();
  if (!dataLayer || !dataLayer->init(5000, 1, 2048))
  { // 5s cleanup, priority 1, 2KB stack
    Serial.println("[ApplicationManager] Failed to initialize Data Layer");
    throw std::runtime_error("Failed to initialize Data Layer");
  }

  // // Create and setup applications
  // cameraApp = new Camera(appManager.getNetworkLayer(), appManager.getDataLayer());
  // if (!cameraApp->setup()) {
  //     Serial.println("Failed to setup camera application");
  //     delete cameraApp;
  //     cameraApp = nullptr;
  // }
  bluetoothApp = new Bluetooth();
  bluetoothApp->setNetworkLayer(networkLayer)->setDataLayer(dataLayer);
  if (!bluetoothApp->setup())
  {
    Serial.println("Failed to setup bluetooth application");
    delete bluetoothApp;
    bluetoothApp = nullptr;
  }

  mpuApp = new MPU();
  mpuApp->setNetworkLayer(networkLayer)->setDataLayer(dataLayer);
  if (!mpuApp->setup())
  {
    Serial.println("Failed to setup MPU application");
    delete mpuApp;
    mpuApp = nullptr;
  } else {
    // Start MPU capturing immediately for maximum speed data collection
    static_cast<MPU*>(mpuApp)->startCapture();
  }

  ledApp = new LED(2); // GPIO 2
  ledApp->setNetworkLayer(networkLayer)->setDataLayer(dataLayer);
  if (!ledApp->setup())
  {
    Serial.println("Failed to setup LED application");
    delete ledApp;
    ledApp = nullptr;
  }

  measurementApp = new MeasurementApp();
  measurementApp->setNetworkLayer(networkLayer)->setDataLayer(dataLayer);
  if (!measurementApp->setup())
  {
    Serial.println("Failed to setup Measurement application");
    delete measurementApp;
    measurementApp = nullptr;
  }

  Serial.println("All applications initialized");

  // Create RTOS tasks for applications
  if (bluetoothApp) {
    if (!bluetoothApp->createTask("BluetoothApp", 4096, 2, tskNO_AFFINITY, 50)) {  // 20Hz for responsive communication
      Serial.println("Failed to create Bluetooth application task");
    }
  }

  if (mpuApp) {
    if (!mpuApp->createTask("MPUApp", 4096, 2, tskNO_AFFINITY, 20)) {  // 50Hz for stable Bluetooth operation
      Serial.println("Failed to create MPU application task");
    }
  }

  if (ledApp) {
    if (!ledApp->createTask("LEDApp", 4096, 2, tskNO_AFFINITY, 100)) {  // 10Hz for status broadcasting
      Serial.println("Failed to create LED application task");
    }
  }

  if (measurementApp) {
    if (!measurementApp->createTask("MeasurementApp", 8192, 1, tskNO_AFFINITY, 100)) {  // Normal priority, 10Hz for status updates
      Serial.println("Failed to create Measurement application task");
    }
  }

  Serial.println("RTOS tasks initialized - applications now running concurrently");
}

void loop()
{
  // Applications are now running in their own RTOS tasks
  // Main loop is free for other system-level tasks if needed
  
  // Small delay to prevent busy looping (though loop() may not be called often)
  delay(1000);
}
