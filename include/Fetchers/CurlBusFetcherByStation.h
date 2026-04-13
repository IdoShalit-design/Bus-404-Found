#ifndef CURL_BUS_FETCHER_BY_STATION_H
#define CURL_BUS_FETCHER_BY_STATION_H

#include "IBusFetcher.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

class CurlBusFetcherByStation : public IBusFetcher {
private:
    static constexpr size_t kMaxStationArrivals = 3;

    struct ArrivalData {
        char line[16];
        char destination[48];
        char eta[6];
        bool isRealtime;
        int minutesRemaining;
    };

    DynamicJsonDocument* _doc;
    char _url[64];
    WiFiClientSecure _secureClient;

    ArrivalData _arrivals[kMaxStationArrivals];
    size_t _arrivalCount;
    size_t _nextArrivalIndex;
    char _currentStationId[24];

    bool loadNextArrivals(const char* stationId);

public:
    CurlBusFetcherByStation();
    ~CurlBusFetcherByStation();

    bool update(BusTarget& bus) override;
};

#endif // CURL_BUS_FETCHER_BY_STATION_H
