#include "RuntimeBuild/RuntimeBuildPortal.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "Config.h"
#include "RuntimeBuild/ConfigLoader.h"
#include "RuntimeBuild/ConfigManager.h"

namespace {

constexpr const char* kApSsid = "Bus-404-Found-Setup";
constexpr const char* kLineStationFields[MAX_RUNTIME_TARGETS] = {"line1StationId", "line2StationId", "line3StationId"};
constexpr const char* kLineNumberFields[MAX_RUNTIME_TARGETS] = {"line1Number", "line2Number", "line3Number"};

struct SubmissionState {
    bool finished;
    RuntimeBuildPortalResult result;
    const char* details;
};

bool tryStaConnect(const char* ssid, const char* password, unsigned long timeoutMs) {
    WiFi.begin(ssid, password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
        delay(200);
    }

    return WiFi.status() == WL_CONNECTED;
}

bool parseBuildStateFromForm(const String& modeValue, BuildState& outState) {
    if (modeValue == "BUS_BY_STATION") {
        outState = BuildState::BUS_BY_STATION;
        return true;
    }
    if (modeValue == "BUS_BY_LINES") {
        outState = BuildState::BUS_BY_LINES;
        return true;
    }
    if (modeValue == "NY_METRO_BY_STATION") {
        outState = BuildState::NY_METRO_BY_STATION;
        return true;
    }
    if (modeValue == "USE_CURRENT_BUILD") {
        outState = BuildState::USE_CURRENT_BUILD;
        return true;
    }

    return false;
}

bool isLengthValid(const char* value, size_t maxLen) {
    return value != nullptr && value[0] != '\0' && strlen(value) < maxLen;
}

bool serveStaticFile(WebServer& server, const char* path, const char* contentType) {
    File file = LittleFS.open(path, "r");
    if (!file) {
        return false;
    }

    server.streamFile(file, contentType);
    file.close();
    return true;
}

// Reads up to MAX_RUNTIME_TARGETS (stationId, line) pairs from the form.
// A pair is only included if both fields are non-empty; a pair with just one
// field filled in is treated as a validation error. Returns 0 on any error.
size_t parseLinePairs(WebServer& server,
                       char stationStorage[MAX_RUNTIME_TARGETS][MAX_STATION_ID_LEN],
                       char lineStorage[MAX_RUNTIME_TARGETS][MAX_LINE_LEN],
                       const char* stationPtrs[MAX_RUNTIME_TARGETS],
                       const char* linePtrs[MAX_RUNTIME_TARGETS],
                       const char** errorDetails) {
    size_t count = 0;

    for (size_t i = 0; i < MAX_RUNTIME_TARGETS; i++) {
        String stationValue = server.arg(kLineStationFields[i]);
        String lineValue = server.arg(kLineNumberFields[i]);
        stationValue.trim();
        lineValue.trim();

        const bool hasStation = stationValue.length() > 0;
        const bool hasLine = lineValue.length() > 0;

        if (!hasStation && !hasLine) {
            continue;
        }

        if (!hasStation || !hasLine) {
            *errorDetails = "Each line entry needs both a station ID and a line number";
            return 0;
        }

        if (!isLengthValid(stationValue.c_str(), MAX_STATION_ID_LEN) ||
            !isLengthValid(lineValue.c_str(), MAX_LINE_LEN)) {
            *errorDetails = "Invalid station ID or line number";
            return 0;
        }

        stationValue.toCharArray(stationStorage[count], MAX_STATION_ID_LEN);
        lineValue.toCharArray(lineStorage[count], MAX_LINE_LEN);
        stationPtrs[count] = stationStorage[count];
        linePtrs[count] = lineStorage[count];
        count++;
    }

    if (count == 0) {
        *errorDetails = "Provide 1-3 station/line pairs";
        return 0;
    }

    return count;
}

String buildResultHtml(const char* message) {
    String html;
    html.reserve(512);
    html += "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>Runtime Build Result</title></head><body style='font-family:Segoe UI,Arial,sans-serif;padding:24px;background:#f4f6fb;'>";
    html += "<h2>";
    html += message;
    html += "</h2><p>You can close this page.</p></body></html>";
    return html;
}

} // namespace

RuntimeBuildPortalResult runRuntimeBuildPortal() {
    DNSServer dnsServer;
    WebServer server(80);
    SubmissionState submissionState = {false, PORTAL_INTERNAL_ERROR, "Internal error"};

    if (!LittleFS.begin(false)) {
        Serial.println("[RuntimeBuildPortal] LittleFS mount failed");
        return PORTAL_INTERNAL_ERROR;
    }

    // Ensure a clean WiFi state before starting the AP: stop any background
    // reconnect attempts that would fight over the radio channel.
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false);
    delay(100);

    WiFi.mode(WIFI_AP_STA);
    delay(100);  // Let the mode transition settle before starting the AP.

    if (!WiFi.softAP(kApSsid)) {
        Serial.println("[RuntimeBuildPortal] Failed to start AP");
        WiFi.setAutoReconnect(true);
        return PORTAL_INTERNAL_ERROR;
    }

    // softAPIP() can return 0.0.0.0 for a brief window after softAP() returns.
    // Wait until the interface has its real address before handing it to the
    // DNS server, otherwise the wildcard redirect points at 0.0.0.0.
    IPAddress apIp;
    {
        unsigned long ipWait = millis();
        while ((apIp = WiFi.softAPIP()) == IPAddress(0, 0, 0, 0) && millis() - ipWait < 3000) {
            delay(50);
        }
    }

    if (apIp == IPAddress(0, 0, 0, 0)) {
        Serial.println("[RuntimeBuildPortal] AP IP not ready after timeout");
        WiFi.softAPdisconnect(true);
        WiFi.setAutoReconnect(true);
        return PORTAL_INTERNAL_ERROR;
    }

    dnsServer.start(53, "*", apIp);

    server.on("/", HTTP_GET, [&server]() {
        if (!serveStaticFile(server, "/portal/index.html", "text/html")) {
            server.send(500, "text/plain", "Missing /portal/index.html");
        }
    });

    // Android captive-portal probe. Returning 302 keeps the network in
    // "captive portal" state so Android's sign-in WebView stays bound to
    // the WiFi interface. Returning 204 would mark the network as "has
    // internet" and cancel the WiFi binding, causing all subsequent requests
    // (including the portal page load) to route through cellular.
    server.on("/generate_204", HTTP_GET, [&apIp, &server]() {
        String url = String("http://") + apIp.toString() + "/";
        server.sendHeader("Location", url, true);
        server.send(302, "text/plain", "");
    });

    // iOS captive-portal probe. Returning anything other than the "Success"
    // page triggers the captive portal popup and keeps the WebView WiFi-bound.
    server.on("/hotspot-detect.html", HTTP_GET, [&apIp, &server]() {
        String url = String("http://") + apIp.toString() + "/";
        server.sendHeader("Location", url, true);
        server.send(302, "text/plain", "");
    });

    // Windows / older captive-portal redirect.
    server.on("/fwlink", HTTP_GET, [&apIp, &server]() {
        String url = String("http://") + apIp.toString() + "/";
        server.sendHeader("Location", url, true);
        server.send(302, "text/plain", "");
    });

    server.on("/wifi-status", HTTP_GET, [&server]() {
        char savedSsid[MAX_WIFI_SSID_LEN] = {};
        char savedPassword[MAX_WIFI_PASSWORD_LEN] = {};

        if (!loadWifiCredentialsConfig(savedSsid, sizeof(savedSsid), savedPassword, sizeof(savedPassword))) {
            server.send(200, "application/json", "{\"hasSaved\":false}");
            return;
        }

        StaticJsonDocument<192> doc;
        doc["hasSaved"] = true;
        doc["ssid"] = savedSsid;
        String body;
        serializeJson(doc, body);
        server.send(200, "application/json", body);
    });

    server.on("/submit", HTTP_POST, [&server, &submissionState]() {
        BuildState selectedState = BuildState::BUS_BY_STATION;

        const String modeValue = server.arg("state");
        const bool useSavedWifi = server.arg("useSavedWifi") == "1";

        if (!parseBuildStateFromForm(modeValue, selectedState)) {
            submissionState.finished = true;
            submissionState.result = PORTAL_VALIDATION_ERROR;
            submissionState.details = "Invalid activation mode";
            server.send(400, "text/html", buildResultHtml(submissionState.details));
            return;
        }

        char savedSsid[MAX_WIFI_SSID_LEN] = {};
        char savedPassword[MAX_WIFI_PASSWORD_LEN] = {};
        String ssidValue;
        String passwordValue;

        if (useSavedWifi) {
            if (!loadWifiCredentialsConfig(savedSsid, sizeof(savedSsid), savedPassword, sizeof(savedPassword))) {
                submissionState.finished = true;
                submissionState.result = PORTAL_VALIDATION_ERROR;
                submissionState.details = "No saved Wi-Fi credentials found";
                server.send(400, "text/html", buildResultHtml(submissionState.details));
                return;
            }
            ssidValue = savedSsid;
            passwordValue = savedPassword;
        } else {
            ssidValue = server.arg("ssid");
            passwordValue = server.arg("password");

            if (!isLengthValid(ssidValue.c_str(), MAX_WIFI_SSID_LEN) ||
                !isLengthValid(passwordValue.c_str(), MAX_WIFI_PASSWORD_LEN)) {
                submissionState.finished = true;
                submissionState.result = PORTAL_VALIDATION_ERROR;
                submissionState.details = "Invalid Wi-Fi credentials";
                server.send(400, "text/html", buildResultHtml(submissionState.details));
                return;
            }

            if (!saveWifiCredentialsConfig(ssidValue.c_str(), passwordValue.c_str())) {
                submissionState.finished = true;
                submissionState.result = PORTAL_INTERNAL_ERROR;
                submissionState.details = "Failed to save Wi-Fi config";
                server.send(500, "text/html", buildResultHtml(submissionState.details));
                return;
            }
        }

        if (!saveBuildStateConfig(selectedState)) {
            submissionState.finished = true;
            submissionState.result = PORTAL_INTERNAL_ERROR;
            submissionState.details = "Failed to save build state";
            server.send(500, "text/html", buildResultHtml(submissionState.details));
            return;
        }

        if (selectedState != BuildState::USE_CURRENT_BUILD && !saveLastConcreteBuildStateConfig(selectedState)) {
            submissionState.finished = true;
            submissionState.result = PORTAL_INTERNAL_ERROR;
            submissionState.details = "Failed to save concrete build state";
            server.send(500, "text/html", buildResultHtml(submissionState.details));
            return;
        }

        bool saveBuildInfoOk = false;
        switch (selectedState) {
            case BuildState::BUS_BY_STATION:
            case BuildState::NY_METRO_BY_STATION: {
                const String stationValue = server.arg("stationId");
                if (!isLengthValid(stationValue.c_str(), MAX_STATION_ID_LEN)) {
                    submissionState.finished = true;
                    submissionState.result = PORTAL_VALIDATION_ERROR;
                    submissionState.details = "Station ID is required";
                    server.send(400, "text/html", buildResultHtml(submissionState.details));
                    return;
                }

                saveBuildInfoOk = saveBuildInfoStationConfig(stationValue.c_str());
                break;
            }
            case BuildState::BUS_BY_LINES: {
                char stationStorage[MAX_RUNTIME_TARGETS][MAX_STATION_ID_LEN] = {};
                char lineStorage[MAX_RUNTIME_TARGETS][MAX_LINE_LEN] = {};
                const char* stationPtrs[MAX_RUNTIME_TARGETS] = {};
                const char* linePtrs[MAX_RUNTIME_TARGETS] = {};
                const char* errorDetails = nullptr;

                size_t pairCount = parseLinePairs(server, stationStorage, lineStorage, stationPtrs, linePtrs, &errorDetails);
                if (pairCount == 0) {
                    submissionState.finished = true;
                    submissionState.result = PORTAL_VALIDATION_ERROR;
                    submissionState.details = errorDetails;
                    server.send(400, "text/html", buildResultHtml(submissionState.details));
                    return;
                }

                saveBuildInfoOk = saveBuildInfoLinesConfig(stationPtrs, linePtrs, pairCount);
                break;
            }
            case BuildState::USE_CURRENT_BUILD:
                // Keep existing build_info payload so resolver can reuse the last concrete mode data.
                saveBuildInfoOk = true;
                break;
            default:
                break;
        }

        if (!saveBuildInfoOk) {
            submissionState.finished = true;
            submissionState.result = PORTAL_INTERNAL_ERROR;
            submissionState.details = "Failed to save build info";
            server.send(500, "text/html", buildResultHtml(submissionState.details));
            return;
        }

        // Respond immediately so the browser doesn't time out waiting while
        // we spend up to RUNTIME_BUILD_PORTAL_STA_CONNECT_TIMEOUT_MS trying
        // to connect to the home network.
        server.send(200, "text/html", buildResultHtml("Config saved. Connecting to Wi-Fi\u2026"));

        const bool connected = tryStaConnect(ssidValue.c_str(), passwordValue.c_str(), RUNTIME_BUILD_PORTAL_STA_CONNECT_TIMEOUT_MS);
        submissionState.finished = true;
        submissionState.result = connected ? PORTAL_SAVE_AND_CONNECT_OK : PORTAL_SAVE_OK_CONNECT_FAILED;
        submissionState.details = connected ? "Config saved. Wi-Fi connected." : "Config saved. Wi-Fi connect failed.";
    });

    server.onNotFound([&apIp, &server]() {
        String url = String("http://") + apIp.toString() + "/";
        server.sendHeader("Location", url, true);
        server.send(302, "text/plain", "");
    });

    server.begin();
    Serial.printf("[RuntimeBuildPortal] AP started. SSID: %s IP: %s\n", kApSsid, WiFi.softAPIP().toString().c_str());

    unsigned long start = millis();
    while (!submissionState.finished) {
        dnsServer.processNextRequest();
        server.handleClient();

        if (WiFi.softAPgetStationNum() == 0 && (millis() - start) >= RUNTIME_BUILD_PORTAL_NO_CLIENT_TIMEOUT_MS) {
            submissionState.finished = true;
            submissionState.result = PORTAL_TIMEOUT_NO_CLIENT;
            submissionState.details = "Timed out waiting for AP client";
            break;
        }

        if (WiFi.softAPgetStationNum() > 0) {
            start = millis();
        }

        delay(10);
    }

    server.stop();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    Serial.printf("[RuntimeBuildPortal] Exit: %d (%s)\n", static_cast<int>(submissionState.result), submissionState.details);
    return submissionState.result;
}
