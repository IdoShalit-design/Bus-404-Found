#include "ConfigManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <string.h>

namespace {

const char* kBuildStateFile = "/build_state.json";
const char* kBusTargetsFile = "/bus_targets.json";
constexpr uint8_t kConfigVersion = 1;

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

bool ensureLittleFsReady() {
    if (LittleFS.begin(false)) {
        return true;
    }

    Serial.println("[ConfigManager] Failed to mount LittleFS");
    return false;
}

} // namespace

bool loadBuildStateConfig(BuildState& outState) {
    if (!ensureLittleFsReady()) {
        return false;
    }

    File stateFile = LittleFS.open(kBuildStateFile, "r");
    if (!stateFile) {
        Serial.println("[ConfigManager] Missing /build_state.json");
        return false;
    }

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, stateFile);
    stateFile.close();
    if (err) {
        Serial.println("[ConfigManager] Invalid build_state.json");
        return false;
    }

    JsonVariantConst versionValue = doc["version"];
    if (versionValue.isNull() || versionValue.as<int>() != kConfigVersion) {
        Serial.println("[ConfigManager] build_state.json schema version mismatch");
        return false;
    }

    const char* stateText = doc["state"];
    if (!parseBuildState(stateText, outState)) {
        Serial.println("[ConfigManager] Unknown state in build_state.json");
        return false;
    }

    return true;
}

bool saveBuildStateConfig(BuildState state) {
    if (!ensureLittleFsReady()) {
        return false;
    }

    const char* stateText = buildStateToString(state);
    if (stateText == nullptr) {
        Serial.println("[ConfigManager] Cannot persist unknown build state");
        return false;
    }

    File stateFile = LittleFS.open(kBuildStateFile, "w");
    if (!stateFile) {
        Serial.println("[ConfigManager] Cannot open /build_state.json for write");
        return false;
    }

    StaticJsonDocument<256> doc;
    doc["version"] = kConfigVersion;
    doc["state"] = stateText;

    if (serializeJson(doc, stateFile) == 0) {
        stateFile.close();
        Serial.println("[ConfigManager] Failed to write build_state.json");
        return false;
    }

    stateFile.close();
    return true;
}

bool saveBusTargetsConfig(const BusTarget (&targets)[3]) {
    if (!ensureLittleFsReady()) {
        return false;
    }

    File targetsFile = LittleFS.open(kBusTargetsFile, "w");
    if (!targetsFile) {
        Serial.println("[ConfigManager] Cannot open /bus_targets.json for write");
        return false;
    }

    StaticJsonDocument<1024> doc;
    doc["version"] = kConfigVersion;
    JsonArray targetsArray = doc.createNestedArray("targets");

    for (int i = 0; i < 3; i++) {
        JsonObject targetObj = targetsArray.createNestedObject();
        targetObj["stationId"] = targets[i].stationId != nullptr ? targets[i].stationId : "";
        targetObj["line"] = targets[i].line != nullptr ? targets[i].line : "";
        targetObj["destination"] = targets[i].destination;
    }

    if (serializeJsonPretty(doc, targetsFile) == 0) {
        targetsFile.close();
        Serial.println("[ConfigManager] Failed to write bus_targets.json");
        return false;
    }

    targetsFile.close();
    return true;
}
