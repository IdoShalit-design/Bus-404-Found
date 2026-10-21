#include "RuntimeBuild/BuildStateNames.h"

#include <string.h>

namespace {

struct BuildStateName {
    BuildState state;
    const char* name;
};

// The one place a build state and its persisted name are tied together.
constexpr BuildStateName kBuildStateNames[] = {
    {BuildState::BUS_BY_STATION, "BUS_BY_STATION"},
    {BuildState::BUS_BY_LINES, "BUS_BY_LINES"},
    {BuildState::NY_METRO_BY_STATION, "NY_METRO_BY_STATION"},
    {BuildState::USE_CURRENT_BUILD, "USE_CURRENT_BUILD"},
};

}  // namespace

const char* buildStateToString(BuildState state) {
    for (const BuildStateName& entry : kBuildStateNames) {
        if (entry.state == state) {
            return entry.name;
        }
    }

    return nullptr;
}

bool parseBuildState(const char* stateText, BuildState& outState) {
    if (stateText == nullptr) {
        return false;
    }

    for (const BuildStateName& entry : kBuildStateNames) {
        if (strcmp(stateText, entry.name) == 0) {
            outState = entry.state;
            return true;
        }
    }

    return false;
}
