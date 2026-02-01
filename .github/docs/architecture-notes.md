# Architecture Notes

## Layer Hierarchy
```
Application Layer (Bluetooth, LED, MPU, etc.)
       ↓ uses
Data Layer (Key-Value Store, Cleanup)
       ↓ uses
Network Layer (Pub/Sub Messaging)
```

**Rule**: Upper layers can use lower layers, but NOT vice versa.

## Network Layer
- **Purpose**: Topic-based pub/sub messaging between applications
- **Location**: `src/layers/network/`
- **Pattern**: Observer pattern with direct callback delivery
- **Usage**: 
  - `publish(topic, data)` - Send message
  - `subscribe(topic, callback)` - Register listener

## Data Layer
- **Purpose**: Redis-like key-value store with automatic cleanup
- **Location**: `src/layers/data/`
- **Features**: TTL support, periodic cleanup task
- **Usage**:
  - `set(key, value, ttl)` - Store data
  - `get(key)` - Retrieve data
  - `remove(key)` - Delete data

## Application Layer
- **Purpose**: Specific functionality modules (hardware control, communication)
- **Location**: `src/layers/application/`
- **Base**: All applications implement `ApplicationInterface`
- **Independence**: Applications should be modular and loosely coupled

## RTOS Design
- Each application runs in its own FreeRTOS task
- Tasks run concurrently on dual-core ESP32
- Main `loop()` is nearly empty - applications self-manage
- Use task delays for frequency control

## Communication Patterns
- **Inter-app**: Use NetworkLayer pub/sub
- **State sharing**: Use DataLayer key-value store
- **Direct calls**: Avoid - breaks modularity

## Design Principles
1. **Separation of Concerns**: Each layer has distinct responsibility
2. **Dependency Injection**: Layers injected via setters
3. **Fail-Safe**: Nullptr checks, error logging, graceful degradation
4. **Concurrency**: RTOS tasks for parallel operations
