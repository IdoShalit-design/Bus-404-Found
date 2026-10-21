/**
 * @file CurlBusFetcherByLine.cpp
 * @brief Implementation of the CurlBusFetcherByLine class for fetching real-time bus data.
 * 
 * This fetcher retrieves bus arrival times from the CurlBus API (curlbus.app),
 * which provides SIRI real-time transit data for Israeli public transportation.
 * 
 * @note API Documentation:
 *   - Endpoint: GET https://curlbus.app/{stationId}
 *   - Header: Accept: application/json
 *   - Response: JSON with visits array containing bus arrivals
 */

#include "Fetchers/CurlBusFetcherByLine.h"

namespace {

// ETA format: "2026-01-03 21:54:00+02:00"
//              0123456789|11111
//                        |01234
// HH:MM sits at offsets 11..15, so anything shorter than 16 chars is unusable.
constexpr size_t kEtaTimeOffset = 11;
constexpr size_t kEtaTimeLength = 5;
constexpr size_t kEtaMinLength = 16;

constexpr int kMinutesPerDay = 24 * 60;
// An ETA that looks more than 12h in the past is really tomorrow's clock time
// seen from before midnight, not a stale arrival.
constexpr int kMidnightWrapThresholdMin = -12 * 60;

bool etaHasTime(const char* eta) {
    return eta != nullptr && strlen(eta) >= kEtaMinLength;
}

void copyEtaTime(char* dst, const char* eta) {
    strncpy(dst, eta + kEtaTimeOffset, kEtaTimeLength);
    dst[kEtaTimeLength] = '\0';
}

/**
 * @brief Minutes from now until the given ETA, or -1 when the clock is unset.
 *
 * Subtracts one extra minute so a bus that is arriving right now reads as 0
 * rather than "1 min". Clamps at 0 only after unwrapping midnight, so a
 * just-past-midnight ETA is not flattened to "arriving now".
 */
int computeMinutesRemaining(const char* eta) {
    int etaHour = (eta[kEtaTimeOffset] - '0') * 10 + (eta[kEtaTimeOffset + 1] - '0');
    int etaMin = (eta[kEtaTimeOffset + 3] - '0') * 10 + (eta[kEtaTimeOffset + 4] - '0');

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return -1;  // Time not available
    }

    int nowMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int delta = (etaHour * 60 + etaMin) - nowMinutes - 1;
    if (delta < kMidnightWrapThresholdMin) {
        delta += kMinutesPerDay;
    }
    return max(delta, 0);
}

// Appends the distinct line_name values in the response to the current log line.
// Bounded on purpose: a busy station lists dozens of visits, and the question
// this answers -- "is this the station I meant?" -- is settled by the first
// handful. Printed inline rather than collected into a String so a long list
// cannot allocate on a path that already holds a 16 KB document.
void printDistinctLineNames(JsonArray visits) {
    constexpr int kMaxNamesToPrint = 12;

    int printed = 0;
    for (JsonObject visit : visits) {
        const char* lineName = visit["line_name"];
        if (lineName == nullptr) {
            continue;
        }

        // O(n^2) over the visit list, which is fine: this runs only on a miss,
        // and only to build one log line.
        bool alreadyPrinted = false;
        for (JsonObject earlier : visits) {
            if (earlier == visit) {
                break;
            }
            const char* earlierName = earlier["line_name"];
            if (earlierName != nullptr && strcmp(earlierName, lineName) == 0) {
                alreadyPrinted = true;
                break;
            }
        }
        if (alreadyPrinted) {
            continue;
        }

        if (printed >= kMaxNamesToPrint) {
            Serial.print(" ...");
            break;
        }

        Serial.printf(" %s", lineName);
        printed++;
    }

    Serial.println();
}

}  // namespace

// =========================================
// Constructor & Destructor
// =========================================

/**
 * @brief Constructor - allocates JSON buffer once on heap.
 * This avoids heap fragmentation from repeated allocations.
 */
CurlBusFetcherByLine::CurlBusFetcherByLine() 
    : _doc(new DynamicJsonDocument(CURLBUS_JSON_BUFFER_SIZE)) {
    if (!_doc) {
        Serial.println("[CurlBusFetcherByLine] FATAL: Failed to allocate JSON buffer!");
    }
    _url[0] = '\0';
    // Skip HTTPS certificate verification (curlbus.app is a public API)
    _secureClient.setInsecure();
    _secureClient.setTimeout(10);  // 10 second timeout for TLS handshake
    #ifdef DEBUG
    Serial.printf("[CurlBusFetcherByLine] Initialized with %d byte JSON buffer\n",
         CURLBUS_JSON_BUFFER_SIZE);
    #endif
}

/**
 * @brief Destructor - frees the pre-allocated JSON buffer.
 */
CurlBusFetcherByLine::~CurlBusFetcherByLine() {
    delete _doc;
    #ifdef DEBUG
    Serial.println("[CurlBusFetcherByLine] Destroyed, JSON buffer freed");
    #endif
}

// =========================================
// IBusFetcher Implementation
// =========================================

/**
 * @brief Fetches the next two arrivals of a specific bus line at a station.
 *
 * @param bus Reference to BusTarget containing stationId and line to search for.
 *            On success, bus.last_known_ETA holds the next arrival in "HH:MM"
 *            format, and bus.next_ETA the one after it when the station lists a
 *            second arrival for the line (bus.has_next_arrival says whether it does).
 * @return true if the bus line was found and ETA was updated.
 * @return false if HTTP request failed, parsing failed, or line not found.
 */
bool CurlBusFetcherByLine::update(BusTarget& bus) {
    if (!_doc) {
        Serial.println("[CurlBusFetcherByLine] JSON buffer not allocated!");
        return false;
    }

    // Bind as const char* so ArduinoJson stores the pointer for filter keys
    // instead of copying the string into the document pool.
    const char* stationId = bus.stationId;

    HTTPClient http;
    
    // =========================================
    // Step 1: Build and send HTTP request (with retry)
    // =========================================
    snprintf(_url, sizeof(_url), "%s%s?format=json", CURLBUS_API_URL, stationId);
    
    int httpCode = -1;
    for (int attempt = 0; attempt < 2; attempt++) {
        if (attempt > 0) {
            Serial.printf("[CurlBusFetcherByLine] Retry %d for station %s\n", attempt, stationId);
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
        Serial.printf("[CurlBusFetcherByLine] HTTP GET failed, code: %d\n", httpCode);
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
    StaticJsonDocument<1024> filter;
    filter["errors"] = true;
    filter["visits"][stationId][0]["line_name"] = true;
    filter["visits"][stationId][0]["eta"] = true;
    filter["visits"][stationId][0]["location"] = true;  // For real-time detection
    filter["visits"][stationId][0]["static_info"]["route"]["destination"]["name"]["EN"] = true;  // Destination name
    
    // =========================================
    // Step 3: Read the full body, then parse it
    // =========================================
    // Parsing straight from http.getStream() stopped early on core 3.x
    // (IncompleteInput after ~1s). getString() reads the body in blocks and
    // honors Content-Length / chunked encoding. The ~30-40KB body goes to PSRAM.
    const int expectedSize = http.getSize();
    String payload = http.getString();

    http.end();
    _secureClient.stop();  // Always close TLS after request to prevent stale connections

    _doc->clear();

    DeserializationError error = deserializeJson(
        *_doc,
        payload,
        DeserializationOption::Filter(filter),
        DeserializationOption::NestingLimit(12)
    );

    if (error) {
        Serial.printf("[CurlBusFetcherByLine] JSON parse failed: %s (got %u of %d bytes)\n",
                      error.c_str(), payload.length(), expectedSize);
        return false;
    }
    
    // Check for API-level errors in response
    if (!(*_doc)["errors"].isNull() && (*_doc)["errors"].size() > 0) {
        Serial.println("[CurlBusFetcherByLine] API returned errors");
        return false;
    }
    
    // =========================================
    // Step 4: Navigate to visits for this station
    // =========================================
    // Response structure: { "visits": { "<stationId>": [ {...}, {...} ] } }
    JsonArray visits = (*_doc)["visits"][stationId];
    
    if (visits.isNull() || visits.size() == 0) {
        Serial.printf("[CurlBusFetcherByLine] No visits found for station %s\n", stationId);
        return false;
    }
    
    // =========================================
    // Step 5: Collect the next two arrivals of the matching bus line
    // =========================================
    // The API lists visits in arrival order, so the first match is the next
    // arrival and the second match the one after it.
    int matchCount = 0;
    bool destinationSeen = false;

    // Counted separately from matchCount so the two ways this can come back
    // empty stay distinguishable: the line is not at this station at all, or it
    // is here but none of its visits carry a time we can read. Both used to
    // report "not found", which is wrong in the second case and sends the reader
    // to check the station ID when the station ID was never the problem.
    int lineNameMatches = 0;

    for (JsonObject visit : visits) {
        const char* lineName = visit["line_name"];
        if (lineName == nullptr || strcmp(lineName, bus.line) != 0) {
            continue;
        }
        lineNameMatches++;

        // Take the destination from the first visit of this line, whether or not
        // its ETA turns out to be usable. The row is labelled by the line, not by
        // an arrival: skipping this on an unusable ETA leaves the destination
        // blank forever for a line whose visits never carry a parseable eta,
        // which reads as a broken row rather than as a missing arrival.
        if (!destinationSeen) {
            const char* destName = visit["static_info"]["route"]["destination"]["name"]["EN"];
            if (destName) {
                strncpy(bus.destination, destName, sizeof(bus.destination) - 1);
                bus.destination[sizeof(bus.destination) - 1] = '\0';
                destinationSeen = true;
            }
        }

        const char* eta = visit["eta"];
        if (!etaHasTime(eta)) {
            continue;
        }

        if (matchCount == 0) {
            copyEtaTime(bus.last_known_ETA, eta);
            // Real-time when the visit carries GPS location data
            bus.is_realtime = !visit["location"].isNull();
            bus.minutes_remaining = computeMinutesRemaining(eta);
            bus.has_next_arrival = false;
            matchCount = 1;
            continue;
        }

        copyEtaTime(bus.next_ETA, eta);
        bus.next_is_realtime = !visit["location"].isNull();
        bus.next_minutes_remaining = computeMinutesRemaining(eta);
        bus.has_next_arrival = true;
        matchCount = 2;
        break;
    }

    if (matchCount == 0) {
        if (lineNameMatches > 0) {
            // The line is here; its arrivals are not readable. Nothing about the
            // station or the line number needs changing, so say so.
            Serial.printf("[CurlBusFetcherByLine] Line %s at station %s: %d visit(s), none with a usable eta\n",
                          bus.line, stationId, lineNameMatches);
        } else {
            // Genuinely absent. The line names that *were* present are the
            // fastest way to tell a wrong station ID from a wrong line number,
            // and from a line that simply has no service right now.
            Serial.printf("[CurlBusFetcherByLine] Line %s not at station %s. Present (%u visits):",
                          bus.line, stationId, static_cast<unsigned>(visits.size()));
            printDistinctLineNames(visits);
        }
        return false;
    }

    Serial.printf("[CurlBusFetcherByLine] Line %s at station %s: ETA %s (%d min), next %s (%d min)\n",
                  bus.line, stationId, bus.last_known_ETA, bus.minutes_remaining,
                  bus.has_next_arrival ? bus.next_ETA : "-",
                  bus.has_next_arrival ? bus.next_minutes_remaining : -1);
    return true;
}

