/**
 * @file IBusFetcher.h
 * @brief Interface definition for bus data retrieval strategies.
 */

#ifndef IBUS_FETCHER_H
#define IBUS_FETCHER_H

#include "BusType.h"

/**
 * @class IBusFetcher
 * @brief Abstract base class (Interface) for fetching bus data.
 * * This class defines the contract for any data fetching strategy.
 * Implementations could include CurlbusFetcher, GovIlFetcher, or MockFetcher.
 */
class IBusFetcher {
public:
    /**
     * @brief Virtual destructor.
     * Essential for proper cleanup of derived classes when deleted via a base pointer.
     */
    virtual ~IBusFetcher() {}

    /**
     * @brief Updates the status of a specific bus target.
     * * @param[in,out] bus Reference to the BusTarget struct to be updated.
     * @return true if the update was successful and the line was found.
     * @return false if the update failed (network error or line not found).
     * * @note This is a Pure Virtual Function (= 0). Derived classes MUST implement it.
     */
    virtual bool update(BusTarget& bus) = 0;
};

#endif // IBUS_FETCHER_H