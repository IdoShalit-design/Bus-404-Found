#include "Fetchers/MetroFetcherStub.h"

const MetroFetcherStub::StubArrival MetroFetcherStub::kStubArrivals[MetroFetcherStub::kStubCount] = {
    {"M1", "Central Station", "08:12", true, 2, "08:27", true, 17},
    {"M3", "City Hall", "08:16", false, 6, "08:46", false, 36},
    {"M2", "Airport Terminal", "08:21", true, 11, nullptr, false, 0},
};

MetroFetcherStub::MetroFetcherStub() : _nextIndex(0) {}

bool MetroFetcherStub::update(BusTarget& bus) {
    const StubArrival& arrival = kStubArrivals[_nextIndex];
    _nextIndex = (_nextIndex + 1) % kStubCount;

    bus.setLine(arrival.line);
    bus.setDestination(arrival.destination);
    bus.setArrival(arrival.eta, arrival.minutes_remaining, arrival.is_realtime);

    if (arrival.next_eta != nullptr) {
        bus.setNextArrival(arrival.next_eta, arrival.next_minutes_remaining,
                           arrival.next_is_realtime);
    }
    return true;
}
