#ifndef CURL_BUS_FETCHER_BY_STATION_H
#define CURL_BUS_FETCHER_BY_STATION_H

#include "IBusFetcher.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "Config.h"

/**
 * @brief Fills each target with one line of a single station, two arrivals deep.
 *
 * One station feeds every row, so the arrivals are fetched once and handed out
 * across the update() calls of a fetch cycle. Each call takes the earliest line
 * not already given to another row and attaches that line's following arrival,
 * which is why a line's second arrival never costs a row of its own.
 */
class CurlBusFetcherByStation : public IBusFetcher {
private:
    // Deep enough that a handful of lines can each contribute two arrivals and
    // still leave distinct lines for the remaining rows.
    static constexpr size_t kMaxStationArrivals = 12;

    // One row per target, so a cycle can never serve more lines than that.
    static constexpr size_t kMaxServedLines = MAX_RUNTIME_TARGETS;

    struct ArrivalData {
        char line[MAX_LINE_LEN];
        char destination[MAX_DESTINATION_LEN];
        char eta[MAX_ETA_LEN];
        bool is_realtime;
        int minutes_remaining;
    };

    DynamicJsonDocument* _doc;
    char _url[64];
    WiFiClientSecure _secureClient;

    ArrivalData _arrivals[kMaxStationArrivals];
    size_t _arrivalCount;
    char _currentStationId[MAX_STATION_ID_LEN];
    unsigned long _loadedAtMs;

    // Lines already handed to a row since the last load, so the next row skips
    // them instead of repeating a line the display already shows.
    char _servedLines[kMaxServedLines][MAX_LINE_LEN];
    size_t _servedLineCount;

    bool loadNextArrivals(const char* stationId);

    /** @brief True when this line was already given to a row this cycle. */
    bool isLineServed(const char* line) const;

    /**
     * @brief Index of the earliest arrival whose line is still unclaimed.
     * @return -1 when every cached line is spoken for, or the served list is full.
     */
    int findUnservedArrivalIndex() const;

public:
    CurlBusFetcherByStation();
    ~CurlBusFetcherByStation();

    bool update(BusTarget& bus) override;
};

#endif // CURL_BUS_FETCHER_BY_STATION_H
