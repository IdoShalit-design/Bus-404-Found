#include "RuntimeBuild/Builder.h"

#include <Arduino.h>
#include <memory>

#include "RuntimeBuild/ConfigManager.h"

#include "Display/HUB75Display.h"
#include "Fetchers/CurlBusFetcherByStation.h"
#include "Fetchers/CurlBuseFetcherByLine.h"
#include "Fetchers/MetroFetcherStub.h"

bool build(BuildState state, std::unique_ptr<IBusFetcher>& fetcher, std::unique_ptr<IRenderer>& renderer) {
	switch (state) {
		case BUS_BY_STATION:
			Serial.println("Selected state: BUS_BY_STATION");
			saveBuildStateConfig(BUS_BY_STATION);
			fetcher = std::unique_ptr<IBusFetcher>(new CurlBusFetcherByStation());
			return true;
		case BUS_BY_LINES:
			Serial.println("Selected state: BUS_BY_LINES");
			saveBuildStateConfig(BUS_BY_LINES);
			fetcher = std::unique_ptr<IBusFetcher>(new CurlBuseFetcherByLine());
			renderer = std::unique_ptr<IRenderer>(new HUB75Display());
			return true;
		case NY_METRO_BY_STATION:
			Serial.println("Selected state: NY_METRO_BY_STATION");
			saveBuildStateConfig(NY_METRO_BY_STATION);
			fetcher = std::unique_ptr<IBusFetcher>(new MetroFetcherStub());
			return true;
		case USE_CURRENT_BUILD:
		{
			Serial.println("Selected state: USE_CURRENT_BUILD");
			// Resolve the persisted state and dispatch recursively once.
			BuildState loadedState = BUS_BY_STATION;
			if (!loadBuildStateConfig(loadedState)) {
				return false;
			}

			if (loadedState == USE_CURRENT_BUILD) {
				Serial.println("[Build] Refusing recursive USE_CURRENT_BUILD loop");
				return false;
			}

			return build(loadedState, fetcher, renderer);
		}
		default:
			Serial.println("Selected state: UNKNOWN");
			return false;
	}
}
