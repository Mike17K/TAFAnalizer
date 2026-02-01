# Application Development Guidelines

## Creating a New Application

### 1. Inherit from ApplicationInterface
All applications must inherit from `ApplicationInterface`:
```cpp
class MyApp : public ApplicationInterface {
  // Implementation
};
```

### 2. Implement Required Methods
- `setup()` - Initialize hardware, configure settings
- `loop()` - Main application logic (called by RTOS task)
- `getStatus()` - Return current application state

### 3. Dependency Injection
Set layers using chained setters:
```cpp
myApp->setNetworkLayer(networkLayer)->setDataLayer(dataLayer);
```

### 4. Initialize in main.cpp
```cpp
// Create instance
ApplicationInterface* myApp = new MyApp();
myApp->setNetworkLayer(networkLayer)->setDataLayer(dataLayer);

// Setup
if (!myApp->setup()) {
  Serial.println("Failed to setup MyApp");
  delete myApp;
  myApp = nullptr;
}

// Create RTOS task
if (myApp) {
  myApp->createTask("MyApp", stackSize, priority, affinity, delayMs);
}
```

## RTOS Task Parameters
- **Name**: Descriptive, PascalCase (e.g., "BluetoothApp")
- **Stack Size**: 4096 bytes minimum (adjust based on needs)
- **Priority**: 1-3 (higher = more important)
- **Affinity**: `tskNO_AFFINITY` (any core) or 0/1 for specific core
- **Delay**: Loop delay in ms (determines update frequency)

## File Structure
```
src/layers/application/myapp/
├── MyApp.h          # Header with class declaration
├── MyApp.cpp        # Implementation
└── README.md        # Documentation (optional)
```

## Best Practices
- Keep applications loosely coupled
- Use NetworkLayer for inter-app communication
- Store persistent data in DataLayer
- Don't block in `loop()` - use non-blocking operations
- Clean up resources in destructor
