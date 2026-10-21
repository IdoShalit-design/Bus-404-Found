#ifndef METRO_FETCHER_STUB_H
#define METRO_FETCHER_STUB_H

#include "IBusFetcher.h"

#include <cstddef>

class MetroFetcherStub : public IBusFetcher {
public:
    struct StubArrival {
        const char* line;
        const char* destination;
        const char* eta;
        bool is_realtime;
        int minutes_remaining;
        // Arrival after that. A null eta stands for a line with nothing else
        // scheduled, so the display path for a missing second arrival is
        // exercised without needing the network.
        const char* next_eta;
        bool next_is_realtime;
        int next_minutes_remaining;
    };

private:
    static constexpr std::size_t kStubCount = 3;
    std::size_t _nextIndex;

    static const StubArrival kStubArrivals[kStubCount];

public:
    MetroFetcherStub();
    bool update(BusTarget& bus) override;
};

#endif // METRO_FETCHER_STUB_H
