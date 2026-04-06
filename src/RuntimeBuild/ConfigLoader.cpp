#include "RuntimeBuild/ConfigLoader.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <string.h>

namespace {

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

void setError(char* errorBuffer, size_t errorBufferLen, const char* message) {
    if (errorBuffer != nullptr && errorBufferLen > 0) {
        snprintf(errorBuffer, errorBufferLen, "%s", message);
    }
}

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

bool parseWifiConfig(RuntimeConfig& outConfig, char* errorBuffer, size_t errorBufferLen) {
    File wifiFile = LittleFS.open("/wifi_credentials.json", "r");
    if (!wifiFile) {
        setError(errorBuffer, errorBufferLen, "Missing /wifi_credentials.json");
        return false;
    }

    StaticJsonDocument<512> wifiDoc;
    DeserializationError wifiErr = deserializeJson(wifiDoc, wifiFile);
    wifiFile.close();

    if (wifiErr) {
        setError(errorBuffer, errorBufferLen, "Invalid wifi_credentials.json");
        return false;
    }

    JsonVariantConst versionValue = wifiDoc["version"];
    if (versionValue.isNull()) {
        setError(errorBuffer, errorBufferLen, "wifi_credentials.json missing version");
        return false;
    }

    int version = versionValue.as<int>();
    if (version != CONFIG_SCHEMA_VERSION) {
        setError(errorBuffer, errorBufferLen, "wifi_credentials.json schema version mismatch");
        return false;
    }

    const char* ssid = wifiDoc["ssid"];
    const char* password = wifiDoc["password"];

    if (ssid == nullptr || strlen(ssid) == 0) {
        setError(errorBuffer, errorBufferLen, "wifi_credentials.json missing ssid");
        return false;
    }

    if (password == nullptr || strlen(password) == 0) {
        setError(errorBuffer, errorBufferLen, "wifi_credentials.json missing password");
        return false;
    }

    copyBounded(outConfig.wifi.ssid, sizeof(outConfig.wifi.ssid), ssid);
    copyBounded(outConfig.wifi.password, sizeof(outConfig.wifi.password), password);

    return true;
}

bool parseBuildStateConfig(RuntimeConfig& outConfig, char* errorBuffer, size_t errorBufferLen) {
    File stateFile = LittleFS.open("/build_state.json", "r");
    if (!stateFile) {
        setError(errorBuffer, errorBufferLen, "Missing /build_state.json");
        return false;
    }

    StaticJsonDocument<256> stateDoc;
    DeserializationError stateErr = deserializeJson(stateDoc, stateFile);
    stateFile.close();

    if (stateErr) {
        setError(errorBuffer, errorBufferLen, "Invalid build_state.json");
        return false;
    }

    JsonVariantConst versionValue = stateDoc["version"];
    if (versionValue.isNull()) {
        setError(errorBuffer, errorBufferLen, "build_state.json missing version");
        return false;
    }

    int version = versionValue.as<int>();
    if (version != CONFIG_SCHEMA_VERSION) {
        setError(errorBuffer, errorBufferLen, "build_state.json schema version mismatch");
        return false;
    }

    const char* stateText = stateDoc["state"];
    if (!parseBuildState(stateText, outConfig.buildState)) {
        setError(errorBuffer, errorBufferLen, "build_state.json has unknown state");
        return false;
    }

    return true;
}

bool parseBuildInfoStationMode(RuntimeConfig& outConfig, JsonObjectConst buildInfoObj, char* errorBuffer, size_t errorBufferLen) {
    const char* stationId = buildInfoObj["stationId"];
    if (stationId == nullptr || strlen(stationId) == 0) {
        setError(errorBuffer, errorBufferLen, "build_info.json missing stationId");
        return false;
    }

    copyBounded(outConfig.bus.targets[0].stationId, sizeof(outConfig.bus.targets[0].stationId), stationId);
    outConfig.bus.targets[0].line[0] = '\0';
    outConfig.bus.targets[0].destination[0] = '\0';
    outConfig.bus.targetCount = 1;
    return true;
}

bool parseBuildInfoLinesMode(RuntimeConfig& outConfig, JsonObjectConst buildInfoObj, char* errorBuffer, size_t errorBufferLen) {
    const char* stationId = buildInfoObj["stationId"];
    if (stationId == nullptr || strlen(stationId) == 0) {
        setError(errorBuffer, errorBufferLen, "build_info.json missing stationId");
        return false;
    }

    JsonArrayConst lines = buildInfoObj["lineNumbers"].as<JsonArrayConst>();
    if (lines.isNull()) {
        setError(errorBuffer, errorBufferLen, "build_info.json missing lineNumbers array");
        return false;
    }

    if (lines.size() == 0) {
        setError(errorBuffer, errorBufferLen, "lineNumbers array cannot be empty");
        return false;
    }

    if (lines.size() > MAX_RUNTIME_TARGETS) {
        setError(errorBuffer, errorBufferLen, "Too many lines in build_info.json");
        return false;
    }

    int idx = 0;
    for (JsonVariantConst lineValue : lines) {
        const char* lineText = lineValue.as<const char*>();
        if (lineText == nullptr || strlen(lineText) == 0) {
            setError(errorBuffer, errorBufferLen, "build_info.json contains empty line");
            return false;
        }
        copyBounded(outConfig.bus.targets[idx].stationId, sizeof(outConfig.bus.targets[idx].stationId), stationId);
        copyBounded(outConfig.bus.targets[idx].line, sizeof(outConfig.bus.targets[idx].line), lineText);
        outConfig.bus.targets[idx].destination[0] = '\0';

        idx++;
    }

    outConfig.bus.targetCount = idx;
    return true;
}

bool parseBuildInfoCurrentMode(RuntimeConfig& outConfig, char* errorBuffer, size_t errorBufferLen) {
    outConfig.bus.targetCount = 0;
    (void)errorBuffer;
    (void)errorBufferLen;
    return true;
}

bool parseBuildInfoConfig(RuntimeConfig& outConfig, char* errorBuffer, size_t errorBufferLen) {
    File buildInfoFile = LittleFS.open("/build_info.json", "r");
    if (!buildInfoFile) {
        setError(errorBuffer, errorBufferLen, "Missing /build_info.json");
        return false;
    }

    StaticJsonDocument<4096> buildInfoDoc;
    DeserializationError buildInfoErr = deserializeJson(buildInfoDoc, buildInfoFile);
    buildInfoFile.close();

    if (buildInfoErr) {
        setError(errorBuffer, errorBufferLen, "Invalid build_info.json");
        return false;
    }

    JsonVariantConst versionValue = buildInfoDoc["version"];
    if (versionValue.isNull()) {
        setError(errorBuffer, errorBufferLen, "build_info.json missing version");
        return false;
    }

    int version = versionValue.as<int>();
    if (version != CONFIG_SCHEMA_VERSION) {
        setError(errorBuffer, errorBufferLen, "build_info.json schema version mismatch");
        return false;
    }

    JsonObjectConst buildInfoObj = buildInfoDoc.as<JsonObjectConst>();
    switch (outConfig.buildState) {
        case BUS_BY_STATION:
        case NY_METRO_BY_STATION:
            return parseBuildInfoStationMode(outConfig, buildInfoObj, errorBuffer, errorBufferLen);
        case BUS_BY_LINES:
            return parseBuildInfoLinesMode(outConfig, buildInfoObj, errorBuffer, errorBufferLen);
        case USE_CURRENT_BUILD:
            return parseBuildInfoCurrentMode(outConfig, errorBuffer, errorBufferLen);
        default:
            setError(errorBuffer, errorBufferLen, "Unknown build state for build_info parsing");
            return false;
    }
}

}  // namespace

bool loadRuntimeConfig(RuntimeConfig& outConfig, char* errorBuffer, size_t errorBufferLen) {
    if (!LittleFS.begin(false)) {
        setError(errorBuffer, errorBufferLen, "LittleFS mount failed");
        return false;
    }

    if (!parseWifiConfig(outConfig, errorBuffer, errorBufferLen)) {
        return false;
    }

    if (!parseBuildStateConfig(outConfig, errorBuffer, errorBufferLen)) {
        return false;
    }

    if (!parseBuildInfoConfig(outConfig, errorBuffer, errorBufferLen)) {
        return false;
    }

    return true;
}

const char* configErrorToDisplayMessage(const char* configError) {
    if (configError == nullptr || configError[0] == '\0') {
        return "Config Error";
    }

    if (strstr(configError, "LittleFS mount failed") != nullptr) {
        return "FS Mount Failed";
    }
    if (strstr(configError, "wifi_credentials.json") != nullptr) {
        return "WiFi JSON Error";
    }
    if (strstr(configError, "build_state.json") != nullptr) {
        return "State JSON Error";
    }
    if (strstr(configError, "build_info.json") != nullptr) {
        return "Build Info Error";
    }
    if (strstr(configError, "targets") != nullptr) {
        return "Targets Error";
    }
    if (strstr(configError, "lineNumbers") != nullptr) {
        return "Lines Error";
    }

    return "Config Error";
}
