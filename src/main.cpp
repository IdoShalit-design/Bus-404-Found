#include <Arduino.h>
#include <WiFi.h>
#include "Network/IBusFetcher.h"
#include "Network/CurlBusFetcher.h"
#include "Display/IRenderer.h"
#include "Display/HUB75Display.h"
#include "Config.h"
#include "Structs.h"
#include "TimeManager.h"
#include <WiFiUdp.h>

// =========================================
// Global instances
// =========================================

// Display renderer
IRenderer* renderer = nullptr;

// Generic fetcher pointer - concrete type determined by FETCHER_TYPE in Config.h
IBusFetcher* bus_fetcher = nullptr;

// Mutable copy of bus targets (original is const)
BusTarget bus_targets[TARGETS_COUNT];


// Update interval (milliseconds)
unsigned long last_fetch_time = 0;
unsigned long last_heap_log_time = 0;  // Timestamp of last heap log write
bool wifi_disconnected_msg_flag = false; // Set when WiFi loss is detected, cleared after UDP report

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

/**
 * @brief Ensures WiFi is connected. Attempts reconnection if disconnected.
 * @return true if connected, false if reconnection failed.
 */
bool ensureWiFi() {
    if (WiFi.status() == WL_CONNECTED) return true;

    wifi_disconnected_msg_flag = true;
    Serial.println("[WiFi] Disconnected, attempting reconnect...");
    WiFi.disconnect();
    WiFi.begin(WIFI_CREDENTIALS.ssid, WIFI_CREDENTIALS.password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.println("[WiFi] Reconnect failed.");
    return false;
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
  

  // If SCREEN_DEBUG is enabled, run display tests and never return
  #if SCREEN_DEBUG
    Serial.println("[Main] SCREEN_DEBUG enabled - running screen tests");
    ((HUB75Display*)renderer)->screen_tests();
    // screen_tests() never returns
  #endif

  #if !DUMMY_BUSES_DEBUG
  // =========================================
  // 2. Initialize WiFi
  // =========================================
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_CREDENTIALS.ssid, WIFI_CREDENTIALS.password);

  Serial.printf("Connecting to %s", WIFI_CREDENTIALS.ssid);
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
  // 3. Synchronize clock
  // =========================================
  time_init_and_sync(TIME_ZONE);
  char time_buf[6];
  time_get_formatted(time_buf, sizeof(time_buf));
  Serial.print("The time now is: ");
  Serial.println(time_buf);

  // =========================================
  // 4. Initialize bus fetcher
  // =========================================
  bus_fetcher = createFetcher();
  #else
  Serial.println("[Main] DUMMY_BUSES_DEBUG enabled - skipping WiFi, NTP and fetcher");
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

    if (!ensureWiFi()) {
      Serial.println("[Main] No WiFi - skipping fetch");
      if (renderer) renderer->showMessage("No WiFi");
    } else {
      Serial.println("\n--- Fetching bus arrivals ---");
      char fetch_time_buf[6];
      time_get_formatted(fetch_time_buf, sizeof(fetch_time_buf));
      Serial.println(fetch_time_buf);
      
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

  #ifdef MEMORY_DEBUG
  if (millis() - last_heap_log_time >= 60000UL) {
    last_heap_log_time = millis();
    sendHeapUDP();
  }
  #endif
}

