#ifndef BUS_TYPES_H
#define BUS_TYPES_H

#include <Arduino.h> // For String type

struct BusTarget {
    const char* stationId;      // Station ID
    const char* line;           // Line number
    

    char last_known_ETA[6];     // "HH:MM" format
    int minutes_remaining;      // Numeric value for logic
};

#endif