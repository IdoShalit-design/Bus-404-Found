#ifndef NY_SUBWAY_FETCHER_H
#define NY_SUBWAY_FETCHER_H

#include "IBusFetcher.h"

#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

/**
 * @brief Fills each target with the next two NYC subway trains for its
 *        station, line and direction.
 *
 * Reads the subwayinfo.nyc JSON API, which wraps the MTA GTFS-Realtime feeds:
 *
 *   GET /api/arrivals?station_id=127&line=2&direction=N&limit=2
 *
 * No key is needed. The API allows 10 requests per minute per IP, and every
 * target costs one request per fetch cycle, so three targets every 30 seconds
 * use 6 of them. That is why a failed request is retried only when it never
 * reached the server: a retry after an HTTP error would count against the limit.
 *
 * Targets must carry a stationId (the parent stop ID, e.g. "127", without the
 * N/S suffix), a line (route ID, e.g. "2") and a direction.
 */
class NYSubwayFetcher : public IBusFetcher {
private:
    DynamicJsonDocument* _doc;
    char _url[128];
    WiFiClientSecure _secureClient;

public:
    NYSubwayFetcher();
    ~NYSubwayFetcher();

    NYSubwayFetcher(const NYSubwayFetcher&) = delete;
    NYSubwayFetcher& operator=(const NYSubwayFetcher&) = delete;

    bool update(BusTarget& bus) override;
};

#endif // NY_SUBWAY_FETCHER_H
