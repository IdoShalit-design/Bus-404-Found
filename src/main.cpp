#include <Arduino.h>
#include <cstring>
#include <memory>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "Config.h"
#include "RuntimeBuild/Builder.h"
#include "RuntimeBuild/ConfigLoader.h"
#include "Display/HUB75Display.h"
#include "Display/IRenderer.h"
#include "Fetchers/IBusFetcher.h"
#include "NetworkManager.h"
#include "RuntimeBuild/RuntimeBuildPortal.h"
#include "Structs.h"
#include "TimeManager.h"

// =========================================
// Global instances
// =========================================

// Display renderer
IRenderer* renderer = nullptr;

// Generic fetcher pointer - concrete type determined by FETCHER_TYPE in Config.h
IBusFetcher* bus_fetcher = nullptr;

// Runtime config loaded from LittleFS JSON files
RuntimeConfig runtime_config;
bool runtime_config_loaded = false;

// Mutable bus targets used by fetcher and renderer
BusTarget bus_targets[MAX_RUNTIME_TARGETS];
int bus_targets_count = 0;


// Update interval (milliseconds)
unsigned long last_fetch_time = 0;
unsigned long last_heap_log_time = 0;  // Timestamp of last heap log write
bool wifi_disconnected_msg_flag = false; // Set when WiFi loss is detected, cleared after UDP report

/**
 * @brief Sends current heap memory status via UDP to COMPUTER_IP.
 */
void sendHeapUDP() {
    WiFiUDP udp;
    char buf[192];
    uint32_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    uint32_t minFreeHeap = ESP.getMinFreeHeap();
    int rssi = WiFi.RSSI();
    const char* warnMsg = wifi_disconnected_msg_flag ? ", WARN: internet disconnected" : "";
    bool connected = (WiFi.status() == WL_CONNECTED);
    snprintf(buf, sizeof(buf), "Time: %lu, Free: %u, MinFree: %u, WiFi: %s, RSSI: %d%s",
             millis() / 1000, freeHeap, minFreeHeap,
             connected ? "OK" : "DOWN", rssi, warnMsg);
    if (connected) wifi_disconnected_msg_flag = false;  // Only clear if packet will actually be sent

    udp.beginPacket(COMPUTER_IP, HEAP_UDP_PORT);
    udp.print(buf);
    udp.endPacket();

    Serial.printf("[HeapUDP] %s\n", buf);
}

void setup() {
  // Initialize serial for output
  Serial.begin(115200);
  Serial.printf("Computer IP Address: %s\n", COMPUTER_IP);

  // =========================================
  // 1. Initialize display (first — show Loading... ASAP)
  // =========================================
  renderer = new HUB75Display();
  renderer->init();
  Serial.println("[Main] Display initialized successfully");
  renderer->showMessage("Loading...");

  RuntimeBuildPortalResult portalResult = runRuntimeBuildPortal();
  Serial.printf("[Main] Runtime build portal result: %d\n", static_cast<int>(portalResult));
  

  // If SCREEN_DEBUG is enabled, run display tests and never return
  #if SCREEN_DEBUG
    Serial.println("[Main] SCREEN_DEBUG enabled - running screen tests");
    ((HUB75Display*)renderer)->screen_tests();
    // screen_tests() never returns
  #endif

  #if !DUMMY_BUSES_DEBUG
  // =========================================
  // 2. Load runtime config from LittleFS (fail-fast)
  // =========================================
  char config_error[128] = {0};
  if (!loadRuntimeConfig(runtime_config, config_error, sizeof(config_error))) {
    Serial.printf("[Config] ERROR: %s\n", config_error);
    if (renderer) renderer->showMessage(configErrorToDisplayMessage(config_error));
    while (true) {
      delay(1000);
    }
  }
  runtime_config_loaded = true;

  bus_targets_count = runtime_config.bus.targetCount;
  for (int i = 0; i < bus_targets_count; i++) {
    bus_targets[i].stationId = runtime_config.bus.targets[i].stationId;
    bus_targets[i].line = runtime_config.bus.targets[i].line;
    strncpy(bus_targets[i].destination, runtime_config.bus.targets[i].destination, sizeof(bus_targets[i].destination) - 1);
    bus_targets[i].destination[sizeof(bus_targets[i].destination) - 1] = '\0';
    bus_targets[i].is_realtime = false;
    bus_targets[i].last_known_ETA[0] = '\0';
    bus_targets[i].minutes_remaining = 0;
    bus_targets[i].no_data = false;
  }

  Serial.printf("[Config] Loaded %d targets\n", bus_targets_count);

  // =========================================
  // 3. Initialize WiFi
  // =========================================
  if (!runtime_config_loaded) {
    Serial.println("[Config] Runtime config not loaded, aborting WiFi init");
    if (renderer) renderer->showMessage("Config Not Loaded");
    while (true) {
      delay(1000);
    }
  }

  WiFi.mode(WIFI_STA);
  Serial.printf("[WiFi] Attempting connect with SSID='%s', PASSWORD='%s'\n",
                runtime_config.wifi.ssid,
                runtime_config.wifi.password);
  WiFi.begin(runtime_config.wifi.ssid, runtime_config.wifi.password);

  Serial.printf("Connecting to %s", runtime_config.wifi.ssid);
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Connected! IP: %s, RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("WiFi connection failed!");
  }

  // =========================================
  // 4. Synchronize clock
  // =========================================
  time_init_and_sync(TIME_ZONE);
  char time_buf[6];
  time_get_formatted(time_buf, sizeof(time_buf));
  Serial.print("The time now is: ");
  Serial.println(time_buf);

  // =========================================
  // 5. Build runtime pipeline (fetcher/renderer) from selected BuildState
  // =========================================
  std::unique_ptr<IBusFetcher> builtFetcher;
  std::unique_ptr<IRenderer> builtRenderer;
  if (!build(runtime_config.buildState, builtFetcher, builtRenderer)) {
    Serial.println("[Build] Runtime build selection failed");
    if (renderer) renderer->showMessage("Build Failed");
    while (true) {
      delay(1000);
    }
  }

  if (builtRenderer) {
    if (renderer != nullptr) {
      delete renderer;
    }
    renderer = builtRenderer.release();
    renderer->init();
    renderer->showMessage("Build Ready");
  }

  bus_fetcher = builtFetcher.release();

  if (bus_fetcher == nullptr) {
    Serial.println("[Build] No fetcher created for selected build state");
    if (renderer) renderer->showMessage("No Fetcher");
  }
  #else
  Serial.println("[Main] DUMMY_BUSES_DEBUG enabled - skipping WiFi, NTP and fetcher");
  #endif

  #if DUMMY_BUSES_DEBUG
  // Load dummy targets and render immediately, no fetch needed
  Serial.println("[Main] Rendering dummy bus data...");
  for (int i = 0; i < DUMMY_TARGETS_COUNT; i++) {
    bus_targets[i] = DUMMY_TARGETS[i];
  }
  bus_targets_count = DUMMY_TARGETS_COUNT;
  if (renderer) {
    renderer->render(bus_targets, bus_targets_count);
  }
  #endif

  #ifdef MEMORY_DEBUG
  Serial.printf("[Main] Heap reports will be sent via UDP to %s:%d\n", COMPUTER_IP, HEAP_UDP_PORT);
  #endif

}

void loop() {
  #if DUMMY_BUSES_DEBUG
  // Nothing to do - dummy data already rendered in setup()
  delay(1000);
  return;
  #endif

  // =========================================
  // Periodic bus data fetch
  // =========================================
  unsigned long now = millis();
  
  if (last_fetch_time == 0 || (now - last_fetch_time >= FETCH_INTERVAL)) {
    last_fetch_time = now ? now : 1;  // Avoid 0 to prevent re-trigger

    if (!runtime_config_loaded) {
      Serial.println("[Config] Runtime config not loaded, skipping WiFi reconnect");
      if (renderer) renderer->showMessage("Config Not Loaded");
      return;
    }

    if (WiFi.status() != WL_CONNECTED) {
      wifi_disconnected_msg_flag = true;
    }

    if (!ensureWiFiConnected(runtime_config.wifi.ssid, runtime_config.wifi.password, 10000)) {
      Serial.println("[Main] No WiFi - skipping fetch");
      if (renderer) renderer->showMessage("No WiFi");
    } else if (bus_fetcher == nullptr) {
      Serial.println("[Main] No fetcher configured for this build state");
      if (renderer) renderer->showMessage("No Fetcher");
    } else {
      Serial.println("\n--- Fetching bus arrivals ---");
      char fetch_time_buf[6];
      time_get_formatted(fetch_time_buf, sizeof(fetch_time_buf));
      Serial.println(fetch_time_buf);
      
      for (int i = 0; i < bus_targets_count; i++) {
        bool success = bus_fetcher->update(bus_targets[i]);
        bus_targets[i].no_data = !success;
        
        if (success) {
          Serial.printf("Line %s: %s (%d min) %s\n", 
                        bus_targets[i].line,
                        bus_targets[i].last_known_ETA,
                        bus_targets[i].minutes_remaining,
                        bus_targets[i].is_realtime ? "[LIVE]" : "[SCHED]");
        } else {
          Serial.printf("Line %s: No data\n", bus_targets[i].line);
        }
      }
      Serial.println("-----------------------------");

      // Update display with new data
      if (renderer) {
        renderer->render(bus_targets, bus_targets_count);
      }
    }
  }

  #ifdef MEMORY_DEBUG
  if (millis() - last_heap_log_time >= 60000UL) {
    last_heap_log_time = millis();
    sendHeapUDP();
  }
  #endif
}

