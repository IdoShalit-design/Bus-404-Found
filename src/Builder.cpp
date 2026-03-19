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
        case BuildState::Production:
        case BuildState::Development:
        case BuildState::Unknown:
        default:
            fetcher = new CurlbusFetcher();
            renderer = new HUB75Display();
            break;
    }
}
