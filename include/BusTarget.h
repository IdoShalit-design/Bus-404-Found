#ifndef BUS_TARGET_H
#define BUS_TARGET_H

#include <stdint.h>

#include "Structs.h"

/**
 * @brief Travel direction of a target, as subway feeds report it.
 *
 * None is the default: bus stations already imply a direction, so bus
 * targets never set it.
 */
enum class Direction : uint8_t {
    None,
    North,
    South,
};

/**
 * @brief One tracked bus target: what to look up, plus its latest arrival data.
 *
 * Owns all of its strings. Setters copy into these buffers rather than keeping
 * the caller's pointer -- an earlier version stored const char* here, which let
 * a fetcher point `line` at its own internal arrival cache. Refilling that cache
 * then mutated the target's line out from under the renderer while destination
 * and ETA still held the previous values. Keeping the buffers private makes that
 * copy the only way in.
 */
class BusTarget {
public:
    BusTarget();

    // --- Lookup keys and row label ---
    const char* stationId() const { return _stationId; }
    const char* line() const { return _line; }
    const char* destination() const { return _destination; }   // Updated from API
    Direction direction() const { return _direction; }

    void setStationId(const char* stationId);
    void setLine(const char* line);
    void setDestination(const char* destination);
    void setDirection(Direction direction) { _direction = direction; }

    // --- Next arrival (the one the row leads with) ---
    bool isRealtime() const { return _isRealtime; }
    const char* eta() const { return _eta; }                   // "HH:MM" format
    int minutesRemaining() const { return _minutesRemaining; }
    bool noData() const { return _noData; }                    // Last fetch returned no data

    /**
     * @brief Records the leading arrival and starts a new pair.
     *
     * Clears the following arrival and the no-data flag, so a fetcher that
     * finds only one arrival cannot leave the previous cycle's second one on
     * the row.
     */
    void setArrival(const char* eta, int minutesRemaining, bool isRealtime);
    void setNoData(bool noData) { _noData = noData; }

    // --- Arrival after that, shown alongside the first on the same row ---
    // hasNextArrival() is the only valid-flag for this group: a station can
    // legitimately expose one upcoming arrival and no second one, which is not
    // an error and must not blank the first.
    bool hasNextArrival() const { return _hasNextArrival; }
    bool nextIsRealtime() const { return _nextIsRealtime; }
    const char* nextEta() const { return _nextEta; }
    int nextMinutesRemaining() const { return _nextMinutesRemaining; }

    void setNextArrival(const char* eta, int minutesRemaining, bool isRealtime);
    void clearNextArrival();

    /**
     * @brief Blanks the destination and both arrivals; keeps the lookup keys
 *        (station, line and direction).
     *
     * Parsed config supplies only station and line, so this leaves a target
     * well-defined before its first fetch.
     */
    void resetArrivals();

private:
    char _stationId[MAX_STATION_ID_LEN];
    char _line[MAX_LINE_LEN];
    char _destination[MAX_DESTINATION_LEN];
    Direction _direction;

    bool _isRealtime;
    char _eta[MAX_ETA_LEN];
    int _minutesRemaining;
    bool _noData;

    bool _nextIsRealtime;
    char _nextEta[MAX_ETA_LEN];
    int _nextMinutesRemaining;
    bool _hasNextArrival;
};

#endif
