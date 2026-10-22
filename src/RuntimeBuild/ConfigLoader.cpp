#include "RuntimeBuild/ConfigLoader.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <string.h>

#include "RuntimeBuild/BuildStateNames.h"
#include "RuntimeBuild/ConfigManager.h"

namespace {

constexpr ConfigResult kOk = {true, nullptr};

ConfigResult failure(const char* message) {
    return {false, message};
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

ConfigResult parseWifiConfig(RuntimeConfig& outConfig) {
    File wifiFile = LittleFS.open("/wifi_credentials.json", "r");
    if (!wifiFile) {
        return failure("Missing /wifi_credentials.json");
    }

    StaticJsonDocument<512> wifiDoc;
    DeserializationError wifiErr = deserializeJson(wifiDoc, wifiFile);
    wifiFile.close();

    if (wifiErr) {
        return failure("Invalid wifi_credentials.json");
    }

    JsonVariantConst versionValue = wifiDoc["version"];
    if (versionValue.isNull()) {
        return failure("wifi_credentials.json missing version");
    }

    int version = versionValue.as<int>();
    if (version != CONFIG_SCHEMA_VERSION) {
        return failure("wifi_credentials.json schema version mismatch");
    }

    const char* ssid = wifiDoc["ssid"];
    const char* password = wifiDoc["password"];

    if (ssid == nullptr || strlen(ssid) == 0) {
        return failure("wifi_credentials.json missing ssid");
    }

    if (password == nullptr || strlen(password) == 0) {
        return failure("wifi_credentials.json missing password");
    }

    copyBounded(outConfig.wifi.ssid, sizeof(outConfig.wifi.ssid), ssid);
    copyBounded(outConfig.wifi.password, sizeof(outConfig.wifi.password), password);

    return kOk;
}

ConfigResult parseBuildStateConfig(RuntimeConfig& outConfig) {
    File stateFile = LittleFS.open("/build_state.json", "r");
    if (!stateFile) {
        return failure("Missing /build_state.json");
    }

    StaticJsonDocument<256> stateDoc;
    DeserializationError stateErr = deserializeJson(stateDoc, stateFile);
    stateFile.close();

    if (stateErr) {
        return failure("Invalid build_state.json");
    }

    JsonVariantConst versionValue = stateDoc["version"];
    if (versionValue.isNull()) {
        return failure("build_state.json missing version");
    }

    int version = versionValue.as<int>();
    if (version != CONFIG_SCHEMA_VERSION) {
        return failure("build_state.json schema version mismatch");
    }

    const char* stateText = stateDoc["state"];
    if (!parseBuildState(stateText, outConfig.buildState)) {
        return failure("build_state.json has unknown state");
    }

    return kOk;
}

ConfigResult parseBuildInfoStationMode(RuntimeConfig& outConfig, JsonObjectConst buildInfoObj) {
    const char* stationId = buildInfoObj["stationId"];
    if (stationId == nullptr || strlen(stationId) == 0) {
        return failure("build_info.json missing stationId");
    }

    for (size_t i = 0; i < MAX_RUNTIME_TARGETS; i++) {
        outConfig.bus.targets[i].setStationId(stationId);
        outConfig.bus.targets[i].setLine("");
        outConfig.bus.targets[i].resetArrivals();
    }

    outConfig.bus.targetCount = MAX_RUNTIME_TARGETS;
    return kOk;
}

ConfigResult parseBuildInfoLinesMode(RuntimeConfig& outConfig, JsonObjectConst buildInfoObj) {
    JsonArrayConst targets = buildInfoObj["targets"].as<JsonArrayConst>();
    if (targets.isNull()) {
        return failure("build_info.json missing targets array");
    }

    if (targets.size() == 0) {
        return failure("targets array cannot be empty");
    }

    if (targets.size() > MAX_RUNTIME_TARGETS) {
        return failure("Too many targets in build_info.json");
    }

    int idx = 0;
    for (JsonVariantConst targetValue : targets) {
        JsonObjectConst target = targetValue.as<JsonObjectConst>();
        const char* stationId = target["stationId"];
        const char* lineText = target["line"];

        if (stationId == nullptr || strlen(stationId) == 0) {
            return failure("build_info.json target missing stationId");
        }
        if (lineText == nullptr || strlen(lineText) == 0) {
            return failure("build_info.json target missing line");
        }

        outConfig.bus.targets[idx].setStationId(stationId);
        outConfig.bus.targets[idx].setLine(lineText);
        outConfig.bus.targets[idx].resetArrivals();

        idx++;
    }

    outConfig.bus.targetCount = idx;
    return kOk;
}

ConfigResult parseBuildInfoConfig(RuntimeConfig& outConfig, BuildState concreteState) {
    File buildInfoFile = LittleFS.open("/build_info.json", "r");
    if (!buildInfoFile) {
        return failure("Missing /build_info.json");
    }

    // Heap, not stack. As a StaticJsonDocument this put 4 KB into the frame of
    // whichever caller reached it, and the worst of those is a portal form
    // submission: WebServer handler chain, this parser and LittleFS all nested on
    // the same stack. That combination overran loopTask and rebooted the board.
    // The heap has room to spare here (~200 KB free while the portal is up).
    DynamicJsonDocument buildInfoDoc(4096);
    DeserializationError buildInfoErr = deserializeJson(buildInfoDoc, buildInfoFile);
    buildInfoFile.close();

    if (buildInfoErr) {
        return failure("Invalid build_info.json");
    }

    JsonVariantConst versionValue = buildInfoDoc["version"];
    if (versionValue.isNull()) {
        return failure("build_info.json missing version");
    }

    int version = versionValue.as<int>();
    if (version != CONFIG_SCHEMA_VERSION) {
        return failure("build_info.json schema version mismatch");
    }

    JsonObjectConst buildInfoObj = buildInfoDoc.as<JsonObjectConst>();
    switch (concreteState) {
        case BuildState::BUS_BY_STATION:
        case BuildState::NY_METRO_BY_STATION:
            return parseBuildInfoStationMode(outConfig, buildInfoObj);
        case BuildState::BUS_BY_LINES:
            return parseBuildInfoLinesMode(outConfig, buildInfoObj);
        case BuildState::USE_CURRENT_BUILD:
            return failure("Concrete state cannot be USE_CURRENT_BUILD");
        default:
            return failure("Unknown build state for build_info parsing");
    }
}

}  // namespace

ConfigResult loadRuntimeConfig(RuntimeConfig& outConfig) {
    if (!LittleFS.begin(false)) {
        return failure("LittleFS mount failed");
    }

    ConfigResult result = parseWifiConfig(outConfig);
    if (!result.ok) {
        return result;
    }

    result = parseBuildStateConfig(outConfig);
    if (!result.ok) {
        return result;
    }

    result = resolveConcreteBuildState(outConfig, outConfig.concreteBuildState);
    if (!result.ok) {
        return result;
    }

    return parseBuildInfoConfig(outConfig, outConfig.concreteBuildState);
}

ConfigResult validateBuildInfoConfig(BuildState concreteState) {
    if (!LittleFS.begin(false)) {
        return failure("LittleFS mount failed");
    }

    // Parse into a throwaway config: only the verdict matters here, and reusing
    // the real parser keeps this check honest as the payload format evolves.
    RuntimeConfig scratch;
    return parseBuildInfoConfig(scratch, concreteState);
}

ConfigResult resolveConcreteBuildState(const RuntimeConfig& config, BuildState& outState) {
    if (config.buildState != BuildState::USE_CURRENT_BUILD) {
        outState = config.buildState;
        return kOk;
    }

    BuildState persistedConcreteState = BuildState::BUS_BY_STATION;
    if (!loadLastConcreteBuildStateConfig(persistedConcreteState)) {
        return failure("Missing /last_concrete_build_state.json");
    }

    if (persistedConcreteState == BuildState::USE_CURRENT_BUILD) {
        return failure("last_concrete_build_state.json cannot be USE_CURRENT_BUILD");
    }

    outState = persistedConcreteState;
    return kOk;
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

    return "Config Error";
}
