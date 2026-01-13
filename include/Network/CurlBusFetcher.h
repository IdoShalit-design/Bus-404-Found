#ifndef CURLBUS_FETCHER_H
#define CURLBUS_FETCHER_H

#include "IBusFetcher.h"

// Note: We don't need the full implementation libraries here if we use pointers,
// but since we don't use pointers for members, we still keep headers usually unless heavily optimized.
#include <HTTPClient.h> 
#include <ArduinoJson.h>

class CurlbusFetcher : public IBusFetcher {
public:
    bool update(BusTarget& bus) override;
};

#endif