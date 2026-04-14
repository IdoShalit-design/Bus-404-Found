#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <Arduino.h>
#include "Config.h"
#include "RuntimeBuild/Builder.h"

constexpr size_t MAX_WIFI_SSID_LEN = 33;
constexpr size_t MAX_WIFI_PASSWORD_LEN = 65;
constexpr size_t MAX_STATION_ID_LEN = 24;
constexpr size_t MAX_LINE_LEN = 16;
constexpr size_t MAX_DESTINATION_LEN = 48;
constexpr uint8_t CONFIG_SCHEMA_VERSION = 1;

struct WifiConfig {
    char ssid[MAX_WIFI_SSID_LEN];
    char password[MAX_WIFI_PASSWORD_LEN];
};

struct BusTargetConfig {
    char stationId[MAX_STATION_ID_LEN];
    char line[MAX_LINE_LEN];
    char destination[MAX_DESTINATION_LEN];
};

struct BusConfig {
    BusTargetConfig targets[MAX_RUNTIME_TARGETS];
    int targetCount;
};

struct RuntimeConfig {
    BuildState buildState;
    BuildState concreteBuildState;
    WifiConfig wifi;
    BusConfig bus;
};

/**
 * @brief Loads and validates runtime configuration from LittleFS JSON files.
 *
 * Reads Wi-Fi credentials, requested build state, resolves concrete build state,
 * and parses mode-specific build info payload into outConfig.
 *
 * @param outConfig Destination config object.
 * @param errorBuffer Optional output buffer for short human-readable error text.
 * @param errorBufferLen Size of errorBuffer in bytes.
 * @return true when all config sources are valid and loaded, false on any error.
 */
bool loadRuntimeConfig(RuntimeConfig& outConfig, char* errorBuffer, size_t errorBufferLen);

/**
 * @brief Resolves requested build state into an executable concrete build state.
 *
 * If config.buildState is concrete, that state is returned unchanged.
 * If config.buildState is USE_CURRENT_BUILD, the resolver loads the persisted
 * last concrete mode from /last_concrete_build_state.json.
 *
 * @param config Source runtime config containing requested buildState.
 * @param outState Resolved concrete state on success.
 * @param errorBuffer Optional output buffer for short human-readable error text.
 * @param errorBufferLen Size of errorBuffer in bytes.
 * @return true when resolution succeeds, false when persisted fallback is missing/invalid.
 */
bool resolveConcreteBuildState(const RuntimeConfig& config, BuildState& outState, char* errorBuffer, size_t errorBufferLen);

/**
 * @brief Maps internal config error text to short display-friendly user messages.
 *
 * @param configError Raw parser/loader error string.
 * @return Static display string intended for screen output.
 */
const char* configErrorToDisplayMessage(const char* configError);

#endif
