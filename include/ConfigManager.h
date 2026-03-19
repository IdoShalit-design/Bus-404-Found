#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "Builder.h"
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
 * @brief Persists exactly three bus targets to /bus_targets.json.
 *
 * @param targets Fixed-size array of 3 BusTarget entries.
 * @return true when file write succeeds, false otherwise.
 */
bool saveBusTargetsConfig(const BusTarget (&targets)[3]);

#endif // CONFIG_MANAGER_H
