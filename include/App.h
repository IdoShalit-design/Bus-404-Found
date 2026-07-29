#ifndef APP_H
#define APP_H

#include <memory>

#include "Config.h"
#include "Display/IRenderer.h"
#include "Fetchers/IBusFetcher.h"
#include "RuntimeBuild/ConfigLoader.h"
#include "Structs.h"

/**
 * @brief Owns the whole device lifecycle: display, provisioning portal,
 *        runtime config, Wi-Fi, the fetch/render pipeline and its scheduling.
 *
 * Replaces the file-scope globals that previously lived in main.cpp so that
 * ownership and initialization order are explicit rather than implied by
 * declaration order.
 *
 * @note Non-copyable: it exclusively owns the renderer and fetcher.
 */
class App {
public:
    App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    /**
     * @brief Runs one-time startup. Never returns on a fatal error.
     */
    void setup();

    /**
     * @brief Runs a single scheduling tick. Call from the Arduino loop().
     */
    void loop();

private:
    std::unique_ptr<IRenderer> _renderer;
    std::unique_ptr<IBusFetcher> _busFetcher;

    // Owns the bus targets directly; they are both the parsed config and the
    // live working set that fetchers update in place.
    RuntimeConfig _runtimeConfig;
    bool _runtimeConfigLoaded;

    unsigned long _lastFetchTime;
    unsigned long _lastHeapLogTime;
    bool _wifiDisconnected;

    // --- Startup steps, in the order setup() runs them ---
    void initDisplay();
    void loadConfig();
    void connectWifi();
    void syncClock();
    void buildPipeline();

    // --- Periodic work ---
    void fetchAndRender();
    void sendHeapReport();

    /** @brief Shows a message when a renderer exists; no-op otherwise. */
    void showMessage(const char* msg);

    /** @brief Shows a message and blocks forever. Never returns. */
    void halt(const char* displayMessage);
};

#endif // APP_H
