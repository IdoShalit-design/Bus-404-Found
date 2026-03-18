#ifndef BUILDER_H
#define BUILDER_H

#include <memory>

#include "Network/IBusFetcher.h"
#include "Display/IRenderer.h"

/**
 * @brief Build modes that control which fetch/render pipeline is selected.
 */
typedef enum BuildState {
    BUS_BY_STATION,
    BUS_BY_LINES,
    NY_METRO_BY_STATION,
    USE_CURRENT_BUILD,
} BuildState;

/**
 * @brief Applies the requested build mode and configures outputs as needed.
 *
 * @param state Selected build mode.
 * @param fetcher Output fetcher instance used by the chosen mode.
 * @param renderer Output renderer instance used by the chosen mode.
 * @return true on a recognized and successfully applied mode, false otherwise.
 */
bool build(BuildState state, std::unique_ptr<IBusFetcher>& fetcher, std::unique_ptr<IRenderer>& renderer);

#endif // BUILDER_H
