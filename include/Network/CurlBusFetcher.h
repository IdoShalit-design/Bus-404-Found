#ifndef CURLBUS_FETCHER_H
#define CURLBUS_FETCHER_H

#include "IBusFetcher.h"

// Note: We don't need the full implementation libraries here if we use pointers,
// but since we don't use pointers for members, we still keep headers usually unless heavily optimized.
#include <HTTPClient.h> 
#include <ArduinoJson.h>

#define CURLBUS_API_URL "https://curlbus.app/"

/**
 * @brief Buffer size for JSON parsing.
 * With stream filtering (only line_name + eta fields), we only need ~2KB
 * instead of the full ~28KB response.
 */
#define CURLBUS_JSON_BUFFER_SIZE 8192

class CurlbusFetcher : public IBusFetcher {
private:
    /**
     * @brief Pre-allocated JSON document buffer.
     * Allocated once in constructor to avoid heap fragmentation from
     * repeated allocations during each API call.
     */
    DynamicJsonDocument* _doc;

public:
    /**
     * @brief Constructor - allocates JSON buffer once.
     */
    CurlbusFetcher();

    /**
     * @brief Destructor - frees JSON buffer.
     */
    ~CurlbusFetcher();

    bool update(BusTarget& bus) override;
};

#endif