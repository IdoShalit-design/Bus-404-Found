#include "Builder.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <memory>
#include <string.h>

#include "Display/HUB75Display.h"
#include "Network/CurlBusFetcher.h"

namespace {

// File that stores the persisted build state.
const char* kBuildStateFile = "/build_state.json";

const char* buildStateToString(BuildState state) {
	switch (state) {
		case BUS_BY_STATION:
			return "BUS_BY_STATION";
		case BUS_BY_LINES:
			return "BUS_BY_LINES";
		case NY_METRO_BY_STATION:
			return "NY_METRO_BY_STATION";
		case USE_CURRENT_BUILD:
			return "USE_CURRENT_BUILD";
		default:
			return nullptr;
	}
}

/**
 * @brief Converts a persisted state string to BuildState enum.
 *
 * @param stateText State value read from JSON.
 * @param outState Parsed enum output on success.
 * @return true if the value is recognized, false otherwise.
 */
bool parseBuildState(const char* stateText, BuildState& outState) {
	if (stateText == nullptr) {
		return false;
	}

	if (strcmp(stateText, "BUS_BY_STATION") == 0) {
		outState = BUS_BY_STATION;
		return true;
	}
	if (strcmp(stateText, "BUS_BY_LINES") == 0) {
		outState = BUS_BY_LINES;
		return true;
	}
	if (strcmp(stateText, "NY_METRO_BY_STATION") == 0) {
		outState = NY_METRO_BY_STATION;
		return true;
	}
	if (strcmp(stateText, "USE_CURRENT_BUILD") == 0) {
		outState = USE_CURRENT_BUILD;
		return true;
	}

	return false;
}

/**
 * @brief Loads the current build state from /build_state.json.
 *
 * @param outState Parsed BuildState on success.
 * @return true on successful load and parse, false on any IO or parse error.
 */
bool loadBuildStateFromJson(BuildState& outState) {
	// Reuse the same persistence backend as other runtime JSON files.
	if (!LittleFS.begin(false)) {
		Serial.println("[Build] Failed to mount LittleFS");
		return false;
	}

	File stateFile = LittleFS.open(kBuildStateFile, "r");
	if (!stateFile) {
		Serial.println("[Build] Missing /build_state.json");
		return false;
	}

	StaticJsonDocument<256> doc;
	DeserializationError err = deserializeJson(doc, stateFile);
	stateFile.close();
	if (err) {
		Serial.println("[Build] Invalid build_state.json");
		return false;
	}

	const char* stateText = doc["state"];
	if (!parseBuildState(stateText, outState)) {
		Serial.println("[Build] Unknown state in build_state.json");
		return false;
	}

	return true;
}

/**
 * @brief Saves the provided build state into /build_state.json.
 *
 * @param state BuildState to persist.
 * @return true when file write succeeds, false otherwise.
 */
bool saveBuildStateToJson(BuildState state) {
	if (!LittleFS.begin(false)) {
		Serial.println("[Build] Failed to mount LittleFS");
		return false;
	}

	const char* stateText = buildStateToString(state);
	if (stateText == nullptr) {
		Serial.println("[Build] Cannot persist unknown build state");
		return false;
	}

	File stateFile = LittleFS.open(kBuildStateFile, "w");
	if (!stateFile) {
		Serial.println("[Build] Cannot open /build_state.json for write");
		return false;
	}

	StaticJsonDocument<256> doc;
	doc["version"] = 1;
	doc["state"] = stateText;

	if (serializeJson(doc, stateFile) == 0) {
		stateFile.close();
		Serial.println("[Build] Failed to write build_state.json");
		return false;
	}

	stateFile.close();
	return true;
}

} // namespace

bool build(BuildState state, std::unique_ptr<IBusFetcher>& fetcher, std::unique_ptr<IRenderer>& renderer) {
	switch (state) {
		case BUS_BY_STATION:
			Serial.println("Selected state: BUS_BY_STATION");
			saveBuildStateToJson(BUS_BY_STATION);
			return true;
		case BUS_BY_LINES:
			Serial.println("Selected state: BUS_BY_LINES");
			saveBuildStateToJson(BUS_BY_LINES);
			fetcher = std::unique_ptr<IBusFetcher>(new CurlbusFetcher());
			renderer = std::unique_ptr<IRenderer>(new HUB75Display());
			return true;
		case NY_METRO_BY_STATION:
			Serial.println("Selected state: NY_METRO_BY_STATION");
			saveBuildStateToJson(NY_METRO_BY_STATION);
			return true;
		case USE_CURRENT_BUILD:
		{
			Serial.println("Selected state: USE_CURRENT_BUILD");
			// Resolve the persisted state and dispatch recursively once.
			BuildState loadedState = BUS_BY_STATION;
			if (!loadBuildStateFromJson(loadedState)) {
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
