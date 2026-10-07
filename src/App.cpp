#include "App.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <cstring>

// USE_SERIAL_RENDERER swaps the LED panel for a renderer that writes the same
// rows to the serial log. Set it in platformio.ini. The two headers are mutually
// exclusive on purpose: HUB75Display.h pulls in the DMA library, which a
// simulator cannot run and which a log-only build has no use for.
#if USE_SERIAL_RENDERER
#include "Display/SerialRenderer.h"
#else
#include "Display/HUB75Display.h"
#endif

#include "NetworkManager.h"
#include "RuntimeBuild/BuildStateNames.h"
#include "RuntimeBuild/Builder.h"
#include "RuntimeBuild/RuntimeBuildPortal.h"
#include "TimeManager.h"

namespace {

constexpr unsigned long kWifiConnectTimeoutMs = 20000UL;
constexpr unsigned long kWifiRetryMessageMs = 3000UL;
constexpr unsigned long kWifiReconnectTimeoutMs = 10000UL;
constexpr unsigned long kHeapReportIntervalMs = 60000UL;
constexpr unsigned long kHaltPollMs = 1000UL;

// Log-safe wrapper: buildStateToString returns nullptr for an unrecognised
// value, which is the right answer when persisting but not when printing.
const char* buildStateForLog(BuildState state) {
    const char* name = buildStateToString(state);
    return name != nullptr ? name : "UNKNOWN";
}

// Returns what the panel should say about a finished portal session, or nullptr
// when the outcome needs no explanation. Either way the caller must repaint: the
// panel is still showing the AP passphrase, which is stale the moment the portal
// exits. The two timeouts are normal -- nobody provisioned, so the saved config
// still stands and there is nothing to report.
const char* portalResultToDisplayMessage(RuntimeBuildPortalResult result) {
    switch (result) {
        case RuntimeBuildPortalResult::SaveAndConnectOk:
            // Confirms the submission landed. Nothing else repaints until the
            // first fetchAndRender(), several boot stages later.
            return "Changes Saved";
        case RuntimeBuildPortalResult::TimeoutNoClient:
        case RuntimeBuildPortalResult::TimeoutNoSubmit:
            return nullptr;
        case RuntimeBuildPortalResult::SaveOkConnectFailed:
            // Saved is the part the user needs to know; the Wi-Fi half explains
            // the silence while connectWifi() works through its retries.
            return "Saved - No WiFi";
        case RuntimeBuildPortalResult::InternalError:
            return "Setup Portal Failed";
    }

    return "Setup Portal Error";
}

const char* wifiFailureToDisplayMessage(wl_status_t status) {
    switch (status) {
        case WL_NO_SSID_AVAIL:
            return "WiFi SSID Not Found";
        case WL_CONNECT_FAILED:
            return "WiFi Auth Failed";
        case WL_CONNECTION_LOST:
            return "WiFi Connection Lost";
        case WL_DISCONNECTED:
            return "WiFi Disconnected";
        case WL_IDLE_STATUS:
            return "WiFi Connect Timeout";
        default:
            return "WiFi Connect Failed";
    }
}

}  // namespace

App::App()
    : _renderer(),
      _busFetcher(),
      _runtimeConfig(),
      _runtimeConfigLoaded(false),
      _lastFetchTime(0),
      _lastHeapLogTime(0),
      _wifiDisconnected(false) {
}

// =========================================
// Lifecycle
// =========================================

void App::setup() {
    Serial.begin(115200);
    Serial.printf("Computer IP Address: %s\n", COMPUTER_IP);

    initDisplay();

    // SKIP_PORTAL exists for simulation and for log-only debugging of the fetch
    // path. The portal blocks boot for up to kNoClientTimeoutMs waiting for a
    // client that, in a simulator, cannot arrive: Wokwi's gateway documents no
    // soft-AP support, so every run would pay the full timeout before reaching
    // the code under test. The saved config on the filesystem is used as-is.
#if SKIP_PORTAL
    Serial.println("[Main] SKIP_PORTAL set - using the saved config, portal not started");
#else
    // [this] carries the object showMessage() needs, and re-reads _renderer per
    // call, so this survives the renderer swap in buildPipeline().
    const PortalNotifyFn showOnPanel =
        [this](const char* msg) { showMessage(msg); };

    const RuntimeBuildPortalResult portalResult =
        runRuntimeBuildPortal(showOnPanel);
    Serial.printf("[Main] Runtime build portal result: %d\n", static_cast<int>(portalResult));

    // A failed portal does not stop boot: the previously saved config may still
    // be good. Surface the reason so a stalled-looking device is explainable,
    // and let loadConfig() halt below if the config really is unusable.
    //
    // Repaint unconditionally. The panel is still showing the AP passphrase, and
    // the next draw is the first fetchAndRender() -- four boot stages away, none
    // of which draw anything on the success path. Leaving it up would strand a
    // stale, and now useless, Wi-Fi passphrase on the display.
    const char* portalMessage = portalResultToDisplayMessage(portalResult);
    showMessage(portalMessage != nullptr ? portalMessage : "Loading...");
#endif  // SKIP_PORTAL

    // If SCREEN_DEBUG is enabled, run display tests and never return.
    // Excluded from serial-renderer builds: screen_tests() is a HUB75Display
    // method exercising the panel itself, so there is no renderer to cast to and
    // nothing it could test.
    #if SCREEN_DEBUG && !USE_SERIAL_RENDERER
        Serial.println("[Main] SCREEN_DEBUG enabled - running screen tests");
        static_cast<HUB75Display*>(_renderer.get())->screen_tests();
        // screen_tests() never returns
    #endif

    loadConfig();
    connectWifi();
    syncClock();
    buildPipeline();

    #ifdef MEMORY_DEBUG
    Serial.printf("[Main] Heap reports will be sent via UDP to %s:%d\n", COMPUTER_IP, HEAP_UDP_PORT);
    #endif
}

void App::loop() {
    unsigned long now = millis();

    if (_lastFetchTime == 0 || (now - _lastFetchTime >= FETCH_INTERVAL_MS)) {
        _lastFetchTime = now ? now : 1;  // Avoid 0 to prevent re-trigger
        fetchAndRender();
    }

    #ifdef MEMORY_DEBUG
    if (millis() - _lastHeapLogTime >= kHeapReportIntervalMs) {
        _lastHeapLogTime = millis();
        sendHeapReport();
    }
    #endif
}

// =========================================
// Startup steps
// =========================================

void App::initDisplay() {
#if USE_SERIAL_RENDERER
    _renderer = std::unique_ptr<IRenderer>(new SerialRenderer());
#else
    _renderer = std::unique_ptr<IRenderer>(new HUB75Display());
#endif
    if (!_renderer->init()) {
        Serial.println("[Main] FATAL: Display init failed");
        halt("Display Init Failed");
    }

    Serial.println("[Main] Display initialized successfully");
    showMessage("Loading...");
}

void App::loadConfig() {
    const ConfigResult result = loadRuntimeConfig(_runtimeConfig);
    if (!result.ok) {
        Serial.printf("[Config] ERROR: %s\n", result.error);
        halt(configErrorToDisplayMessage(result.error));
    }
    _runtimeConfigLoaded = true;

#if SIM_WIFI
    // Overridden here rather than at each call site: connectWifi() and the
    // ensureWiFiConnected() call in fetchAndRender() both read these fields, so
    // this is the one place that covers every consumer. The saved credentials
    // stay on flash untouched -- only this boot's copy is redirected.
    strncpy(_runtimeConfig.wifi.ssid, SIM_WIFI_SSID, sizeof(_runtimeConfig.wifi.ssid) - 1);
    _runtimeConfig.wifi.ssid[sizeof(_runtimeConfig.wifi.ssid) - 1] = '\0';
    strncpy(_runtimeConfig.wifi.password, SIM_WIFI_PASSWORD,
            sizeof(_runtimeConfig.wifi.password) - 1);
    _runtimeConfig.wifi.password[sizeof(_runtimeConfig.wifi.password) - 1] = '\0';
    Serial.printf("[Config] SIM_WIFI set - using '%s' instead of the saved network\n",
                  _runtimeConfig.wifi.ssid);
#endif

    Serial.printf("[Config] Requested state: %s, concrete state: %s\n",
                  buildStateForLog(_runtimeConfig.buildState),
                  buildStateForLog(_runtimeConfig.concreteBuildState));
    Serial.printf("[Config] Loaded %d targets\n", _runtimeConfig.bus.targetCount);
}

void App::connectWifi() {
    WiFi.mode(WIFI_STA);

    int wifiAttempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
        wifiAttempt++;
        Serial.printf("[WiFi] Attempt #%d with SSID='%s'\n", wifiAttempt, _runtimeConfig.wifi.ssid);

        WiFi.disconnect();
#if SIM_WIFI
        // See ensureWiFiConnected(): the pinned channel skips the scan phase.
        WiFi.begin(_runtimeConfig.wifi.ssid, _runtimeConfig.wifi.password, SIM_WIFI_CHANNEL);
#else
        WiFi.begin(_runtimeConfig.wifi.ssid, _runtimeConfig.wifi.password);
#endif

        Serial.printf("Connecting to %s", _runtimeConfig.wifi.ssid);
        unsigned long wifiStart = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < kWifiConnectTimeoutMs) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            break;
        }

        wl_status_t status = WiFi.status();
        const char* failMessage = wifiFailureToDisplayMessage(status);
        Serial.printf("[WiFi] Attempt #%d failed, status=%d (%s)\n", wifiAttempt,
                      static_cast<int>(status), failMessage);
        showMessage(failMessage);
        delay(kWifiRetryMessageMs);
    }

    Serial.printf("Connected! IP: %s, RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

void App::syncClock() {
    time_init_and_sync(TIME_ZONE);

    char timeBuf[6];
    time_get_formatted(timeBuf, sizeof(timeBuf));
    Serial.print("The time now is: ");
    Serial.println(timeBuf);
}

void App::buildPipeline() {
    std::unique_ptr<IBusFetcher> builtFetcher;
    std::unique_ptr<IRenderer> builtRenderer;
    if (!build(_runtimeConfig.concreteBuildState, builtFetcher, builtRenderer)) {
        Serial.println("[Build] Runtime build selection failed");
        halt("Build Failed");
    }

    if (builtRenderer) {
        // Assignment releases the previously owned renderer.
        _renderer = std::move(builtRenderer);
        if (!_renderer->init()) {
            // Nothing to report on: the panel is dead once init() fails.
            Serial.println("[Build] FATAL: Replacement renderer init failed");
            halt("Renderer Init Failed");
        }
        showMessage("Build Ready");
    }

    _busFetcher = std::move(builtFetcher);

    if (!_busFetcher) {
        Serial.println("[Build] No fetcher created for selected build state");
        showMessage("No Fetcher");
    }
}

// =========================================
// Periodic work
// =========================================

void App::fetchAndRender() {
    if (!_runtimeConfigLoaded) {
        Serial.println("[Config] Runtime config not loaded, skipping WiFi reconnect");
        showMessage("Config Not Loaded");
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        _wifiDisconnected = true;
    }

    if (!ensureWiFiConnected(_runtimeConfig.wifi.ssid, _runtimeConfig.wifi.password,
                             kWifiReconnectTimeoutMs)) {
        Serial.println("[Main] No WiFi - skipping fetch");
        showMessage(wifiFailureToDisplayMessage(WiFi.status()));
        return;
    }

    if (!_busFetcher) {
        Serial.println("[Main] No fetcher configured for this build state");
        showMessage("No Fetcher");
        return;
    }

    Serial.println("\n--- Fetching bus arrivals ---");
    char fetchTimeBuf[6];
    time_get_formatted(fetchTimeBuf, sizeof(fetchTimeBuf));
    Serial.println(fetchTimeBuf);

    BusTarget* targets = _runtimeConfig.bus.targets;
    const int targetCount = _runtimeConfig.bus.targetCount;

    for (int i = 0; i < targetCount; i++) {
        bool success = _busFetcher->update(targets[i]);
        targets[i].no_data = !success;

        if (success) {
            Serial.printf("Line %s: %s (%d min) %s | next: %s (%d min) %s\n",
                          targets[i].line,
                          targets[i].last_known_ETA,
                          targets[i].minutes_remaining,
                          targets[i].is_realtime ? "[LIVE]" : "[SCHED]",
                          targets[i].has_next_arrival ? targets[i].next_ETA : "--",
                          targets[i].has_next_arrival ? targets[i].next_minutes_remaining : -1,
                          !targets[i].has_next_arrival ? "" :
                              (targets[i].next_is_realtime ? "[LIVE]" : "[SCHED]"));
        } else {
            Serial.printf("Line %s: No data\n", targets[i].line);
        }
    }
    Serial.println("-----------------------------");

    if (_renderer) {
        _renderer->render(targets, targetCount);
    }
}

void App::sendHeapReport() {
    WiFiUDP udp;
    char buf[192];
    uint32_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    uint32_t minFreeHeap = ESP.getMinFreeHeap();
    int rssi = WiFi.RSSI();
    const char* warnMsg = _wifiDisconnected ? ", WARN: internet disconnected" : "";
    bool connected = (WiFi.status() == WL_CONNECTED);
    snprintf(buf, sizeof(buf), "Time: %lu, Free: %u, MinFree: %u, WiFi: %s, RSSI: %d%s",
             millis() / 1000, freeHeap, minFreeHeap,
             connected ? "OK" : "DOWN", rssi, warnMsg);
    if (connected) _wifiDisconnected = false;  // Only clear if packet will actually be sent,

    udp.beginPacket(COMPUTER_IP, HEAP_UDP_PORT);
    udp.print(buf);
    udp.endPacket();

    Serial.printf("[HeapUDP] %s\n", buf);
}

// =========================================
// Helpers
// =========================================

void App::showMessage(const char* msg) {
    if (_renderer) {
        _renderer->showMessage(msg);
    }
}

void App::halt(const char* displayMessage) {
    showMessage(displayMessage);
    while (true) {
        delay(kHaltPollMs);
    }
}
