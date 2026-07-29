#include "App.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <cstring>

#include "Display/HUB75Display.h"
#include "NetworkManager.h"
#include "RuntimeBuild/Builder.h"
#include "RuntimeBuild/RuntimeBuildPortal.h"
#include "TimeManager.h"

namespace {

constexpr unsigned long kWifiConnectTimeoutMs = 20000UL;
constexpr unsigned long kWifiRetryMessageMs = 3000UL;
constexpr unsigned long kWifiReconnectTimeoutMs = 10000UL;
constexpr unsigned long kHeapReportIntervalMs = 60000UL;
constexpr unsigned long kHaltPollMs = 1000UL;

const char* buildStateToString(BuildState state) {
    switch (state) {
        case BUS_BY_STATION:
            return "BUS_BY_STATION";
        case BUS_BY_LINES:
            return "BUS_BY_LINES";
        case NY_METRO_BY_STATION:
            return "NY_METRO_BY_STATION";
        case USE_CURRENT_BUILD:
            return "USE_CURRENT_BUILD";
        default:
            return "UNKNOWN";
    }
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

    RuntimeBuildPortalResult portalResult = runRuntimeBuildPortal();
    Serial.printf("[Main] Runtime build portal result: %d\n", static_cast<int>(portalResult));

    // If SCREEN_DEBUG is enabled, run display tests and never return
    #if SCREEN_DEBUG
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

    if (_lastFetchTime == 0 || (now - _lastFetchTime >= FETCH_INTERVAL)) {
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
    _renderer = std::unique_ptr<IRenderer>(new HUB75Display());
    if (!_renderer->init()) {
        Serial.println("[Main] FATAL: Display init failed");
        halt("Display Init Failed");
    }

    Serial.println("[Main] Display initialized successfully");
    showMessage("Loading...");
}

void App::loadConfig() {
    char configError[128] = {0};
    if (!loadRuntimeConfig(_runtimeConfig, configError, sizeof(configError))) {
        Serial.printf("[Config] ERROR: %s\n", configError);
        halt(configErrorToDisplayMessage(configError));
    }
    _runtimeConfigLoaded = true;

    Serial.printf("[Config] Requested state: %s, concrete state: %s\n",
                  buildStateToString(_runtimeConfig.buildState),
                  buildStateToString(_runtimeConfig.concreteBuildState));
    Serial.printf("[Config] Loaded %d targets\n", _runtimeConfig.bus.targetCount);
}

void App::connectWifi() {
    WiFi.mode(WIFI_STA);

    int wifiAttempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
        wifiAttempt++;
        Serial.printf("[WiFi] Attempt #%d with SSID='%s'\n", wifiAttempt, _runtimeConfig.wifi.ssid);

        WiFi.disconnect();
        WiFi.begin(_runtimeConfig.wifi.ssid, _runtimeConfig.wifi.password);

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
            Serial.printf("Line %s: %s (%d min) %s\n",
                          targets[i].line,
                          targets[i].last_known_ETA,
                          targets[i].minutes_remaining,
                          targets[i].is_realtime ? "[LIVE]" : "[SCHED]");
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
