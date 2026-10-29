#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stddef.h>

#include "RuntimeBuild/Builder.h"

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
 * @brief Loads the last concrete (non-USE_CURRENT_BUILD) state from /last_concrete_build_state.json.
 *
 * @param outState Parsed BuildState on success.
 * @return true on successful load and parse, false otherwise.
 */
bool loadLastConcreteBuildStateConfig(BuildState& outState);

/**
 * @brief Persists the last concrete (non-USE_CURRENT_BUILD) state to /last_concrete_build_state.json.
 *
 * @param state BuildState to persist.
 * @return true when file write succeeds, false otherwise.
 */
bool saveLastConcreteBuildStateConfig(BuildState state);

/**
 * @brief Persists Wi-Fi credentials to /wifi_credentials.json.
 *
 * @param ssid Wi-Fi SSID.
 * @param password Wi-Fi password.
 * @return true when file write succeeds, false otherwise.
 */
bool saveWifiCredentialsConfig(const char* ssid, const char* password);

/**
 * @brief Loads previously persisted Wi-Fi credentials from /wifi_credentials.json.
 *
 * @param ssidOut Destination buffer for the SSID.
 * @param ssidOutLen Size of ssidOut in bytes.
 * @param passwordOut Destination buffer for the password.
 * @param passwordOutLen Size of passwordOut in bytes.
 * @return true when a valid saved config was found and copied out, false otherwise.
 */
bool loadWifiCredentialsConfig(char* ssidOut, size_t ssidOutLen, char* passwordOut, size_t passwordOutLen);

/**
 * @brief Persists station-only build info to /build_info.json.
 *
 * @param stationId Station identifier.
 * @return true when file write succeeds, false otherwise.
 */
bool saveBuildInfoStationConfig(const char* stationId);

/**
 * @brief Persists station+line target pairs build info to /build_info.json.
 *
 * Each entry i pairs stationIds[i] with lines[i], allowing up to 3
 * independent (station, line) targets rather than one shared station.
 *
 * @param stationIds Array of station identifier strings.
 * @param lines Array of line number strings, parallel to stationIds.
 * @param targetCount Number of entries in both arrays.
 * @param directions Optional array of "N"/"S" strings, parallel to stationIds.
 *        NY subway mode passes it; bus line mode passes nullptr.
 * @return true when file write succeeds, false otherwise.
 */
bool saveBuildInfoLinesConfig(const char* const* stationIds, const char* const* lines, size_t targetCount,
                              const char* const* directions = nullptr);

#endif // CONFIG_MANAGER_H
