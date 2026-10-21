#include "BusTarget.h"

#include <string.h>

namespace {

void copyBounded(char* dst, size_t dstSize, const char* src) {
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

} // namespace

BusTarget::BusTarget() {
    _stationId[0] = '\0';
    _line[0] = '\0';
    resetArrivals();
}

void BusTarget::setStationId(const char* stationId) {
    copyBounded(_stationId, sizeof(_stationId), stationId);
}

void BusTarget::setLine(const char* line) {
    copyBounded(_line, sizeof(_line), line);
}

void BusTarget::setDestination(const char* destination) {
    copyBounded(_destination, sizeof(_destination), destination);
}

void BusTarget::setArrival(const char* eta, int minutesRemaining, bool isRealtime) {
    copyBounded(_eta, sizeof(_eta), eta);
    _minutesRemaining = minutesRemaining;
    _isRealtime = isRealtime;
    _noData = false;
    clearNextArrival();
}

void BusTarget::setNextArrival(const char* eta, int minutesRemaining, bool isRealtime) {
    copyBounded(_nextEta, sizeof(_nextEta), eta);
    _nextMinutesRemaining = minutesRemaining;
    _nextIsRealtime = isRealtime;
    _hasNextArrival = true;
}

void BusTarget::clearNextArrival() {
    _nextEta[0] = '\0';
    _nextMinutesRemaining = 0;
    _nextIsRealtime = false;
    _hasNextArrival = false;
}

void BusTarget::resetArrivals() {
    _destination[0] = '\0';
    _eta[0] = '\0';
    _minutesRemaining = 0;
    _isRealtime = false;
    _noData = false;
    clearNextArrival();
}
