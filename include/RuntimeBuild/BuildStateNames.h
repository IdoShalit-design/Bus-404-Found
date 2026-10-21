#ifndef BUILD_STATE_NAMES_H
#define BUILD_STATE_NAMES_H

#include "RuntimeBuild/Builder.h"

/**
 * @brief Single owner of the BuildState <-> persisted-name mapping.
 *
 * These names appear in /build_state.json, /last_concrete_build_state.json and
 * in the portal form's data-state attributes, so config written by one boot must
 * parse identically on the next. Keeping one implementation is what makes that
 * true; it previously existed in four files with two subtly different fallbacks.
 */

/**
 * @brief Converts a build state to the name used in persisted config.
 *
 * @param state State to name.
 * @return Static string, or nullptr when the value is not a known enumerator.
 *         Callers that persist the result must treat nullptr as "do not write".
 */
const char* buildStateToString(BuildState state);

/**
 * @brief Parses a persisted build state name.
 *
 * @param stateText Name to parse. May be nullptr, which fails.
 * @param outState Parsed state on success; untouched on failure.
 * @return true when stateText named a known state.
 */
bool parseBuildState(const char* stateText, BuildState& outState);

#endif // BUILD_STATE_NAMES_H
