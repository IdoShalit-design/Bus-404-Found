#ifndef STRUCTS_H
#define STRUCTS_H

// --- WiFi Credentials Struct ---
struct WifiCredentialsData {
    const char* ssid;
    const char* password;
};

// --- Bus Target Struct ---
struct BusTarget {
    const char* stationId;      // Station ID
    const char* line;           // Line number
    char destination[48];       // Destination display name (updated from API)
    bool is_realtime;           // True if real-time data
    char last_known_ETA[6];     // "HH:MM" format
    int minutes_remaining;      // Numeric value for logic
    bool no_data;               // True if last fetch returned no data
};

#endif
