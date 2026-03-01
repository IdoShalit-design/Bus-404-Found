#include <Arduino.h>
#include "Network/NetworkManager.h"
#include "Network/IBusFetcher.h"
#include "Network/CurlBusFetcher.h"
#include "Network/ConfigPortal.h"
#include "Display/IRenderer.h"
#include "Display/HUB75Display.h"
#include "NVSManager.h"
#include "Config.h"
#include "Structs.h"
#include "TimeManager.h"

#define FETCH_INTERVAL 30000  // 30 seconds

// =========================================
// Global instances
// =========================================
TimeManager time_manager(TIME_ZONE);

// Display renderer
IRenderer* renderer = nullptr;

// Generic fetcher pointer - concrete type determined by FETCHER_TYPE in Config.h
IBusFetcher* bus_fetcher = nullptr;

// NVS and configuration portal
NVSManager nvs_manager;
ConfigPortal* config_portal = nullptr;

// Mutable copy of bus targets (original is const)
BusTarget bus_targets[TARGETS_COUNT];

// Update interval (milliseconds)
unsigned long last_fetch_time = 0;

/**
 * @brief Creates the appropriate IBusFetcher based on FETCHER_TYPE config.
 * @return Pointer to concrete IBusFetcher implementation.
 */
IBusFetcher* createFetcher() {
    #if FETCHER_TYPE == FETCHER_CURLBUS
        Serial.println("[Main] Using CurlbusFetcher");
        return new CurlbusFetcher();
    // Future fetcher types:
    // #elif FETCHER_TYPE == FETCHER_GOVIL
    //     return new GovIlFetcher();
    // #elif FETCHER_TYPE == FETCHER_MOCK
    //     return new MockFetcher();
    #else
        Serial.println("[Main] Unknown FETCHER_TYPE, defaulting to CurlbusFetcher");
        return new CurlbusFetcher();
    #endif
}

void setup() {
  // Initialize serial for output
  Serial.begin(115200);

  // =========================================
  // 1. Initialize display and show boot message
  // =========================================
  renderer = new HUB75Display();
  if (!renderer->init()) {
    Serial.println("[Main] Display initialization failed!");
  } else {
    Serial.println("[Main] Display initialized successfully");
    renderer->showStatus("BOOTING...");
  }

  // If SCREEN_DEBUG is enabled, run display tests and never return
  #if SCREEN_DEBUG
    Serial.println("[Main] SCREEN_DEBUG enabled - running screen tests");
    ((HUB75Display*)renderer)->screen_tests();
    // screen_tests() never returns
  #endif

  // =========================================
  // 2. Initialize NVS
  // =========================================
  nvs_manager.begin();

  #if DUMMY_BUSES_DEBUG
  Serial.println("[Main] DUMMY_BUSES_DEBUG enabled - skipping WiFi, NTP and fetcher");

  // Load dummy targets and render immediately, no fetch needed
  Serial.println("[Main] Rendering dummy bus data...");
  for (int i = 0; i < DUMMY_TARGETS_COUNT; i++) {
    bus_targets[i] = DUMMY_TARGETS[i];
  }
  if (renderer) {
    renderer->render(bus_targets, DUMMY_TARGETS_COUNT);
  }
  #else
  // =========================================
  // 3. Start Access Point for setup window
  // =========================================
  config_portal = new ConfigPortal(nvs_manager);
  config_portal->startAP();

  bool clientConnected = false;
  for (int i = SETUP_TIMEOUT_SEC; i > 0; i--) {
    char msg[20];
    snprintf(msg, sizeof(msg), "SETUP? %ds...", i);
    if (renderer) renderer->showStatus(msg);

    // Check for AP client connections 10 times per second
    for (int j = 0; j < 10; j++) {
      if (config_portal->hasClientConnected()) {
        clientConnected = true;
        break;
      }
      delay(100);
    }
    if (clientConnected) break;
  }

  if (clientConnected) {
    // =========================================
    // 4a. Client connected — stay in AP mode
    // =========================================
    Serial.println("[Main] Client connected to AP — entering config mode");
    if (renderer) renderer->showStatus("AP CONFIG");
    config_portal->startWebServer();
    // loop() will handle web server requests; bus fetching is skipped.
    return;
  }

  // =========================================
  // 4b. No client — close AP, connect to WiFi
  // =========================================
  config_portal->stopAP();

  // Get credentials: NVS first, then compile-time fallback
  String ssid = nvs_manager.getSSID();
  String password = nvs_manager.getPassword();

  if (ssid.length() == 0) {
    ssid = WIFI_CREDENTIALS.ssid;
    password = WIFI_CREDENTIALS.password;
    Serial.println("[Main] Using fallback WiFi credentials from config");
  } else {
    Serial.println("[Main] Using WiFi credentials from NVS");
  }

  if (renderer) renderer->showStatus("Connecting...");

  WifiCredentials credentials(ssid.c_str(), password.c_str());
  NetworkManager network_manager(credentials);

  network_manager.print_networks();
  bool connected = network_manager.connect_to_wifi();

  if (connected) {
    Serial.printf("Successfully connected to %s\n", ssid.c_str());
    network_manager.print_wifi_status();

    if (renderer) renderer->showStatus("WiFi OK");
    delay(1000);

    // Show IP address
    String ipStr = WiFi.localIP().toString();
    if (renderer) renderer->showStatus(ipStr.c_str());
    delay(2000);

    // Start mDNS and web server (remain active for settings changes)
    config_portal->startMDNS();
    config_portal->startWebServer();

    if (renderer) renderer->showStatus("bus.local");
    delay(1000);
  } else {
    Serial.printf("Failed to connect to %s\n", ssid.c_str());
    network_manager.print_wifi_status();
    if (renderer) renderer->showStatus("WiFi FAIL");
    delay(3000);
  }

  // =========================================
  // 5. Synchronize clock
  // =========================================
  time_manager.init_and_sync();
  Serial.println("The time now is:");
  char timeBuf[6];
  time_manager.get_formatted_time(timeBuf, sizeof(timeBuf));
  Serial.println(timeBuf);

  // =========================================
  // 6. Initialize bus fetcher
  // =========================================
  bus_fetcher = createFetcher();

  // Copy const targets to mutable array
  for (int i = 0; i < TARGETS_COUNT; i++) {
    bus_targets[i] = MY_TARGETS[i];
  }
  Serial.printf("[Main] Tracking %d bus targets\n", TARGETS_COUNT);
  #endif
}

void loop() {
  // =========================================
  // Handle web server requests (AP or STA mode)
  // =========================================
  if (config_portal) {
    config_portal->handleClient();
  }

  // In AP config mode, only serve the web interface
  if (config_portal && config_portal->isAPMode()) {
    delay(10);
    return;
  }

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
    
    Serial.println("\n--- Fetching bus arrivals ---");
    char timeBuf[6];
    time_manager.get_formatted_time(timeBuf, sizeof(timeBuf));
    Serial.println(timeBuf);
    
    for (int i = 0; i < TARGETS_COUNT; i++) {
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
    Serial.printf("[Main] Free heap: %u bytes\n", ESP.getFreeHeap());

    // Update display with new data
    if (renderer) {
      renderer->render(bus_targets, TARGETS_COUNT);
    }
  }
}