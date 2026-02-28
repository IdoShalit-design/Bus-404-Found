#include <Arduino.h>
#include "Network/NetworkManager.h"
#include "Network/IBusFetcher.h"
#include "Network/CurlBusFetcher.h"
#include "Display/IRenderer.h"
#include "Display/HUB75Display.h"
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

  #if !DUMMY_BUSES_DEBUG
  // =========================================
  // 1. Initialize WiFi
  // =========================================
  WifiCredentials credentials(WIFI_CREDENTIALS.ssid, WIFI_CREDENTIALS.password);
  NetworkManager network_manager(credentials);

  network_manager.print_networks();

  bool connected = network_manager.connect_to_wifi();
  if (connected) {
    Serial.printf("Successfully connected to %s\n", WIFI_CREDENTIALS.ssid);
  } else {
    Serial.printf("Failed to connect to %s\n", WIFI_CREDENTIALS.ssid);
  }
  network_manager.print_wifi_status();

  // =========================================
  // 2. Synchronize clock
  // =========================================
  time_manager.init_and_sync();
  Serial.println("The time now is:");
  Serial.println(time_manager.get_formatted_time());

  // =========================================
  // 3. Initialize bus fetcher
  // =========================================
  bus_fetcher = createFetcher();
  #else
  Serial.println("[Main] DUMMY_BUSES_DEBUG enabled - skipping WiFi, NTP and fetcher");
  #endif

  // =========================================
  // 4. Initialize display
  // =========================================
  renderer = new HUB75Display();
  if (!renderer->init()) {
    Serial.println("[Main] Display initialization failed!");
  } else {
    Serial.println("[Main] Display initialized successfully");
  }

  // If SCREEN_DEBUG is enabled, run display tests and never return
  #if SCREEN_DEBUG
    Serial.println("[Main] SCREEN_DEBUG enabled - running screen tests");
    ((HUB75Display*)renderer)->screen_tests();
    // screen_tests() never returns
  #endif

  #if DUMMY_BUSES_DEBUG
  // Load dummy targets and render immediately, no fetch needed
  Serial.println("[Main] Rendering dummy bus data...");
  for (int i = 0; i < DUMMY_TARGETS_COUNT; i++) {
    bus_targets[i] = DUMMY_TARGETS[i];
  }
  if (renderer) {
    renderer->render(bus_targets, DUMMY_TARGETS_COUNT);
  }
  #else
  // Copy const targets to mutable array
  for (int i = 0; i < TARGETS_COUNT; i++) {
    bus_targets[i] = MY_TARGETS[i];
  }
  Serial.printf("[Main] Tracking %d bus targets\n", TARGETS_COUNT);
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
    
    Serial.println("\n--- Fetching bus arrivals ---");
    Serial.println(time_manager.get_formatted_time());
    
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

    // Update display with new data
    if (renderer) {
      renderer->render(bus_targets, TARGETS_COUNT);
    }
  }
}