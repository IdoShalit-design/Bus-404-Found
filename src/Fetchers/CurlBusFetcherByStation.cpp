#include "Fetchers/CurlBusFetcherByStation.h"

#include <Arduino.h>

#define CURLBUS_API_URL "https://curlbus.app/"
#define CURLBUS_JSON_BUFFER_SIZE 16384

namespace {

// One load serves every row of a fetch cycle; half the fetch interval is long
// enough for that burst of update() calls and short enough that the following
// cycle always starts from a fresh station snapshot.
constexpr unsigned long kArrivalCacheTtlMs = FETCH_INTERVAL_MS / 2;

void copyBounded(char* dst, size_t dstSize, const char* src) {
    if (dst == nullptr || dstSize == 0) {
        return;
    }

    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

int computeMinutesRemaining(const char* eta) {
    if (eta == nullptr || strlen(eta) < 16) {
        return -1;
    }

    int etaHour = (eta[11] - '0') * 10 + (eta[12] - '0');
    int etaMin = (eta[14] - '0') * 10 + (eta[15] - '0');

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return -1;
    }

    int nowMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int etaMinutes = etaHour * 60 + etaMin;
    int delta = etaMinutes - nowMinutes - 1;
    if (delta < -720) {
        delta += 1440;
    }
    return max(delta, 0);
}

} // namespace

CurlBusFetcherByStation::CurlBusFetcherByStation()
    : _doc(new DynamicJsonDocument(CURLBUS_JSON_BUFFER_SIZE)),
      _arrivalCount(0),
      _loadedAtMs(0),
      _servedLineCount(0) {
    if (!_doc) {
        Serial.println("[CurlBusFetcherByStation] FATAL: Failed to allocate JSON buffer!");
    }

    _url[0] = '\0';
    _currentStationId[0] = '\0';
    _secureClient.setInsecure();
    _secureClient.setTimeout(10);
}

CurlBusFetcherByStation::~CurlBusFetcherByStation() {
    delete _doc;
}

bool CurlBusFetcherByStation::loadNextArrivals(const char* stationId) {
    if (!_doc || stationId == nullptr || stationId[0] == '\0') {
        return false;
    }

    HTTPClient http;
    snprintf(_url, sizeof(_url), "%s%s?format=json", CURLBUS_API_URL, stationId);

    int httpCode = -1;
    for (int attempt = 0; attempt < 2; attempt++) {
        if (attempt > 0) {
            delay(500);
        }

        if (_secureClient.connected()) {
            _secureClient.stop();
        }

        http.begin(_secureClient, _url);
        http.addHeader("Accept", "application/json");
        http.setTimeout(10000);

        httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            break;
        }

        http.end();
    }

    if (httpCode != HTTP_CODE_OK) {
        http.end();
        _secureClient.stop();
        return false;
    }

    StaticJsonDocument<1024> filter;
    filter["errors"] = true;
    filter["visits"][stationId][0]["line_name"] = true;
    filter["visits"][stationId][0]["eta"] = true;
    filter["visits"][stationId][0]["location"] = true;
    filter["visits"][stationId][0]["static_info"]["route"]["destination"]["name"]["EN"] = true;

    // Read the full body first; parsing straight from getStream() stopped early on core 3.x.
    String payload = http.getString();

    http.end();
    _secureClient.stop();

    _doc->clear();
    DeserializationError error = deserializeJson(
        *_doc,
        payload,
        DeserializationOption::Filter(filter),
        DeserializationOption::NestingLimit(12));

    if (error) {
        return false;
    }

    if (!(*_doc)["errors"].isNull() && (*_doc)["errors"].size() > 0) {
        return false;
    }

    JsonArray visits = (*_doc)["visits"][stationId];
    if (visits.isNull() || visits.size() == 0) {
        return false;
    }

    _arrivalCount = 0;
    for (JsonObject visit : visits) {
        if (_arrivalCount >= kMaxStationArrivals) {
            break;
        }

        const char* lineName = visit["line_name"];
        const char* eta = visit["eta"];
        const char* destination = visit["static_info"]["route"]["destination"]["name"]["EN"];

        if (lineName == nullptr || lineName[0] == '\0' || eta == nullptr || strlen(eta) < 16) {
            continue;
        }

        copyBounded(_arrivals[_arrivalCount].line, sizeof(_arrivals[_arrivalCount].line), lineName);
        copyBounded(_arrivals[_arrivalCount].destination, sizeof(_arrivals[_arrivalCount].destination), destination);
        strncpy(_arrivals[_arrivalCount].eta, eta + 11, 5);
        _arrivals[_arrivalCount].eta[5] = '\0';
        _arrivals[_arrivalCount].is_realtime = !visit["location"].isNull();
        _arrivals[_arrivalCount].minutes_remaining = computeMinutesRemaining(eta);
        _arrivalCount++;
    }

    _servedLineCount = 0;
    _loadedAtMs = millis();
    copyBounded(_currentStationId, sizeof(_currentStationId), stationId);
    return _arrivalCount > 0;
}

bool CurlBusFetcherByStation::isLineServed(const char* line) const {
    for (size_t i = 0; i < _servedLineCount; i++) {
        if (strcmp(_servedLines[i], line) == 0) {
            return true;
        }
    }
    return false;
}

int CurlBusFetcherByStation::findUnservedArrivalIndex() const {
    if (_servedLineCount >= kMaxServedLines) {
        return -1;
    }

    for (size_t i = 0; i < _arrivalCount; i++) {
        if (!isLineServed(_arrivals[i].line)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool CurlBusFetcherByStation::update(BusTarget& bus) {
    if (bus.stationId[0] == '\0') {
        return false;
    }

    // Reload on a new station, once the snapshot ages out, or when every cached
    // line has already been given to a row. The age check is what starts each
    // fetch cycle fresh: rows are filled faster than the cache expires, so the
    // first update() of a cycle reloads and the rest share that snapshot.
    const bool stationChanged = strcmp(_currentStationId, bus.stationId) != 0;
    const bool cacheStale = (millis() - _loadedAtMs) >= kArrivalCacheTtlMs;
    if (stationChanged || cacheStale || findUnservedArrivalIndex() < 0) {
        if (!loadNextArrivals(bus.stationId)) {
            return false;
        }
    }

    const int index = findUnservedArrivalIndex();
    if (index < 0) {
        return false;
    }

    // Copy, don't alias: _arrivals is refilled in place by loadNextArrivals().
    const ArrivalData& arrival = _arrivals[index];
    copyBounded(bus.line, sizeof(bus.line), arrival.line);
    copyBounded(bus.destination, sizeof(bus.destination), arrival.destination);
    copyBounded(bus.last_known_ETA, sizeof(bus.last_known_ETA), arrival.eta);
    bus.is_realtime = arrival.is_realtime;
    bus.minutes_remaining = arrival.minutes_remaining;
    bus.no_data = false;

    // The same line's following arrival shares this row instead of consuming
    // the next one, so every row shows a different line.
    bus.has_next_arrival = false;
    for (size_t i = static_cast<size_t>(index) + 1; i < _arrivalCount; i++) {
        if (strcmp(_arrivals[i].line, arrival.line) != 0) {
            continue;
        }

        copyBounded(bus.next_ETA, sizeof(bus.next_ETA), _arrivals[i].eta);
        bus.next_is_realtime = _arrivals[i].is_realtime;
        bus.next_minutes_remaining = _arrivals[i].minutes_remaining;
        bus.has_next_arrival = true;
        break;
    }

    copyBounded(_servedLines[_servedLineCount], sizeof(_servedLines[_servedLineCount]), arrival.line);
    _servedLineCount++;
    return true;
}
