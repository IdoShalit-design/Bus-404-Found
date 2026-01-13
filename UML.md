```mermaid
---
title: Bus-404-Found Architecture
---
classDiagram
    direction TB

    %% ============================================
    %% src/ - Main Application Entry Point
    %% ============================================
    namespace src {
        class Main {
            <<main.cpp>>
            +setup() void
            +loop() void
        }
    }

    %% ============================================
    %% include/ - Root Level Headers
    %% ============================================
    namespace include {
        class BusTarget {
            <<BusType.h>>
            +string stationId
            +string line
            +String eta
            +int minutesRemaining
            +bool isValid
            +ulong lastUpdate
        }
        class FetchResult {
            <<BusType.h>>
            +bool success
            +int errorCode
            +String errorMessage
        }
        class WifiCredentials {
            <<Config.h>>
            +string ssid
            +string password
        }
        class TimeManager {
            <<TimeManager.h>>
            -string _timezone
            -string _ntpServer
            +TimeManager(tz)
            +init_and_sync() void
            +is_time_set() bool
            +get_formatted_time() String
            +get_minutes_until(eta) int
        }
    }

    %% ============================================
    %% include/Network/ - Network Module Headers
    %% ============================================
    namespace include_Network {
        class NetworkManager {
            <<NetworkManager.h>>
            -WifiCredentials _credentials
            +NetworkManager(creds)
            +connect_to_wifi() bool
            +print_networks() void
            +print_wifi_status() void
        }
        class IBusFetcher {
            <<IBusFetcher.h>>
            +update(BusTarget bus) FetchResult
            +getName() string
        }
        class CurlbusFetcher {
            <<CurlBusFetcher.h>>
            -string _apiBase
            +update(BusTarget bus) FetchResult
            +getName() string
        }
        class TransitClient {
            <<TransitClient.h>>
            -IBusFetcher _fetcher
            -BusTarget _targets
            -size_t _targetCount
            -ulong _lastFetchTime
            -ulong _fetchInterval
            +TransitClient(fetcher, targets, count)
            +setFetchInterval(ms) void
            +fetchAll() void
            +shouldFetch() bool
        }
    }

    %% ============================================
    %% Future Modules
    %% ============================================
    namespace Future {
        class MockFetcher {
            <<Testing>>
            +update(BusTarget bus) FetchResult
            +getName() string
        }
        class GovIlFetcher {
            <<Alternative API>>
            +update(BusTarget bus) FetchResult
            +getName() string
        }
        class DisplayManager {
            <<LED Display>>
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
    TransitClient --> IBusFetcher : uses DI
    TransitClient o-- BusTarget : manages array
    CurlbusFetcher ..> BusTarget : updates
    MockFetcher ..> BusTarget : updates
