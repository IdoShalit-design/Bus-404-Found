#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stddef.h>

#include "RuntimeBuild/Builder.h"
#include "Structs.h"

/**
 * @brief Loads the persisted build state from /build_state.json.
 *
 * @param outState Parsed BuildState on success.
 * @return true on successful load and parse, false otherwise.
 */
bool loadBuildStateConfig(BuildState& outState);

/**
 * @brief Persists the selected build state to /build_state.json.
 *
 * @param state BuildState to persist.
 * @return true when file write succeeds, false otherwise.
 */
bool saveBuildStateConfig(BuildState state);

/**
 * @brief Persists Wi-Fi credentials to /wifi_credentials.json.
 *
 * @param ssid Wi-Fi SSID.
 * @param password Wi-Fi password.
 * @return true when file write succeeds, false otherwise.
 */
bool saveWifiCredentialsConfig(const char* ssid, const char* password);

/**
 * @brief Persists station-only build info to /build_info.json.
 *
 * @param stationId Station identifier.
 * @return true when file write succeeds, false otherwise.
 */
bool saveBuildInfoStationConfig(const char* stationId);

/**
 * @brief Persists station + line numbers build info to /build_info.json.
 *
 * @param stationId Station identifier.
 * @param lines Array of line number strings.
 * @param lineCount Number of lines in the array.
 * @return true when file write succeeds, false otherwise.
 */
bool saveBuildInfoLinesConfig(const char* stationId, const char* const* lines, size_t lineCount);

/**
 * @brief Persists USE_CURRENT_BUILD metadata to /build_info.json.
 *
 * @return true when file write succeeds, false otherwise.
 */
bool saveBuildInfoCurrentConfig();

/**
 * @brief Backward-compatible writer that maps exactly three targets to line-mode build info.
 *
 * @param targets Fixed-size array of 3 BusTarget entries.
 * @return true when file write succeeds, false otherwise.
 */
bool saveBusTargetsConfig(const BusTarget (&targets)[3]);

#endif // CONFIG_MANAGER_H
