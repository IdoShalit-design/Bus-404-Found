#include "RuntimeBuild/ConfigManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <string.h>

namespace {

const char* kBuildStateFile = "/build_state.json";
const char* kLastConcreteBuildStateFile = "/last_concrete_build_state.json";
const char* kWifiCredentialsFile = "/wifi_credentials.json";
const char* kBuildInfoFile = "/build_info.json";
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

bool writeBuildInfoDoc(const JsonDocument& doc) {
    File buildInfoFile = LittleFS.open(kBuildInfoFile, "w");
    if (!buildInfoFile) {
        Serial.println("[ConfigManager] Cannot open /build_info.json for write");
        return false;
    }

    if (serializeJsonPretty(doc, buildInfoFile) == 0) {
        buildInfoFile.close();
        Serial.println("[ConfigManager] Failed to write build_info.json");
        return false;
    }

    buildInfoFile.close();
    return true;
}

bool loadBuildStateFromFile(const char* filePath, const char* displayName, BuildState& outState) {
    File stateFile = LittleFS.open(filePath, "r");
    if (!stateFile) {
        Serial.printf("[ConfigManager] Missing %s\n", displayName);
        return false;
    }

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, stateFile);
    stateFile.close();
    if (err) {
        Serial.printf("[ConfigManager] Invalid %s\n", displayName);
        return false;
    }

    JsonVariantConst versionValue = doc["version"];
    if (versionValue.isNull() || versionValue.as<int>() != kConfigVersion) {
        Serial.printf("[ConfigManager] %s schema version mismatch\n", displayName);
        return false;
    }

    const char* stateText = doc["state"];
    if (!parseBuildState(stateText, outState)) {
        Serial.printf("[ConfigManager] Unknown state in %s\n", displayName);
        return false;
    }

    return true;
}

bool saveBuildStateToFile(const char* filePath, const char* displayName, BuildState state) {
    const char* stateText = buildStateToString(state);
    if (stateText == nullptr) {
        Serial.printf("[ConfigManager] Cannot persist unknown build state to %s\n", displayName);
        return false;
    }

    File stateFile = LittleFS.open(filePath, "w");
    if (!stateFile) {
        Serial.printf("[ConfigManager] Cannot open %s for write\n", displayName);
        return false;
    }

    StaticJsonDocument<256> doc;
    doc["version"] = kConfigVersion;
    doc["state"] = stateText;

    if (serializeJson(doc, stateFile) == 0) {
        stateFile.close();
        Serial.printf("[ConfigManager] Failed to write %s\n", displayName);
        return false;
    }

    stateFile.close();
    return true;
}

} // namespace

bool loadBuildStateConfig(BuildState& outState) {
    if (!ensureLittleFsReady()) {
        return false;
    }

    return loadBuildStateFromFile(kBuildStateFile, "/build_state.json", outState);
}

bool saveBuildStateConfig(BuildState state) {
    if (!ensureLittleFsReady()) {
        return false;
    }

    return saveBuildStateToFile(kBuildStateFile, "/build_state.json", state);
}

bool loadLastConcreteBuildStateConfig(BuildState& outState) {
    if (!ensureLittleFsReady()) {
        return false;
    }

    return loadBuildStateFromFile(kLastConcreteBuildStateFile, "/last_concrete_build_state.json", outState);
}

bool saveLastConcreteBuildStateConfig(BuildState state) {
    if (!ensureLittleFsReady()) {
        return false;
    }

    if (state == USE_CURRENT_BUILD) {
        Serial.println("[ConfigManager] Refusing to save USE_CURRENT_BUILD as concrete state");
        return false;
    }

    return saveBuildStateToFile(kLastConcreteBuildStateFile, "/last_concrete_build_state.json", state);
}

bool saveWifiCredentialsConfig(const char* ssid, const char* password) {
    if (!ensureLittleFsReady()) {
        return false;
    }

    if (ssid == nullptr || ssid[0] == '\0') {
        Serial.println("[ConfigManager] Cannot persist empty Wi-Fi SSID");
        return false;
    }

    if (password == nullptr || password[0] == '\0') {
        Serial.println("[ConfigManager] Cannot persist empty Wi-Fi password");
        return false;
    }

    File wifiFile = LittleFS.open(kWifiCredentialsFile, "w");
    if (!wifiFile) {
        Serial.println("[ConfigManager] Cannot open /wifi_credentials.json for write");
        return false;
    }

    StaticJsonDocument<512> doc;
    doc["version"] = kConfigVersion;
    doc["ssid"] = ssid;
    doc["password"] = password;

    if (serializeJsonPretty(doc, wifiFile) == 0) {
        wifiFile.close();
        Serial.println("[ConfigManager] Failed to write wifi_credentials.json");
        return false;
    }

    wifiFile.close();
    return true;
}

bool saveBuildInfoStationConfig(const char* stationId) {
    if (!ensureLittleFsReady()) {
        return false;
    }

    if (stationId == nullptr || stationId[0] == '\0') {
        Serial.println("[ConfigManager] Cannot persist empty stationId in build_info.json");
        return false;
    }

    StaticJsonDocument<512> doc;
    doc["version"] = kConfigVersion;
    doc["stationId"] = stationId;
    return writeBuildInfoDoc(doc);
}

bool saveBuildInfoLinesConfig(const char* stationId, const char* const* lines, size_t lineCount) {
    if (!ensureLittleFsReady()) {
        return false;
    }

    if (stationId == nullptr || stationId[0] == '\0') {
        Serial.println("[ConfigManager] Cannot persist empty stationId in build_info.json");
        return false;
    }

    if (lines == nullptr || lineCount == 0) {
        Serial.println("[ConfigManager] build_info.json lines array cannot be empty");
        return false;
    }

    StaticJsonDocument<1024> doc;
    doc["version"] = kConfigVersion;
    doc["stationId"] = stationId;
    JsonArray linesArray = doc.createNestedArray("lineNumbers");

    for (size_t i = 0; i < lineCount; i++) {
        const char* lineValue = lines[i];
        if (lineValue == nullptr || lineValue[0] == '\0') {
            Serial.println("[ConfigManager] Empty line value in build_info.json");
            return false;
        }

        linesArray.add(lineValue);
    }

    return writeBuildInfoDoc(doc);
}

bool saveBuildInfoCurrentConfig() {
    if (!ensureLittleFsReady()) {
        return false;
    }

    StaticJsonDocument<256> doc;
    doc["version"] = kConfigVersion;
    doc["useCurrentBuild"] = true;
    return writeBuildInfoDoc(doc);
}

bool saveBusTargetsConfig(const BusTarget (&targets)[3]) {
    const char* stationId = targets[0].stationId;
    const char* lines[3] = {
        targets[0].line,
        targets[1].line,
        targets[2].line,
    };

    return saveBuildInfoLinesConfig(stationId, lines, 3);
}
