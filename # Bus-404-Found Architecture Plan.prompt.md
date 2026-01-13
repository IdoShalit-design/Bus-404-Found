# Bus-404-Found Architecture Plan

## Overview

This document outlines the architecture for a flexible, maintainable bus data fetching system. The design uses the **Strategy Pattern** with dependency injection, allowing data sources to be swapped without changing the rest of the codebase.

## Current State

| Component | Status |
|-----------|--------|
| `IBusFetcher` interface | ✅ Defined |
| `CurlbusFetcher` | ⚠️ Header only, no implementation |
| `TransitClient` | ⚠️ Empty stub files |
| `NetworkManager` | ✅ Working |
| `TimeManager` | ✅ Working |
| Display module | ❌ Not started |

## Target Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        main.cpp                             │
│  - Instantiates concrete fetcher                            │
│  - Injects into TransitClient                               │
│  - Calls update loop                                        │
└─────────────────┬───────────────────────────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────────────────────────┐
│                    TransitClient                            │
│  - Owns BusTarget[] array                                   │
│  - Receives IBusFetcher* via constructor (DI)               │
│  - Orchestrates fetch cycles                                │
└─────────────────┬───────────────────────────────────────────┘
                  │ uses
                  ▼
┌─────────────────────────────────────────────────────────────┐
│                 «interface» IBusFetcher                     │
├─────────────────────────────────────────────────────────────┤
│  + update(BusTarget&) → FetchResult                         │
│  + getName() → const char*                                  │
└─────────────────┬───────────────────────────────────────────┘
                  │ implements
        ┌─────────┼─────────┬─────────────┐
        ▼         ▼         ▼             ▼
┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐
│ Curlbus  │ │ GovIL    │ │ Mock     │ │ Future   │
│ Fetcher  │ │ Fetcher  │ │ Fetcher  │ │ Source   │
└──────────┘ └──────────┘ └──────────┘ └──────────┘
```

## Implementation Steps

### Step 1: Enhance IBusFetcher Interface

**File:** `include/Network/IBusFetcher.h`

Add a richer return type and identification method:

```cpp
struct FetchResult {
    bool success;
    int errorCode;        // 0 = OK, HTTP codes, or custom codes
    String errorMessage;  // Human-readable error
};

class IBusFetcher {
public:
    virtual ~IBusFetcher() = default;
    virtual FetchResult update(BusTarget& bus) = 0;
    virtual const char* getName() const = 0;
};
```

### Step 2: Expand BusTarget Struct

**File:** `include/BusType.h`

```cpp
struct BusTarget {
    const char* stationId;
    const char* line;
    String eta;              // Changed from char[6] for flexibility
    int minutesRemaining;
    bool isValid;            // Flag for stale/error data
    unsigned long lastUpdate; // millis() timestamp
};
```

### Step 3: Implement CurlbusFetcher

**Files:** `include/Network/CurlBusFetcher.h` + `src/Network/CurlBusFetcher.cpp`

- Use `HTTPClient` to call curlbus.app API
- Parse JSON response with `ArduinoJson`
- Extract `expected_arrival_time` and populate `BusTarget`
- Return `FetchResult` with success/failure info

### Step 4: Complete TransitClient

**Files:** `include/Network/TransitClient.h` + `src/Network/TransitClient.cpp`

```cpp
class TransitClient {
private:
    IBusFetcher* _fetcher;      // Injected dependency
    BusTarget* _targets;
    size_t _targetCount;
    unsigned long _lastFetchTime;
    unsigned long _fetchInterval; // e.g., 30000ms

public:
    TransitClient(IBusFetcher* fetcher, BusTarget* targets, size_t count);
    void setFetchInterval(unsigned long ms);
    void fetchAll();             // Updates all targets
    bool shouldFetch();          // Checks if interval elapsed
};
```

### Step 5: Wire Up in main.cpp

```cpp
// In setup():
CurlbusFetcher* fetcher = new CurlbusFetcher();
TransitClient* transit = new TransitClient(fetcher, busTargets, TARGET_COUNT);

// In loop():
if (transit->shouldFetch()) {
    transit->fetchAll();
    // Update display...
}
```

### Step 6: Add MockFetcher for Testing

**File:** `include/Network/MockFetcher.h`

```cpp
class MockFetcher : public IBusFetcher {
public:
    FetchResult update(BusTarget& bus) override {
        bus.eta = "12:34";
        bus.minutesRemaining = 5;
        bus.isValid = true;
        return {true, 0, ""};
    }
    const char* getName() const override { return "Mock"; }
};
```

## How to Add a New Data Source

1. Create a new class that inherits from `IBusFetcher`
2. Implement `update(BusTarget&)` with your API logic
3. Implement `getName()` for logging
4. Instantiate your fetcher in `main.cpp` and inject into `TransitClient`

Example for Israeli Government API:
```cpp
class GovIlFetcher : public IBusFetcher {
    FetchResult update(BusTarget& bus) override;
    const char* getName() const override { return "GovIL"; }
};
```

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Error handling | Return struct | Exceptions not ideal on ESP32; bool alone lacks detail |
| Dependency injection | Constructor injection | Simple, testable, no runtime overhead |
| Update interval | Managed by TransitClient | Centralized control, easy to adjust |
| Memory | Heap allocation for managers | Flexibility; ESP32 has sufficient RAM |

## Future Considerations

- [ ] Add display module (`DisplayManager`) with similar interface pattern
- [ ] Configuration via web interface or SD card
- [ ] OTA updates
- [ ] Multiple fetcher fallback (try GovIL if CurlBus fails)
