#include "RuntimeBuild/Builder.h"

#include <Arduino.h>
#include <memory>

#include "Display/HUB75Display.h"
#include "Fetchers/CurlBusFetcherByStation.h"
#include "Fetchers/CurlBusFetcherByLine.h"
#include "Fetchers/MetroFetcherStub.h"

bool build(BuildState state, std::unique_ptr<IBusFetcher>& fetcher, std::unique_ptr<IRenderer>& renderer) {
    switch (state) {
        case BUS_BY_STATION:
            Serial.println("Selected state: BUS_BY_STATION");
            fetcher = std::unique_ptr<IBusFetcher>(new CurlBusFetcherByStation());
            return true;
        case BUS_BY_LINES:
            Serial.println("Selected state: BUS_BY_LINES");
            fetcher = std::unique_ptr<IBusFetcher>(new CurlBusFetcherByLine());
            // Do not create a renderer here: setup() already initialized the
            // HUB75 display. Replacing it would release the I2S/DMA bus and
            // re-allocate the framebuffer after WiFi has claimed internal RAM,
            // which fails and leaves the panel permanently black.
            return true;
        case NY_METRO_BY_STATION:
            Serial.println("Selected state: NY_METRO_BY_STATION");
            fetcher = std::unique_ptr<IBusFetcher>(new MetroFetcherStub());
            return true;
        case USE_CURRENT_BUILD:
            Serial.println("[Build] USE_CURRENT_BUILD must be resolved before build()");
            return false;
        default:
            Serial.println("Selected state: UNKNOWN");
            return false;
    }
}
