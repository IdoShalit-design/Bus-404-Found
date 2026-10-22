#ifndef STRUCTS_H
#define STRUCTS_H

#include <stddef.h>

// --- Field length limits (shared by config parsing and runtime state) ---
constexpr size_t MAX_STATION_ID_LEN = 24;
constexpr size_t MAX_LINE_LEN = 16;
constexpr size_t MAX_DESTINATION_LEN = 48;
constexpr size_t MAX_ETA_LEN = 6;  // "HH:MM" + terminator

// --- WiFi Credentials Struct ---
struct WifiCredentialsData {
    const char* ssid;
    const char* password;
};

#endif
