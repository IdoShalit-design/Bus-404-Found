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
    // Initialize buffers to empty strings
    _url[0] = '\0';
    _payload[0] = '\0';
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
 *            On success, bus.arrivals[] is updated with up to MAX_ARRIVALS entries.
 * @return true if the bus line was found and at least one ETA was updated.
 * @return false if HTTP request failed, parsing failed, or line not found.
 */
bool CurlbusFetcher::update(BusTarget& bus) {
    HTTPClient http;
    
    // =========================================
    // Step 1: Build and send HTTP request
    // =========================================
    snprintf(_url, sizeof(_url), "%s%s", CURLBUS_API_URL, bus.stationId);
    
    http.begin(_url);
    http.addHeader("Accept", "application/json");
    
    int httpCode = http.GET();
    
    // Check HTTP response status
    if (httpCode != HTTP_CODE_OK) {
        #ifdef DEBUG
        Serial.printf("[CurlbusFetcher] HTTP GET failed, code: %d\n", httpCode);
        #endif
        http.end();
        return false;
    }
    
    size_t len = http.getSize();
    if (len > sizeof(_payload) - 1) {
        #ifdef DEBUG
        Serial.printf("[CurlbusFetcher] Payload too large: %d bytes\n", len);
        #endif
        http.end();
        return false;
    }
    http.getStream().readBytes(_payload, len);
    _payload[len] = '\0';
    http.end();
    
    // =========================================
    // Step 2: Create filter to parse only needed fields
    // =========================================
    // Filter reduces memory by ignoring unwanted fields.
    // Full response is ~28KB, filtered is ~2-4KB.
    StaticJsonDocument<512> filter;
    filter["errors"] = true;
    filter["visits"][bus.stationId][0]["line_name"] = true;
    filter["visits"][bus.stationId][0]["eta"] = true;
    filter["visits"][bus.stationId][0]["location"] = true;  // For real-time detection
    
    // =========================================
    // Step 3: Parse JSON with filter
    // =========================================
    // Clear the pre-allocated document before reuse
    _doc->clear();
    
    DeserializationError error = deserializeJson(
        *_doc, 
        _payload,
        DeserializationOption::Filter(filter)
    );
    
    if (error) {
        #ifdef DEBUG
        Serial.printf("[CurlbusFetcher] JSON parse failed: %s\n", error.c_str());
        Serial.printf("[CurlbusFetcher] Payload size: %d bytes\n", strlen(_payload));
        #endif
        return false;
    }
    
    // Check for API-level errors in response
    if (!(*_doc)["errors"].isNull() && (*_doc)["errors"].size() > 0) {
        #ifdef DEBUG
        Serial.println("[CurlbusFetcher] API returned errors");
        #endif
        return false;
    }
    
    // =========================================
    // Step 4: Navigate to visits for this station
    // =========================================
    // Response structure: { "visits": { "<stationId>": [ {...}, {...} ] } }
    JsonArray visits = (*_doc)["visits"][bus.stationId];
    
    if (visits.isNull() || visits.size() == 0) {
        #ifdef DEBUG
        Serial.printf("[CurlbusFetcher] No visits found for station %s\n", bus.stationId);
        #endif
        return false;
    }
    
    // =========================================
    // Step 5: Search for matching bus line (up to MAX_ARRIVALS)
    // =========================================
    bus.arrival_count = 0;

    for (JsonObject visit : visits) {
        if (bus.arrival_count >= MAX_ARRIVALS) break;

        const char* lineName = visit["line_name"];
        
        if (lineName && strcmp(lineName, bus.line) == 0) {
            // Found matching line - extract ETA
            const char* eta = visit["eta"];
            
            if (eta && strlen(eta) >= 16) {
                ArrivalInfo& arr = bus.arrivals[bus.arrival_count];

                // ETA format: "2026-01-03 21:54:00+02:00"
                //              0123456789|11111
                //                        |01234
                // Extract HH:MM from character positions 11-15
                strncpy(arr.eta, eta + 11, 5);
                arr.eta[5] = '\0';  // Null-terminate the string
                
                // Check if real-time (has GPS location data)
                arr.is_realtime = !visit["location"].isNull();
                
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
                    arr.minutes_remaining = etaMinutes - nowMinutes;
                    
                    // Handle midnight crossing (e.g., now=23:50, eta=00:10)
                    if (arr.minutes_remaining < -720) {  // More than 12 hours negative
                        arr.minutes_remaining += 1440;   // Add 24 hours
                    }
                } else {
                    arr.minutes_remaining = -1;  // Time not available
                }
                
                #ifdef DEBUG
                Serial.printf("[CurlbusFetcher] Line %s at station %s: arrival #%d ETA %s (%d min)\n", 
                              bus.line, bus.stationId, bus.arrival_count + 1,
                              arr.eta, arr.minutes_remaining);
                #endif
                bus.arrival_count++;
            }
        }
    }
    
    if (bus.arrival_count == 0) {
        // Line not found in any of the visits
        #ifdef DEBUG
        Serial.printf("[CurlbusFetcher] Line %s not found at station %s\n", bus.line, bus.stationId);
        #endif
        return false;
    }

    return true;
}

