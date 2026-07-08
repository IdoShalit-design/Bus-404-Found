#ifndef CURL_BUSE_FETCHER_BY_LINE_H
#define CURL_BUSE_FETCHER_BY_LINE_H

#include "IBusFetcher.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>


#define CURLBUS_API_URL "https://curlbus.app/"
#define CURLBUS_JSON_BUFFER_SIZE 16384

#define CURLBUS_URL_BUFFER_SIZE 64

class CurlBuseFetcherByLine : public IBusFetcher {
private:

    // Pre-allocated JSON document buffer.
    DynamicJsonDocument* _doc;

    // Pre-allocated buffer for building request URL.
    char _url[CURLBUS_URL_BUFFER_SIZE];

    // Secure WiFi client for HTTPS.
    WiFiClientSecure _secureClient;

public:

    // Constructor - allocates JSON buffer once.
    CurlBuseFetcherByLine();

    /**
     * @brief Destructor - frees JSON buffer.
     */
    ~CurlBuseFetcherByLine();

    bool update(BusTarget& bus) override;
};

#endif
