```mermaid
---
title: Bus-404-Found Architecture
---
classDiagram
    direction TB

    namespace Network_Module {
        class NetworkManager {
            -WifiCredentials _credentials
            +NetworkManager(creds)
            +connect_to_wifi() bool
            +print_networks() void
            +print_wifi_status() void
        }
        class WifiCredentials {
            <<struct>>
            +const char* ssid
            +const char* password
            +WifiCredentials(s, p)
        }
    }

    namespace System_Module {
        class TimeManager {
            -const char* _timezone
            -const char* _ntpServer
            +TimeManager(tz)
            +init_and_sync() void
            +is_time_set() bool
            +get_formatted_time() String
            +get_minutes_until(eta) int
        }
    }

    namespace Data_Fetching_Module {
        class IBusFetcher {
            <<interface>>
            +update(BusTarget bus) FetchResult*
            +getName() const char**
        }
        class FetchResult {
            <<struct>>
            +bool success
            +int errorCode
            +String errorMessage
        }
        class CurlbusFetcher {
            -const char* _apiBase
            +update(BusTarget bus) FetchResult
            +getName() const char*
        }
        class MockFetcher {
            +update(BusTarget bus) FetchResult
            +getName() const char*
        }
        class GovIlFetcher {
            <<Future>>
            +update(BusTarget bus) FetchResult
            +getName() const char*
        }
    }

    namespace Data_Logic_Module {
        class TransitClient {
            -IBusFetcher* _fetcher
            -BusTarget* _targets
            -size_t _targetCount
            -unsigned long _lastFetchTime
            -unsigned long _fetchInterval
            +TransitClient(fetcher, targets, count)
            +setFetchInterval(ms) void
            +fetchAll() void
            +shouldFetch() bool
        }
        class BusTarget {
            <<struct>>
            +const char* stationId
            +const char* line
            +String eta
            +int minutesRemaining
            +bool isValid
            +unsigned long lastUpdate
        }
    }

    namespace Display_Module {
        class DisplayManager {
            <<Future>>
            +init() void
            +render() void
        }
    }

    %% Interface implementations (Strategy Pattern)
    IBusFetcher <|.. CurlbusFetcher : implements
    IBusFetcher <|.. MockFetcher : implements
    IBusFetcher <|.. GovIlFetcher : implements
    IBusFetcher ..> FetchResult : returns

    %% Main application relationships
    Main --> NetworkManager : owns
    Main --> TimeManager : owns
    Main --> TransitClient : owns
    Main --> IBusFetcher : creates concrete

    %% Dependencies
    NetworkManager ..> WifiCredentials : uses
    TransitClient --> IBusFetcher : uses (DI)
    TransitClient o-- BusTarget : manages array
    CurlbusFetcher ..> BusTarget : updates
    MockFetcher ..> BusTarget : updates
