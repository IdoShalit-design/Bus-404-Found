#ifndef CURLBUS_FETCHER_H
#define CURLBUS_FETCHER_H

#include "IBusFetcher.h"

#include <HTTPClient.h> 
#include <ArduinoJson.h>


#define CURLBUS_API_URL "https://curlbus.app/"
#define CURLBUS_JSON_BUFFER_SIZE 8192

#define CURLBUS_URL_BUFFER_SIZE 64
#define CURLBUS_PAYLOAD_BUFFER_SIZE 2048

class CurlbusFetcher : public IBusFetcher {
private:

    // Pre-allocated JSON document buffer.
    DynamicJsonDocument* _doc;

    // Pre-allocated buffer for building request URL.
    char _url[CURLBUS_URL_BUFFER_SIZE];

    // Pre-allocated buffer for HTTP response payload.
    
    char _payload[CURLBUS_PAYLOAD_BUFFER_SIZE];

public:

    // Constructor - allocates JSON buffer once.

    CurlbusFetcher();

    /**
     * @brief Destructor - frees JSON buffer.
     */
    ~CurlbusFetcher();

    bool update(BusTarget& bus) override;
};

#endif