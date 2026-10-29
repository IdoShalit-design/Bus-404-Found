#include "Fetchers/NYSubwayFetcher.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <time.h>

#define SUBWAY_API_URL "https://subwayinfo.nyc/api/arrivals"
#define SUBWAY_JSON_BUFFER_SIZE 2048

namespace {

// Two arrivals fill a row: the one it leads with and the one after.
constexpr int kArrivalsPerRow = 2;

int parseDigits(const char* text, int count) {
    int value = 0;
    for (int i = 0; i < count; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return -1;
        }
        value = value * 10 + (text[i] - '0');
    }
    return value;
}

// Days from 1970-01-01 to the given civil date (proleptic Gregorian). newlib
// has no timegm(), and mktime() would read the fields as local time.
long daysFromCivil(int year, int month, int day) {
    year -= month <= 2 ? 1 : 0;
    const long era = (year >= 0 ? year : year - 399) / 400;
    const long yearOfEra = year - era * 400;
    const long dayOfYear = (153L * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const long dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

/**
 * @brief Parses a UTC ISO 8601 time such as "2026-10-29T06:38:17.000Z".
 * @return true and the epoch time on success.
 */
bool parseUtcIso8601(const char* text, time_t& outEpoch) {
    if (text == nullptr || strlen(text) < 19 || text[4] != '-' || text[7] != '-' ||
        text[10] != 'T' || text[13] != ':' || text[16] != ':') {
        return false;
    }

    const int year = parseDigits(text, 4);
    const int month = parseDigits(text + 5, 2);
    const int day = parseDigits(text + 8, 2);
    const int hour = parseDigits(text + 11, 2);
    const int minute = parseDigits(text + 14, 2);
    const int second = parseDigits(text + 17, 2);
    if (year < 0 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || minute < 0 || second < 0) {
        return false;
    }

    outEpoch = static_cast<time_t>(daysFromCivil(year, month, day)) * 86400 +
               hour * 3600 + minute * 60 + second;
    return true;
}

/**
 * @brief Converts one API arrival into the row's "HH:MM" and minutes.
 *
 * The ETA is shown in local time, so it relies on the NY timezone that
 * App::syncClock() sets for this build state. Minutes come from the device
 * clock when it is synced, because the API's minutesAway is frozen at the
 * moment the server built its response. It is the fallback otherwise.
 */
bool readArrival(JsonObjectConst arrival, char* etaOut, size_t etaLen, int& minutesOut) {
    time_t arrivalEpoch;
    if (!parseUtcIso8601(arrival["arrivalTime"], arrivalEpoch)) {
        return false;
    }

    struct tm local;
    localtime_r(&arrivalEpoch, &local);
    snprintf(etaOut, etaLen, "%02d:%02d", local.tm_hour, local.tm_min);

    const time_t now = time(nullptr);
    struct tm nowTm;
    if (getLocalTime(&nowTm, 0)) {
        const long seconds = static_cast<long>(arrivalEpoch - now);
        minutesOut = seconds > 0 ? static_cast<int>(seconds / 60) : 0;
    } else {
        minutesOut = max(arrival["minutesAway"] | 0, 0);
    }
    return true;
}

} // namespace

NYSubwayFetcher::NYSubwayFetcher()
    : _doc(new DynamicJsonDocument(SUBWAY_JSON_BUFFER_SIZE)) {
    if (!_doc) {
        Serial.println("[NYSubwayFetcher] FATAL: Failed to allocate JSON buffer!");
    }

    _url[0] = '\0';
    _secureClient.setInsecure();
    _secureClient.setTimeout(10);
}

NYSubwayFetcher::~NYSubwayFetcher() {
    delete _doc;
}

bool NYSubwayFetcher::update(BusTarget& bus) {
    if (!_doc || bus.stationId()[0] == '\0' || bus.line()[0] == '\0' ||
        bus.direction() == Direction::None) {
        return false;
    }

    snprintf(_url, sizeof(_url), "%s?station_id=%s&line=%s&direction=%s&limit=%d",
             SUBWAY_API_URL, bus.stationId(), bus.line(),
             directionToString(bus.direction()), kArrivalsPerRow);

    HTTPClient http;
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
        // Retry only a request that never got an HTTP answer. Any status code
        // means the server counted it against the rate limit.
        if (httpCode > 0) {
            break;
        }

        http.end();
    }

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[NYSubwayFetcher] HTTP %d for %s\n", httpCode, _url);
        http.end();
        _secureClient.stop();
        return false;
    }

    StaticJsonDocument<256> filter;
    filter["arrivals"][0]["arrivalTime"] = true;
    filter["arrivals"][0]["minutesAway"] = true;
    filter["arrivals"][0]["isAssigned"] = true;
    filter["arrivals"][0]["headsign"] = true;

    // Read the full body first; parsing straight from getStream() stopped early on core 3.x.
    String payload = http.getString();

    http.end();
    _secureClient.stop();

    _doc->clear();
    DeserializationError error = deserializeJson(*_doc, payload, DeserializationOption::Filter(filter));
    if (error) {
        Serial.printf("[NYSubwayFetcher] JSON error: %s\n", error.c_str());
        return false;
    }

    JsonArrayConst arrivals = (*_doc)["arrivals"];
    if (arrivals.isNull() || arrivals.size() == 0) {
        return false;
    }

    char eta[MAX_ETA_LEN];
    int minutes = 0;
    JsonObjectConst first = arrivals[0];
    if (!readArrival(first, eta, sizeof(eta), minutes)) {
        return false;
    }

    // headsign is the terminal station, e.g. "Wakefield-241 St".
    bus.setDestination(first["headsign"] | "");
    // isAssigned: a real train is on the trip, not only a scheduled one.
    bus.setArrival(eta, minutes, first["isAssigned"] | false);

    if (arrivals.size() >= 2) {
        JsonObjectConst second = arrivals[1];
        if (readArrival(second, eta, sizeof(eta), minutes)) {
            bus.setNextArrival(eta, minutes, second["isAssigned"] | false);
        }
    }
    return true;
}
