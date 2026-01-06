classDiagram
    class NetworkManager {
        -WifiCredentials credentials
        +connect_to_wifi() bool
        +is_connected() bool
        +print_status()
    }

    class TimeManager {
        -const char* timezone
        +init_and_sync()
        +get_minutes_until(String eta) int
        +get_formatted_time() String
    }

    class TransitClient {
        -NetworkManager* network
        -BusTarget* targets
        -int target_count
        +fetchAllArrivals()
        -fetchSingleETA(String sId, String line) String
    }

    class BusTarget {
        <<struct>>
        +String stationId
        +String line
        +String lastKnownETA
    }

    class Main {
        +setup()
        +loop()
    }

    Main --> NetworkManager : owns
    Main --> TimeManager : owns
    Main --> TransitClient : owns
    TransitClient ..> NetworkManager : uses
    TransitClient o-- BusTarget : manages