/**
 * @file Builder.h
 * @brief Declares the Builder responsible for wiring application components.
 */

#ifndef BUILDER_H
#define BUILDER_H

#include "ConfigManager.h"
#include "Network/IBusFetcher.h"
#include "Display/IRenderer.h"

/**
 * @brief Responsible for constructing core application objects from configuration.
 */
class Builder {
public:
    /**
     * @brief Builds renderer and fetcher instances according to configuration.
     * @param config Application configuration containing build state and options.
     * @param fetcher Reference to a fetcher pointer that will be replaced. Any existing instance will be deleted.
     * @param renderer Reference to a renderer pointer that will be replaced. Any existing instance will be deleted.
     */
    static void build(const AppConfig& config, IBusFetcher*& fetcher, IRenderer*& renderer);
};

#endif // BUILDER_H
