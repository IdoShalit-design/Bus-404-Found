/**
 * @file CurlBusFetcher.cpp
 * @brief Implementation of the CurlbusFetcher class for fetching real-time bus data.
 * 
 * This fetcher retrieves bus arrival times from the CurlBus API (curlbus.app),
 * which provides SIRI real-time transit data for Israeli public transportation.
 * 
 * @note API Documentation:
 *   - Endpoint: GET https://curlbus.app/{stationId}
 *   - Header: Accept: application/json
 *   - Response: JSON with visits array containing bus arrivals
 */

#include "Network/CurlBusFetcher.h"

// =========================================
// Constructor & Destructor
// =========================================

/**
 * @brief Constructor - allocates JSON buffer once on heap.
 * This avoids heap fragmentation from repeated allocations.
 */
CurlbusFetcher::CurlbusFetcher() 
    : _doc(new DynamicJsonDocument(CURLBUS_JSON_BUFFER_SIZE)) {
    if (!_doc) {
        Serial.println("[CurlbusFetcher] FATAL: Failed to allocate JSON buffer!");
    }
    _url[0] = '\0';
    // Skip HTTPS certificate verification (curlbus.app is a public API)
    _secureClient.setInsecure();
    _secureClient.setTimeout(10);  // 10 second timeout for TLS handshake
    #ifdef DEBUG
    Serial.printf("[CurlbusFetcher] Initialized with %d byte JSON buffer\n",
         CURLBUS_JSON_BUFFER_SIZE);
    #endif
}

/**
 * @brief Destructor - frees the pre-allocated JSON buffer.
 */
CurlbusFetcher::~CurlbusFetcher() {
    delete _doc;
    #ifdef DEBUG
    Serial.println("[CurlbusFetcher] Destroyed, JSON buffer freed");
    #endif
}

// =========================================
// IBusFetcher Implementation
// =========================================

/**
 * @brief Fetches and updates the ETA for a specific bus line at a station.
 * 
 * @param bus Reference to BusTarget containing stationId and line to search for.
 *            On success, bus.last_known_ETA is updated with "HH:MM" format.
 * @return true if the bus line was found and ETA was updated.
 * @return false if HTTP request failed, parsing failed, or line not found.
 */
bool CurlbusFetcher::update(BusTarget& bus) {
    if (!_doc) {
        Serial.println("[CurlbusFetcher] JSON buffer not allocated!");
        return false;
    }
    
    HTTPClient http;
    
    // =========================================
    // Step 1: Build and send HTTP request (with retry)
    // =========================================
    snprintf(_url, sizeof(_url), "%s%s?format=json", CURLBUS_API_URL, bus.stationId);
    
    int httpCode = -1;
    for (int attempt = 0; attempt < 2; attempt++) {
        if (attempt > 0) {
            Serial.printf("[CurlbusFetcher] Retry %d for station %s\n", attempt, bus.stationId);
            delay(500);  // Short delay before retry
        }
        
        // Always ensure clean connection state
        if (_secureClient.connected()) {
            _secureClient.stop();
        }
        
        http.begin(_secureClient, _url);
        http.addHeader("Accept", "application/json");
        http.setTimeout(10000);  // 10s timeout (was too long at 15s)
        
        httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) break;
        
        http.end();
    }
    
    // Check HTTP response status
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[CurlbusFetcher] HTTP GET failed, code: %d\n", httpCode);
        http.end();
        _secureClient.stop();
        return false;
    }
    
    // =========================================
    // Step 2: Create filter to parse only needed fields
    // =========================================
    // Filter reduces memory by ignoring unwanted fields.
    // Full response is ~28KB; the filter lets ArduinoJson discard
    // unneeded data during streaming parse, so we never buffer it all.
    StaticJsonDocument<768> filter;
    filter["errors"] = true;
    filter["visits"][bus.stationId][0]["line_name"] = true;
    filter["visits"][bus.stationId][0]["eta"] = true;
    filter["visits"][bus.stationId][0]["location"] = true;  // For real-time detection
    
    // =========================================
    // Step 3: Parse JSON directly from HTTP stream (no buffering)
    // =========================================
    _doc->clear();
    
    DeserializationError error = deserializeJson(
        *_doc, 
        http.getStream(),
        DeserializationOption::Filter(filter),
        DeserializationOption::NestingLimit(12)
    );
    
    http.end();
    _secureClient.stop();  // Always close TLS after request to prevent stale connections
    
    if (error) {
        Serial.printf("[CurlbusFetcher] JSON parse failed: %s\n", error.c_str());
        return false;
    }
    
    // Check for API-level errors in response
    if (!(*_doc)["errors"].isNull() && (*_doc)["errors"].size() > 0) {
        Serial.println("[CurlbusFetcher] API returned errors");
        return false;
    }
    
    // =========================================
    // Step 4: Navigate to visits for this station
    // =========================================
    // Response structure: { "visits": { "<stationId>": [ {...}, {...} ] } }
    JsonArray visits = (*_doc)["visits"][bus.stationId];
    
    if (visits.isNull() || visits.size() == 0) {
        Serial.printf("[CurlbusFetcher] No visits found for station %s\n", bus.stationId);
        return false;
    }
    
    // =========================================
    // Step 5: Search for matching bus line
    // =========================================
    for (JsonObject visit : visits) {
        const char* lineName = visit["line_name"];
        
        if (lineName && strcmp(lineName, bus.line) == 0) {
            // Found matching line - extract ETA
            const char* eta = visit["eta"];
            
            if (eta && strlen(eta) >= 16) {
                // ETA format: "2026-01-03 21:54:00+02:00"
                //              0123456789|11111
                //                        |01234
                // Extract HH:MM from character positions 11-15
                strncpy(bus.last_known_ETA, eta + 11, 5);
                bus.last_known_ETA[5] = '\0';  // Null-terminate the string
                
                // Check if real-time (has GPS location data)
                bus.is_realtime = !visit["location"].isNull();
                
                // =========================================
                // Calculate minutes remaining
                // =========================================
                // Parse ETA hours and minutes from string
                int etaHour = (eta[11] - '0') * 10 + (eta[12] - '0');
                int etaMin  = (eta[14] - '0') * 10 + (eta[15] - '0');
                
                // Get current local time
                struct tm timeinfo;
                if (getLocalTime(&timeinfo)) {
                    int nowMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
                    int etaMinutes = etaHour * 60 + etaMin;
                    bus.minutes_remaining = etaMinutes - nowMinutes;
                    
                    // Handle midnight crossing (e.g., now=23:50, eta=00:10)
                    if (bus.minutes_remaining < -720) {  // More than 12 hours negative
                        bus.minutes_remaining += 1440;   // Add 24 hours
                    }
                } else {
                    bus.minutes_remaining = -1;  // Time not available
                }
                
                #ifdef DEBUG
                Serial.printf("[CurlbusFetcher] Line %s at station %s: ETA %s (%d min)\n", 
                              bus.line, bus.stationId, bus.last_known_ETA, bus.minutes_remaining);
                #endif
                return true;
            }
        }
    }
    
    // Line not found in any of the visits
    Serial.printf("[CurlbusFetcher] Line %s not found at station %s\n", bus.line, bus.stationId);
    return false;
}

