#include "Network/RuntimeBuildPortal.h"

#include <DNSServer.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "Config.h"
#include "RuntimeBuild/ConfigLoader.h"
#include "RuntimeBuild/ConfigManager.h"

namespace {

constexpr const char* kApSsid = "Bus-404-Found-Setup";
constexpr size_t kLineCsvMaxLen = 128;

struct SubmissionState {
    bool finished;
    RuntimeBuildPortalResult result;
    const char* details;
};

bool tryStaConnect(const char* ssid, const char* password, unsigned long timeoutMs) {
    WiFi.disconnect();
    WiFi.begin(ssid, password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
        delay(200);
    }

    return WiFi.status() == WL_CONNECTED;
}

bool parseBuildStateFromForm(const String& modeValue, BuildState& outState) {
    if (modeValue == "BUS_BY_STATION") {
        outState = BUS_BY_STATION;
        return true;
    }
    if (modeValue == "BUS_BY_LINES") {
        outState = BUS_BY_LINES;
        return true;
    }
    if (modeValue == "NY_METRO_BY_STATION") {
        outState = NY_METRO_BY_STATION;
        return true;
    }
    if (modeValue == "USE_CURRENT_BUILD") {
        outState = USE_CURRENT_BUILD;
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

size_t parseLineCsv(const String& csv, char output[MAX_RUNTIME_TARGETS][MAX_LINE_LEN], const char* linePtrs[MAX_RUNTIME_TARGETS]) {
    if (csv.length() == 0 || csv.length() >= kLineCsvMaxLen) {
        return 0;
    }

    size_t count = 0;
    int start = 0;

    while (start < static_cast<int>(csv.length()) && count < MAX_RUNTIME_TARGETS) {
        int comma = csv.indexOf(',', start);
        String token = comma == -1 ? csv.substring(start) : csv.substring(start, comma);
        token.trim();

        if (token.length() > 0) {
            if (token.length() >= MAX_LINE_LEN) {
                return 0;
            }

            token.toCharArray(output[count], MAX_LINE_LEN);
            linePtrs[count] = output[count];
            count++;
        }

        if (comma == -1) {
            break;
        }
        start = comma + 1;
    }

    if (count == 0) {
        return 0;
    }

    if (csv.indexOf(',', start) != -1) {
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

    WiFi.mode(WIFI_AP_STA);
    if (!WiFi.softAP(kApSsid)) {
        Serial.println("[RuntimeBuildPortal] Failed to start AP");
        return PORTAL_INTERNAL_ERROR;
    }

    IPAddress apIp = WiFi.softAPIP();
    dnsServer.start(53, "*", apIp);

    server.on("/", HTTP_GET, [&server]() {
        if (!serveStaticFile(server, "/portal/index.html", "text/html")) {
            server.send(500, "text/plain", "Missing /portal/index.html");
        }
    });

    server.on("/portal.css", HTTP_GET, [&server]() {
        if (!serveStaticFile(server, "/portal/portal.css", "text/css")) {
            server.send(404, "text/plain", "Missing /portal/portal.css");
        }
    });

    server.on("/portal.js", HTTP_GET, [&server]() {
        if (!serveStaticFile(server, "/portal/portal.js", "application/javascript")) {
            server.send(404, "text/plain", "Missing /portal/portal.js");
        }
    });

    server.on("/generate_204", HTTP_GET, [&server]() {
        server.sendHeader("Location", "http://192.168.4.1/", true);
        server.send(302, "text/plain", "");
    });

    server.on("/fwlink", HTTP_GET, [&server]() {
        server.sendHeader("Location", "http://192.168.4.1/", true);
        server.send(302, "text/plain", "");
    });

    server.on("/submit", HTTP_POST, [&server, &submissionState]() {
        BuildState selectedState = BUS_BY_STATION;

        const String modeValue = server.arg("state");
        const String ssidValue = server.arg("ssid");
        const String passwordValue = server.arg("password");

        if (!parseBuildStateFromForm(modeValue, selectedState)) {
            submissionState.finished = true;
            submissionState.result = PORTAL_VALIDATION_ERROR;
            submissionState.details = "Invalid activation mode";
            server.send(400, "text/html", buildResultHtml(submissionState.details));
            return;
        }

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

        if (!saveBuildStateConfig(selectedState)) {
            submissionState.finished = true;
            submissionState.result = PORTAL_INTERNAL_ERROR;
            submissionState.details = "Failed to save build state";
            server.send(500, "text/html", buildResultHtml(submissionState.details));
            return;
        }

        bool saveBuildInfoOk = false;
        switch (selectedState) {
            case BUS_BY_STATION:
            case NY_METRO_BY_STATION: {
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
            case BUS_BY_LINES: {
                const String stationValue = server.arg("linesStationId");
                if (!isLengthValid(stationValue.c_str(), MAX_STATION_ID_LEN)) {
                    submissionState.finished = true;
                    submissionState.result = PORTAL_VALIDATION_ERROR;
                    submissionState.details = "Station ID is required for BUS_BY_LINES";
                    server.send(400, "text/html", buildResultHtml(submissionState.details));
                    return;
                }

                char lineStorage[MAX_RUNTIME_TARGETS][MAX_LINE_LEN] = {};
                const char* linePtrs[MAX_RUNTIME_TARGETS] = {};
                size_t lineCount = parseLineCsv(server.arg("lineNumbers"), lineStorage, linePtrs);
                if (lineCount == 0) {
                    submissionState.finished = true;
                    submissionState.result = PORTAL_VALIDATION_ERROR;
                    submissionState.details = "Provide 1-3 valid line numbers";
                    server.send(400, "text/html", buildResultHtml(submissionState.details));
                    return;
                }

                saveBuildInfoOk = saveBuildInfoLinesConfig(stationValue.c_str(), linePtrs, lineCount);
                break;
            }
            case USE_CURRENT_BUILD:
                saveBuildInfoOk = saveBuildInfoCurrentConfig();
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

        const bool connected = tryStaConnect(ssidValue.c_str(), passwordValue.c_str(), RUNTIME_BUILD_PORTAL_STA_CONNECT_TIMEOUT_MS);
        submissionState.finished = true;
        submissionState.result = connected ? PORTAL_SAVE_AND_CONNECT_OK : PORTAL_SAVE_OK_CONNECT_FAILED;
        submissionState.details = connected ? "Config saved. Wi-Fi connected." : "Config saved. Wi-Fi connect failed.";
        server.send(200, "text/html", buildResultHtml(submissionState.details));
    });

    server.onNotFound([&server]() {
        server.sendHeader("Location", "http://192.168.4.1/", true);
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

    Serial.printf("[RuntimeBuildPortal] Exit: %d (%s)\n", static_cast<int>(submissionState.result), submissionState.details);
    return submissionState.result;
}
