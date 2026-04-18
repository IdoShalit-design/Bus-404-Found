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
            -TimeManager time_manager
            -IRenderer* renderer
            -IBusFetcher* bus_fetcher
            -BusTarget bus_targets[]
            -ulong last_fetch_time
            +setup() void
            +loop() void
            -createFetcher() IBusFetcher*
        }
    }

    %% ============================================
    %% include/ - Root Level Headers
    %% ============================================
    namespace include {
        class Structs {
            <<Structs.h>>
            +BusTarget
            +WifiCredentialsData
        }
        class Config {
            <<Config.h>>
            +FETCHER_TYPE
            +MY_TARGETS[]
            +TARGETS_COUNT
            +TIME_ZONE
            +SCREEN_DEBUG
        }
        class Secrets {
            <<Secrets.h>>
            +WIFI_CREDENTIALS WifiCredentialsData
        }
        class TimeManager {
            <<TimeManager.h>>
            -const char* _timezone
            -const char* _ntpServer
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
        class WifiCredentials {
            <<NetworkManager.h>>
            +const char* ssid
            +const char* password
            +WifiCredentials(s, p)
        }
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
            +update(BusTarget bus) bool
        }
        class CurlBuseFetcherByLine {
            <<CurlBuseFetcherByLine.h>>
            -DynamicJsonDocument* _doc
            -char _url[]
            -WiFiClientSecure _secureClient
            +CurlBuseFetcherByLine()
            +~CurlBuseFetcherByLine()
            +update(BusTarget bus) bool
        }
    }

    %% ============================================
    %% include/Display/ - Display Module Headers
    %% ============================================
    namespace include_Display {
        class IRenderer {
            <<IRenderer.h>>
            +init() bool
            +render(BusTarget* targets, int count) void
            +clear() void
            +setBrightness(uint8_t) void
        }
        class HUB75Display {
            <<HUB75Display.h>>
            -MatrixPanel_I2S_DMA* _matrix
            +HUB75Display()
            +~HUB75Display()
            +init() bool
            +render(BusTarget* targets, int count) void
            +clear() void
            +setBrightness(uint8_t) void
            +screen_tests() void
            -drawBusRow(BusTarget target, int row) void
            -drawIcon(int x, int y, uint16_t color) void
        }
    }

    %% ============================================
    %% Future Modules
    %% ============================================
    namespace Future {
        class MockFetcher {
            <<Testing>>
            +update(BusTarget bus) bool
        }
        class GovIlFetcher {
            <<Alternative API>>
            +update(BusTarget bus) bool
        }
    }

    %% Interface implementations (Strategy Pattern)
    IBusFetcher <|.. CurlBuseFetcherByLine : implements
    IBusFetcher <|.. MockFetcher : implements
    IBusFetcher <|.. GovIlFetcher : implements

    %% Main application relationships
    Main --> TimeManager : owns
    Main --> IBusFetcher : owns
    Main --> IRenderer : owns
    Main ..> NetworkManager : creates locally
    HUB75Display ..|> IRenderer : implements

    %% Config/Secrets dependencies
    Main ..> Config : uses
    Config ..> Structs : uses
    Config ..> Secrets : uses

    %% Network dependencies
    NetworkManager --> WifiCredentials : has
    Main ..> WifiCredentials : creates
    WifiCredentials ..> WifiCredentialsData : constructed from

    %% Data flow
    CurlBuseFetcherByLine ..> BusTarget : updates
    MockFetcher ..> BusTarget : updates
    HUB75Display ..> BusTarget : renders
