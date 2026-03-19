/**
 * @file Builder.cpp
 * @brief Implements construction logic for application components.
 */

#include "Builder.h"
#include "Network/CurlBusFetcher.h"
#include "Display/HUB75Display.h"

void Builder::build(const AppConfig& config, IBusFetcher*& fetcher, IRenderer*& renderer) {
    if (fetcher != nullptr) {
        delete fetcher;
        fetcher = nullptr;
    }

    if (renderer != nullptr) {
        delete renderer;
        renderer = nullptr;
    }

    switch (config.state) {
        case BuildState::Current:
            fetcher = new CurlbusFetcher();
            renderer = new HUB75Display();
            break;
        case BuildState::Station:
        case BuildState::NyMetro:
            // Stubs for future modes; use current defaults until specific fetchers/renderers exist.
            fetcher = new CurlbusFetcher();
            renderer = new HUB75Display();
            break;
        default:
            fetcher = new CurlbusFetcher();
            renderer = new HUB75Display();
            break;
    }
}
