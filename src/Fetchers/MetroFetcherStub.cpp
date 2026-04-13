#include "Fetchers/MetroFetcherStub.h"

#include <string.h>

namespace {

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

} // namespace

const MetroFetcherStub::StubArrival MetroFetcherStub::kStubArrivals[MetroFetcherStub::kStubCount] = {
    {"M1", "Central Station", "08:12", true, 2},
    {"M3", "City Hall", "08:16", false, 6},
    {"M2", "Airport Terminal", "08:21", true, 11},
};

MetroFetcherStub::MetroFetcherStub() : _nextIndex(0) {}

bool MetroFetcherStub::update(BusTarget& bus) {
    const StubArrival& arrival = kStubArrivals[_nextIndex];
    _nextIndex = (_nextIndex + 1) % kStubCount;

    bus.line = arrival.line;
    copyBounded(bus.destination, sizeof(bus.destination), arrival.destination);
    copyBounded(bus.last_known_ETA, sizeof(bus.last_known_ETA), arrival.eta);
    bus.is_realtime = arrival.isRealtime;
    bus.minutes_remaining = arrival.minutesRemaining;
    bus.no_data = false;
    return true;
}
