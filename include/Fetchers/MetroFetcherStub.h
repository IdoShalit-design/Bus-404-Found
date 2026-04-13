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
        bool isRealtime;
        int minutesRemaining;
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
