```mermaid
classDiagram
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

    namespace Data_Logic_Module {
        class TransitClient {
            -NetworkManager* _network
            -BusTarget* _bus_targets
            -int _target_count
            +TransitClient(targets, count, network)
            +fetchAllArrivals() void
            -fetchSingleETA(sId, line) String
        }
        class BusTarget {
            <<struct>>
            +const char* stationId
            +const char* line
            +String lastKnownETA
            +int minutesRemaining
        }
    }

    namespace Display_Module {
        class DisplayManager {
            <<Future>>
            +init() void
            +render() void
        }
    }

    %% Relationships based on actual code
    Main --> NetworkManager : owns (Pointer)
    Main --> TimeManager : owns (Static)
    Main --> TransitClient : owns (Pointer)
    
    NetworkManager ..> WifiCredentials : uses
    TransitClient --> NetworkManager : uses (Dependency Injection)
    TransitClient o-- BusTarget : manages array
