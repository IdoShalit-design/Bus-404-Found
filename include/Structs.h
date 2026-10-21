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

/**
 * @brief One tracked bus target: what to look up, plus its latest arrival data.
 *
 * Owns all of its strings. Fetchers must copy into these buffers rather than
 * reassigning pointers -- an earlier version stored const char* here, which let
 * a fetcher point `line` at its own internal arrival cache. Refilling that cache
 * then mutated the target's line out from under the renderer while destination
 * and ETA still held the previous values.
 */
struct BusTarget {
    char stationId[MAX_STATION_ID_LEN];      // Station ID
    char line[MAX_LINE_LEN];                 // Line number
    char destination[MAX_DESTINATION_LEN];   // Destination display name (updated from API)

    // --- Next arrival (the one the row leads with) ---
    bool is_realtime;                        // True if real-time data
    char last_known_ETA[MAX_ETA_LEN];        // "HH:MM" format
    int minutes_remaining;                   // Numeric value for logic
    bool no_data;                            // True if last fetch returned no data

    // --- Arrival after that, shown alongside the first on the same row ---
    // has_next_arrival is the only valid-flag for this group: a station can
    // legitimately expose one upcoming arrival and no second one, which is not
    // an error and must not blank the first.
    bool next_is_realtime;
    char next_ETA[MAX_ETA_LEN];
    int next_minutes_remaining;
    bool has_next_arrival;
};

#endif
