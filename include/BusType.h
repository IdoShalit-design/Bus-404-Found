#ifndef BUS_TYPES_H
#define BUS_TYPES_H

#include <Arduino.h> // For String type

// Maximum number of upcoming arrivals to track per line
#define MAX_ARRIVALS 2

struct ArrivalInfo {
    bool is_realtime;           // True if real-time data
    char eta[6];                // "HH:MM" format
    int minutes_remaining;      // Numeric value for logic
};

struct BusTarget {
    const char* stationId;      // Station ID
    const char* line;           // Line number

    ArrivalInfo arrivals[MAX_ARRIVALS];
    int arrival_count;          // Number of valid arrivals (0..MAX_ARRIVALS)
};

#endif