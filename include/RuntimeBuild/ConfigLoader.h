#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <Arduino.h>
#include "Config.h"
#include "RuntimeBuild/Builder.h"

// Station/line/destination lengths live in Structs.h, shared with BusTarget.
constexpr size_t MAX_WIFI_SSID_LEN = 33;
constexpr size_t MAX_WIFI_PASSWORD_LEN = 65;
constexpr uint8_t CONFIG_SCHEMA_VERSION = 1;

struct WifiConfig {
    char ssid[MAX_WIFI_SSID_LEN];
    char password[MAX_WIFI_PASSWORD_LEN];
};

// BusTarget owns its strings, so parsed config doubles as the live working set;
// no separate config-side target struct or copy step is needed.
struct BusConfig {
    BusTarget targets[MAX_RUNTIME_TARGETS];
    int targetCount;
};

struct RuntimeConfig {
    BuildState buildState;
    BuildState concreteBuildState;
    WifiConfig wifi;
    BusConfig bus;
};

/**
 * @brief Outcome of a config load or resolve step.
 *
 * Allocation-free: `error` points at a string literal with static storage
 * duration, so callers need no buffer and the pointer stays valid.
 */
struct ConfigResult {
    bool ok;
    const char* error;  // nullptr when ok is true
};

/**
 * @brief Loads and validates runtime configuration from LittleFS JSON files.
 *
 * Reads Wi-Fi credentials, requested build state, resolves concrete build state,
 * and parses mode-specific build info payload into outConfig.
 *
 * @param outConfig Destination config object.
 * @return ok when all config sources are valid and loaded; otherwise error text.
 */
ConfigResult loadRuntimeConfig(RuntimeConfig& outConfig);

/**
 * @brief Resolves requested build state into an executable concrete build state.
 *
 * If config.buildState is concrete, that state is returned unchanged.
 * If config.buildState is USE_CURRENT_BUILD, the resolver loads the persisted
 * last concrete mode from /last_concrete_build_state.json.
 *
 * @param config Source runtime config containing requested buildState.
 * @param outState Resolved concrete state on success.
 * @return ok when resolution succeeds; error when the persisted fallback is
 *         missing or invalid.
 */
ConfigResult resolveConcreteBuildState(const RuntimeConfig& config, BuildState& outState);

/**
 * @brief Maps internal config error text to short display-friendly user messages.
 *
 * @param configError Raw parser/loader error string.
 * @return Static display string intended for screen output.
 */
const char* configErrorToDisplayMessage(const char* configError);

#endif
