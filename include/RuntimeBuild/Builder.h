#ifndef BUILDER_H
#define BUILDER_H

#include <memory>

#include "Network/IBusFetcher.h"
#include "Display/IRenderer.h"

typedef enum BuildState {
    BUS_BY_STATION = 0,
    BUS_BY_LINES = 1,
    NY_METRO_BY_STATION = 2,
    USE_CURRENT_BUILD,
} BuildState;

#define WIFI_CREDENTIAL_MAX_LEN 64
#define MAX_BUS_TARGETS 3
#define STATION_ID_MAX_LEN 32

/**
 * @brief Holds the complete application configuration.
 */
typedef struct AppConfig {
    BuildState state;
    char wifiSSID[WIFI_CREDENTIAL_MAX_LEN];
    char wifiPassword[WIFI_CREDENTIAL_MAX_LEN];
    union {
        BusTarget lines[MAX_BUS_TARGETS];
        char stationid[STATION_ID_MAX_LEN];
    };
} AppConfig;

bool build(BuildState state, std::unique_ptr<IBusFetcher>& fetcher, std::unique_ptr<IRenderer>& renderer);

#endif // BUILDER_H
