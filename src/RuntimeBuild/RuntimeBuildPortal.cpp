#include "RuntimeBuild/RuntimeBuildPortal.h"

#include <DNSServer.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_netif.h>
#include <esp_wifi.h>
// ESP-IDF 5.x moved the "which client holds which lease" lookup here and renamed
// it: esp_netif_get_sta_list() and its esp_netif_sta_list_t are gone, replaced by
// esp_wifi_ap_get_sta_list_with_ip() and wifi_sta_mac_ip_list_t. See logApClients.
#include <esp_wifi_ap_get_sta_list.h>
// dhcps_lease_t, for setting the AP's address pool explicitly. Same path the
// core's own NetworkInterface.cpp uses.
#include "dhcpserver/dhcpserver.h"

#include "Config.h"
#include "RuntimeBuild/BuildStateNames.h"
#include "RuntimeBuild/ConfigLoader.h"
#include "RuntimeBuild/ConfigManager.h"

namespace {

constexpr const char* kApSsid = "Bus-404-Found-Setup";

constexpr uint16_t kHttpPort = 80;
constexpr uint16_t kDnsPort = 53;

// The AP's own address and the subnet it hands out. These are the values the
// core would pick by default, but setting them explicitly is what guarantees the
// DHCP server is (re)started on the AP netif: softAPConfig() stops DHCPS, writes
// the IP info and starts it again. Relying on the default leaves a client able to
// associate -- the panel says "Connected" -- while holding no lease, and with no
// address on the subnet it cannot reach the portal at all.
const IPAddress kApIp(192, 168, 4, 1);
const IPAddress kApGateway(192, 168, 4, 1);
const IPAddress kApSubnet(255, 255, 255, 0);

// The addresses the DHCP server may hand out. Stated explicitly rather than left
// to the default, because the default is what goes missing: a server with no pool
// still starts, still answers, and still leases nothing. Deliberately clear of
// kApIp -- a pool containing the AP's own address is rejected outright.
const IPAddress kApDhcpPoolStart(192, 168, 4, 2);
const IPAddress kApDhcpPoolEnd(192, 168, 4, 11);

// Two separate settle points: after tearing down any prior STA connection, and
// after the AP+STA mode switch. Same duration today, but they are not the same
// wait, so changing one should not silently change the other.
constexpr unsigned long kRadioDisconnectSettleMs = 100UL;
constexpr unsigned long kModeChangeSettleMs = 100UL;

// softAPIP() briefly reports 0.0.0.0 after softAP() returns; how long to wait for
// the real address, and how often to re-check.
constexpr unsigned long kApIpReadyTimeoutMs = 3000UL;
constexpr unsigned long kApIpPollMs = 50UL;

// How long to wait for the AP netif to report itself started. Separate from the
// address wait above: the address is written synchronously by softAPConfig(),
// while the started flag is raised from the WiFi event task, so the two become
// true at different moments and only the latter gates enableDhcpCaptivePortal().
constexpr unsigned long kApStartedTimeoutMs = 3000UL;

// Yield between service passes so the portal loop does not starve WiFi/system tasks.
constexpr unsigned long kPortalLoopTickMs = 10UL;

// How often to re-read the AP's client table. Fast enough to catch a DHCP
// handshake completing (typically well under a second after association), slow
// enough that the read itself is not part of the portal's workload.
constexpr unsigned long kClientPollIntervalMs = 2000UL;

// Poll interval while waiting on the STA join.
constexpr unsigned long kStaPollIntervalMs = 200UL;

// Grace period between closing the submit response's connection and retuning the
// radio. Short, but the close has to actually reach the client: the alternative
// is the browser losing the reply to a deauthentication it cannot distinguish
// from the device having crashed.
constexpr unsigned long kSubmitResponseSettleMs = 250UL;

// Covers the result page's fixed markup plus a typical message, so the String
// does not reallocate while being assembled. Only an optimisation: a longer
// message still grows it.
constexpr size_t kResultHtmlReserveBytes = 512;

// How long to keep the AP up with nobody associated. This is wall-clock time for
// a person to notice the panel, open Wi-Fi settings and pick the network, so it
// is set generously: at the previous 40s the AP could vanish mid-join, which from
// the phone is indistinguishable from the portal being broken.
constexpr unsigned long kNoClientTimeoutMs = 120000UL;

// How long to wait for the STA join after a submission is already saved.
constexpr unsigned long kStaConnectTimeoutMs = 10000UL;

// Hard cap on a whole portal session. The no-client timeout is reset whenever a
// station is associated, so on its own it cannot bound anything: a phone that
// auto-joins the AP and then idles would block boot indefinitely, since the
// portal runs before the rest of App::setup().
constexpr unsigned long kMaxSessionMs = 300000UL;

// Must stay in step with the per-line inputs in data/portal/index.html, which
// cannot be checked at compile time. Sizing these from their initialisers rather
// than from MAX_RUNTIME_TARGETS means raising that constant is a build error here
// instead of a nullptr handed to WebServer::arg() at runtime.
constexpr const char* kLineStationFields[] = {"line1StationId", "line2StationId", "line3StationId"};
constexpr const char* kLineNumberFields[] = {"line1Number", "line2Number", "line3Number"};
// Read only in NY subway mode, where each pair also needs a direction.
constexpr const char* kLineDirectionFields[] = {"line1Direction", "line2Direction", "line3Direction"};

static_assert(sizeof(kLineStationFields) / sizeof(kLineStationFields[0]) == MAX_RUNTIME_TARGETS,
              "Add a station field name per runtime target, here and in data/portal/index.html");
static_assert(sizeof(kLineNumberFields) / sizeof(kLineNumberFields[0]) == MAX_RUNTIME_TARGETS,
              "Add a line field name per runtime target, here and in data/portal/index.html");
static_assert(sizeof(kLineDirectionFields) / sizeof(kLineDirectionFields[0]) == MAX_RUNTIME_TARGETS,
              "Add a direction field name per runtime target, here and in data/portal/index.html");

struct SubmissionState {
    bool finished;
    RuntimeBuildPortalResult result;
    const char* details;
};

bool tryStaConnect(const char* ssid, const char* password, unsigned long timeoutMs) {
    // Enable the station interface only now. The portal runs in pure AP mode (see
    // startProvisioningAp), so this is the first point where STA is wanted. The AP
    // stays up alongside it: the client is still on the result page, and dropping
    // the AP mid-request would look like the failure this whole function reports.
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid, password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
        delay(kStaPollIntervalMs);
    }

    return WiFi.status() == WL_CONNECTED;
}

// True when a USE_CURRENT_BUILD selection would actually resolve on the next
// boot: a concrete state must be persisted, and its build_info payload must
// parse. validateBuildInfoConfig rejects USE_CURRENT_BUILD itself, so a corrupt
// last-concrete file is caught here too.
bool hasKeepableBuild() {
    BuildState previousState = BuildState::BUS_BY_STATION;
    if (!loadLastConcreteBuildStateConfig(previousState)) {
        return false;
    }

    const ConfigResult buildInfo = validateBuildInfoConfig(previousState);
    if (!buildInfo.ok) {
        Serial.printf("[RuntimeBuildPortal] Cannot keep current build: %s\n", buildInfo.error);
        return false;
    }

    return true;
}

// Reads a form field with surrounding whitespace stripped. WebServer::arg returns
// an empty String for an absent field, so "missing" and "blank" collapse to the
// same value here -- both are rejected by isLengthValid, which is what we want.
//
// Every field goes through this except the Wi-Fi password: whitespace can be
// meaningful there, it is typed into a masked input where the user cannot see
// what was trimmed, and silently altering it would produce a device that saves
// credentials it can never connect with.
String readTrimmedArg(WebServer& server, const char* name) {
    String value = server.arg(name);
    value.trim();
    return value;
}

bool isLengthValid(const char* value, size_t maxLen) {
    return value != nullptr && value[0] != '\0' && strlen(value) < maxLen;
}

// Returns false only when the file could not be opened at all. A short write is
// reported but still counts as served: the client has already been given a
// response, and there is nothing better to send it afterwards.
bool serveStaticFile(WebServer& server, const char* path, const char* contentType) {
    File file = LittleFS.open(path, "r");
    if (!file || file.isDirectory()) {
        Serial.printf("[RuntimeBuildPortal] open(%s) failed\n", path);
        return false;
    }

    // Captured before streaming: streamFile() runs the connection to completion,
    // and a client that disconnects mid-transfer takes the peer address with it.
    const String peer = server.client().remoteIP().toString();
    const size_t declared = file.size();
    const size_t sent = server.streamFile(file, contentType);
    file.close();

    // Logged as a pair because the two ways this goes wrong are indistinguishable
    // from the browser -- both are a blank page -- and completely distinct from
    // here. declared == 0 means the filesystem image is wrong; sent < declared
    // means the page exists and the connection died carrying it.
    Serial.printf("[RuntimeBuildPortal] served %s: %u/%u bytes to %s\n", path,
                  static_cast<unsigned>(sent), static_cast<unsigned>(declared), peer.c_str());
    return true;
}

// Lists what is actually present at the path the portal serves from. Runs once
// per session, before any request can arrive, because "the page was never
// flashed" and "the page is there but never reaches the browser" both present as
// a blank page and nothing else in the log separates them.
void logPortalAssets() {
    File dir = LittleFS.open("/portal");
    if (!dir || !dir.isDirectory()) {
        Serial.println("[RuntimeBuildPortal] /portal missing or not a directory - run 'pio run -t uploadfs'");
        return;
    }

    for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
        Serial.printf("[RuntimeBuildPortal]   asset %s (%u bytes)\n", entry.name(),
                      static_cast<unsigned>(entry.size()));
        entry.close();
    }
    dir.close();
}

// The (stationId, line) pairs a submission asked for, owning its own storage.
// `error` follows the ConfigResult convention in ConfigLoader.h: nullptr on
// success, otherwise a string literal, so there is nothing to free.
struct LineTargets {
    char stations[MAX_RUNTIME_TARGETS][MAX_STATION_ID_LEN];
    char lines[MAX_RUNTIME_TARGETS][MAX_LINE_LEN];
    // "N" or "S"; filled only when parseLinePairs() is asked for directions.
    char directions[MAX_RUNTIME_TARGETS][2];
    size_t count;
    const char* error;
};

// Reads up to MAX_RUNTIME_TARGETS (stationId, line) pairs from the form. A pair
// is included only if both fields are filled in; one field on its own is a
// validation error rather than a partially usable target. With withDirection,
// each included pair must also carry a direction of N or S.
LineTargets parseLinePairs(WebServer& server, bool withDirection) {
    LineTargets targets = {};

    for (size_t i = 0; i < MAX_RUNTIME_TARGETS; i++) {
        const String stationValue = readTrimmedArg(server, kLineStationFields[i]);
        const String lineValue = readTrimmedArg(server, kLineNumberFields[i]);

        const bool hasStation = stationValue.length() > 0;
        const bool hasLine = lineValue.length() > 0;

        if (!hasStation && !hasLine) {
            continue;
        }

        if (!hasStation || !hasLine) {
            targets.error = "Each line entry needs both a station ID and a line number";
            return targets;
        }

        if (!isLengthValid(stationValue.c_str(), MAX_STATION_ID_LEN) ||
            !isLengthValid(lineValue.c_str(), MAX_LINE_LEN)) {
            targets.error = "Invalid station ID or line number";
            return targets;
        }

        if (withDirection) {
            Direction direction = Direction::None;
            if (!parseDirection(readTrimmedArg(server, kLineDirectionFields[i]).c_str(), direction)) {
                targets.error = "Each subway entry needs a direction (N or S)";
                return targets;
            }
            const char* directionText = directionToString(direction);
            targets.directions[targets.count][0] = directionText[0];
            targets.directions[targets.count][1] = '\0';
        }

        stationValue.toCharArray(targets.stations[targets.count], MAX_STATION_ID_LEN);
        lineValue.toCharArray(targets.lines[targets.count], MAX_LINE_LEN);
        targets.count++;
    }

    if (targets.count == 0) {
        targets.error = "Provide 1-3 station/line pairs";
    }

    return targets;
}

// Escapes text for interpolation into an HTML element body or a quoted
// attribute. Every caller of buildResultHtml passes a literal today, but the
// obvious next change -- echoing the offending value back, "Invalid station ID:
// <input>" -- would otherwise turn the result page into reflected XSS against a
// portal served over the air, with nothing in the signature to warn against it.
String escapeHtml(const char* text) {
    String escaped;
    if (text == nullptr) {
        return escaped;
    }

    for (const char* cursor = text; *cursor != '\0'; cursor++) {
        switch (*cursor) {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            case '\'':
                escaped += "&#39;";
                break;
            default:
                escaped += *cursor;
                break;
        }
    }

    return escaped;
}

String buildResultHtml(const char* message) {
    String html;
    html.reserve(kResultHtmlReserveBytes);
    html += "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>Runtime Build Result</title></head><body style='font-family:Segoe UI,Arial,sans-serif;padding:24px;background:#f4f6fb;'>";
    html += "<h2>";
    html += escapeHtml(message);
    html += "</h2><p>You can close this page.</p></body></html>";
    return html;
}

// Rejects the current submission but leaves the portal running, so the user can
// correct the form and resubmit. submissionState is deliberately left untouched:
// a rejected attempt must not become the portal's reported outcome.
void rejectSubmission(WebServer& server, const char* details) {
    Serial.printf("[RuntimeBuildPortal] Rejected submission: %s\n", details);
    server.send(400, "text/html", buildResultHtml(details));
}

// Ends the portal session. Reserved for failures the user cannot recover from by
// resubmitting, such as a flash write failure.
void abortSubmission(SubmissionState& submissionState, WebServer& server,
     const char* details) {
    submissionState.finished = true;
    submissionState.result = RuntimeBuildPortalResult::InternalError;
    submissionState.details = details;
    server.send(500, "text/html", buildResultHtml(details));
}

// How one step of a submission ended. The distinction that matters is whether
// the user can do anything about it: Rejected is their problem to fix and the
// portal keeps serving, Failed is the device's and the session ends.
enum class StepOutcome { Ok, Rejected, Failed };

// Mirrors ConfigResult in ConfigLoader.h: `details` always points at a string
// literal, so there is no ownership to track.
struct StepResult {
    StepOutcome outcome;
    const char* details;  // nullptr when outcome is Ok
};

StepResult stepOk() { return {StepOutcome::Ok, nullptr}; }
StepResult stepRejected(const char* details) { return {StepOutcome::Rejected, details}; }
StepResult stepFailed(const char* details) { return {StepOutcome::Failed, details}; }

// Turns a step outcome into the HTTP response and session update it implies.
// Returns true when the handler should carry on to the next step.
bool applyStep(WebServer& server, SubmissionState& submissionState, const StepResult& step) {
    switch (step.outcome) {
        case StepOutcome::Ok:
            return true;
        case StepOutcome::Rejected:
            rejectSubmission(server, step.details);
            return false;
        case StepOutcome::Failed:
            abortSubmission(submissionState, server, step.details);
            return false;
    }

    return false;
}

StepResult selectBuildState(WebServer& server, BuildState& outState) {
    // The form posts the same names that are persisted in build_state.json.
    if (!parseBuildState(readTrimmedArg(server, "state").c_str(), outState)) {
        return stepRejected("Invalid activation mode");
    }

    // "Keep current build" persists no build info of its own; it relies on the
    // last concrete state and the build_info payload already on flash. Reject it
    // before saving anything if that fallback is missing or unparseable,
    // otherwise the next boot halts in App::loadConfig() with no way back.
    if (outState == BuildState::USE_CURRENT_BUILD && !hasKeepableBuild()) {
        return stepRejected("No previous build to keep - choose a specific mode");
    }

    return stepOk();
}

// Yields the credentials the device should connect with, saving them first when
// the user supplied new ones. Reusing saved credentials deliberately does not
// rewrite them.
StepResult resolveWifiCredentials(WebServer& server, String& outSsid, String& outPassword) {
    if (server.arg("useSavedWifi") == "1") {
        char savedSsid[MAX_WIFI_SSID_LEN] = {};
        char savedPassword[MAX_WIFI_PASSWORD_LEN] = {};

        if (!loadWifiCredentialsConfig(savedSsid, sizeof(savedSsid), savedPassword, sizeof(savedPassword))) {
            return stepRejected("No saved Wi-Fi credentials found");
        }

        outSsid = savedSsid;
        outPassword = savedPassword;
        return stepOk();
    }

    outSsid = readTrimmedArg(server, "ssid");
    outPassword = server.arg("password");  // See readTrimmedArg: passwords are taken verbatim.

    if (!isLengthValid(outSsid.c_str(), MAX_WIFI_SSID_LEN) ||
        !isLengthValid(outPassword.c_str(), MAX_WIFI_PASSWORD_LEN)) {
        return stepRejected("Invalid Wi-Fi credentials");
    }

    if (!saveWifiCredentialsConfig(outSsid.c_str(), outPassword.c_str())) {
        return stepFailed("Failed to save Wi-Fi config");
    }

    return stepOk();
}

// Writes the mode-specific payload of /build_info.json.
StepResult persistBuildInfo(WebServer& server, BuildState state) {
    switch (state) {
        case BuildState::BUS_BY_STATION: {
            const String stationValue = readTrimmedArg(server, "stationId");
            if (!isLengthValid(stationValue.c_str(), MAX_STATION_ID_LEN)) {
                return stepRejected("Station ID is required");
            }

            return saveBuildInfoStationConfig(stationValue.c_str()) ? stepOk()
                                                                    : stepFailed("Failed to save build info");
        }
        case BuildState::BUS_BY_LINES:
        case BuildState::NY_METRO_BY_STATION: {
            const bool withDirection = state == BuildState::NY_METRO_BY_STATION;
            const LineTargets targets = parseLinePairs(server, withDirection);
            if (targets.error != nullptr) {
                return stepRejected(targets.error);
            }

            // saveBuildInfoLinesConfig takes parallel pointer arrays; build them
            // here so that storage shape stays out of the parser's signature.
            const char* stationPtrs[MAX_RUNTIME_TARGETS] = {};
            const char* linePtrs[MAX_RUNTIME_TARGETS] = {};
            const char* directionPtrs[MAX_RUNTIME_TARGETS] = {};
            for (size_t i = 0; i < targets.count; i++) {
                stationPtrs[i] = targets.stations[i];
                linePtrs[i] = targets.lines[i];
                directionPtrs[i] = targets.directions[i];
            }

            return saveBuildInfoLinesConfig(stationPtrs, linePtrs, targets.count,
                                            withDirection ? directionPtrs : nullptr)
                       ? stepOk()
                       : stepFailed("Failed to save build info");
        }
        case BuildState::USE_CURRENT_BUILD:
            // Leave the existing payload alone so the resolver can reuse the last
            // concrete mode's data. selectBuildState already proved it is usable.
            return stepOk();
    }

    return stepFailed("Unknown build state");
}

StepResult persistSelection(WebServer& server, BuildState state) {
    if (!saveBuildStateConfig(state)) {
        return stepFailed("Failed to save build state");
    }

    if (state != BuildState::USE_CURRENT_BUILD && !saveLastConcreteBuildStateConfig(state)) {
        return stepFailed("Failed to save concrete build state");
    }

    return persistBuildInfo(server, state);
}

// Best-effort join of the user's own network, after the config is already safely
// on flash. Failing here is still a successful provisioning.
void finishWithStaConnect(SubmissionState& submissionState, const String& ssid, const String& password) {
    Serial.printf("[RuntimeBuildPortal] Joining '%s' (AP clients will be dropped)...\n", ssid.c_str());
    const bool connected =
        tryStaConnect(ssid.c_str(), password.c_str(), kStaConnectTimeoutMs);
    Serial.printf("[RuntimeBuildPortal] STA join %s\n", connected ? "succeeded" : "FAILED");

    submissionState.finished = true;
    submissionState.result =
        connected ? RuntimeBuildPortalResult::SaveAndConnectOk : RuntimeBuildPortalResult::SaveOkConnectFailed;
    submissionState.details = connected ? "Config saved. Wi-Fi connected." : "Config saved. Wi-Fi connect failed.";
}

// Dumps every field the client actually sent, by iterating what arrived rather
// than by asking for the names this code expects. That distinction is the point:
// a form that never arrived, one whose fields are named differently than the
// parser looks for, and one that is genuinely invalid all produce the same
// rejection message, and only the raw inventory tells them apart.
//
// The Wi-Fi password is reported as a length, never a value. This log is read off
// a terminal and routinely pasted elsewhere while debugging, so it must not be
// the thing that leaks the user's home network credentials.
void logSubmittedFields(WebServer& server) {
    const int count = server.args();
    Serial.printf("[RuntimeBuildPortal] submit carried %d field(s):\n", count);

    for (int i = 0; i < count; i++) {
        const String name = server.argName(i);
        if (name == "password") {
            Serial.printf("[RuntimeBuildPortal]   %s = <%u chars>\n", name.c_str(),
                          static_cast<unsigned>(server.arg(i).length()));
        } else {
            Serial.printf("[RuntimeBuildPortal]   %s = '%s'\n", name.c_str(), server.arg(i).c_str());
        }
    }
}

void handleSubmit(WebServer& server, SubmissionState& submissionState) {
    // The success path through here used to be entirely silent: a submission that
    // saved correctly and one that never arrived produced the same empty log.
    Serial.printf("[RuntimeBuildPortal] POST /submit from %s\n",
                  server.client().remoteIP().toString().c_str());
    logSubmittedFields(server);

    // Placeholder, not a default: BuildState has no "unset" member
    BuildState selectedState = BuildState::BUS_BY_STATION;
    if (!applyStep(server, submissionState, selectBuildState(server, selectedState))) {
        return;
    }

    String ssid;
    String password;
    if (!applyStep(server, submissionState, resolveWifiCredentials(server, ssid, password))) {
        return;
    }

    if (!applyStep(server, submissionState, persistSelection(server, selectedState))) {
        return;
    }

    // Answer before connecting: tryStaConnect blocks for up to
    // kStaConnectTimeoutMs, long enough for the browser to give up on the request.
    Serial.println("[RuntimeBuildPortal] Config saved, answering client");
    server.send(200, "text/html", buildResultHtml("Config saved. Connecting to Wi-Fi…"));

    // Deliver and close the connection before the radio moves, which sending
    // alone does not do: WebServer::send() writes the body but leaves the socket
    // open until the next handleClient() pass, and that pass never happens here.
    //
    // It has to happen before tryStaConnect, because that switches to WIFI_AP_STA
    // and retunes to the target network's channel, deauthenticating every
    // associated client (see startProvisioningAp). The browser waiting on this
    // very response is one of them, so an unflushed reply is simply lost and the
    // user is shown "site can't be reached" on /submit -- with no way to tell
    // that their configuration was in fact written correctly a moment earlier.
    WiFiClient client = server.client();
    client.flush();
    client.stop();
    delay(kSubmitResponseSettleMs);

    finishWithStaConnect(submissionState, ssid, password);
}

void registerRoutes(WebServer& server, IPAddress apIp, SubmissionState& submissionState) {
    // Serves a literal, touching neither LittleFS nor streamFile. If this answers
    // and "/" does not, then the AP, the lease, the routing and the web server are
    // all proven working and the fault is isolated to the filesystem or to the
    // transfer of a 13 KB body -- a split that otherwise needs a packet capture.
    server.on("/ping", HTTP_GET, [&server]() {
        Serial.printf("[RuntimeBuildPortal] GET /ping from %s\n",
                      server.client().remoteIP().toString().c_str());
        server.send(200, "text/plain", "portal-alive");
    });

    server.on("/", HTTP_GET, [&server]() {
        // Peer address rather than just the path: with two devices associated,
        // the log otherwise cannot say which one asked, and "the phone never got
        // the page" is a different bug from "the phone never asked".
        Serial.printf("[RuntimeBuildPortal] GET / from %s\n",
                      server.client().remoteIP().toString().c_str());
        if (!serveStaticFile(server, "/portal/index.html", "text/html")) {
            // Reachable with the firmware flashed but the LittleFS image not:
            // the AP and the server come up fine and only the page is missing,
            // which from the phone looks the same as the portal being broken.
            Serial.println("[RuntimeBuildPortal] /portal/index.html missing - run 'pio run -t uploadfs'");
            server.send(500, "text/plain", "Missing /portal/index.html");
        }
    });

    // Answering the OS probes with a redirect rather than the success response
    // they expect is what keeps the network in "captive portal" state. On
    // Android in particular, a 204 would mark the network as "has internet" and
    // cancel the WiFi binding, so every later request -- including the portal
    // page itself -- would be routed over cellular and never reach us.
    auto redirectToPortal = [apIp, &server]() {
        // Logged because association and HTTP both have to work and they fail
        // independently. Without this, a phone that joins the AP but whose probe
        // never arrives looks identical on serial to one whose probe arrived and
        // was answered -- and those two point at completely different causes
        // (DNS interception vs. the browser ignoring the redirect).
        Serial.printf("[RuntimeBuildPortal] probe http://%s%s from %s -> redirect\n",
                      server.hostHeader().c_str(), server.uri().c_str(),
                      server.client().remoteIP().toString().c_str());
        server.sendHeader("Location", String("http://") + apIp.toString() + "/", true);
        server.send(302, "text/plain", "");
    };

    for (const char* probePath : {
             "/generate_204",         // Android
             "/hotspot-detect.html",  // iOS
             "/fwlink",               // Windows and older clients
         }) {
        server.on(probePath, HTTP_GET, redirectToPortal);
    }

    // Anything else the client asks for lands on the portal page too.
    server.onNotFound(redirectToPortal);

    server.on("/wifi-status", HTTP_GET, [&server]() {
        char savedSsid[MAX_WIFI_SSID_LEN] = {};
        char savedPassword[MAX_WIFI_PASSWORD_LEN] = {};

        // Whether credentials exist is all the form needs in order to offer the
        // "use saved network" toggle. The SSID itself is withheld: this endpoint
        // is unauthenticated, and a home network name is a strong location hint.
        const bool hasSaved =
            loadWifiCredentialsConfig(savedSsid, sizeof(savedSsid), savedPassword, sizeof(savedPassword));
        server.send(200, "application/json", hasSaved ? "{\"hasSaved\":true}" : "{\"hasSaved\":false}");
    });

    server.on("/submit", HTTP_POST, [&server, &submissionState]() { handleSubmit(server, submissionState); });
}

// Names the 802.11 reason codes that actually distinguish causes here; anything
// else is printed numerically rather than guessed at.
const char* apDisconnectReasonName(uint16_t reason) {
    switch (reason) {
        case WIFI_REASON_AUTH_EXPIRE:    return "AUTH_EXPIRE";
        case WIFI_REASON_AUTH_LEAVE:     return "AUTH_LEAVE";
        case WIFI_REASON_ASSOC_EXPIRE:   return "ASSOC_EXPIRE - we aged it out for inactivity";
        case WIFI_REASON_ASSOC_TOOMANY:  return "ASSOC_TOOMANY - AP connection limit";
        case WIFI_REASON_NOT_AUTHED:     return "NOT_AUTHED";
        case WIFI_REASON_NOT_ASSOCED:    return "NOT_ASSOCED";
        case WIFI_REASON_ASSOC_LEAVE:    return "ASSOC_LEAVE - the client chose to leave";
        case WIFI_REASON_BEACON_TIMEOUT: return "BEACON_TIMEOUT - client stopped hearing us";
        default:                         return "unlisted 802.11 reason";
    }
}

// Reports why a station left, which the client table cannot: from there a phone
// that walked away, one we aged out, and one that lost our beacons all look
// identical -- the count simply drops to zero. The reason code separates "the
// client decided to leave" (its policy, nothing here to fix) from "we dropped it"
// (ours), and those two lead to opposite investigations.
//
// Defined above RadioRestore because that guard unregisters it.
void onApStationDisconnected(arduino_event_id_t event, arduino_event_info_t info) {
    const wifi_event_ap_stadisconnected_t& station = info.wifi_ap_stadisconnected;
    Serial.printf("[RuntimeBuildPortal] station %02X:%02X:%02X:%02X:%02X:%02X left, reason=%u (%s)\n",
                  station.mac[0], station.mac[1], station.mac[2], station.mac[3], station.mac[4],
                  station.mac[5], station.reason, apDisconnectReasonName(station.reason));
}

// Puts the radio back the way the rest of the firmware expects to find it:
// station mode, no access point, auto-reconnect on. Doing this in a destructor
// rather than at each return is what makes the early-error paths correct -- they
// previously restored three different subsets of the state, and two of them left
// the chip in WIFI_AP_STA, recovered only by App::connectWifi() happening to set
// the mode again.
struct RadioRestore {
    // Set by the caller once the disconnect handler is registered. Removal goes
    // through the handle rather than the function pointer: the pointer overload
    // is deprecated in arduino-esp32 3.x and cannot match a functional callback.
    network_event_handle_t stationEventHandle = 0;

    ~RadioRestore() {
        // Before the radio goes down: the handler is portal-scoped and would
        // otherwise keep logging against the rest of the firmware's Wi-Fi use.
        if (stationEventHandle != 0) {
            WiFi.removeEvent(stationEventHandle);
        }
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
    }
};

// The URI advertised in DHCP option 114. File scope because esp_netif stores the
// pointer rather than copying the string -- "set operation copies the pointer to
// the URI, so it is owned by the application and needs to be maintained valid
// throughout the entire DHCP Server lifetime" (esp_netif.h). A local buffer here
// would leave the DHCP server reading freed stack for the rest of the session.
char gCaptivePortalUri[32] = {};

// Puts the AP's address and DHCP server into the state the portal needs, working
// on the netif directly.
//
// One stop -> configure -> start cycle, deliberately: the lease pool and option
// 114 are both properties of a *stopped* DHCP server, and anything that restarts
// the interface afterwards discards them. Splitting them across calls is what
// produced a server that was running yet had no pool to lease from -- which from
// the client is indistinguishable from an AP that ignores DHCP entirely.
bool configureApNetif(esp_netif_t* apNetif) {
    esp_err_t err = esp_netif_dhcps_stop(apNetif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        Serial.printf("[RuntimeBuildPortal] DHCPS stop failed: %s\n", esp_err_to_name(err));
        return false;
    }

    esp_netif_ip_info_t ipInfo = {};
    ipInfo.ip.addr = static_cast<uint32_t>(kApIp);
    ipInfo.gw.addr = static_cast<uint32_t>(kApGateway);
    ipInfo.netmask.addr = static_cast<uint32_t>(kApSubnet);
    err = esp_netif_set_ip_info(apNetif, &ipInfo);
    if (err != ESP_OK) {
        Serial.printf("[RuntimeBuildPortal] Set AP IP info failed: %s\n", esp_err_to_name(err));
        return false;
    }

    dhcps_lease_t lease = {};
    lease.enable = true;
    lease.start_ip.addr = static_cast<uint32_t>(kApDhcpPoolStart);
    lease.end_ip.addr = static_cast<uint32_t>(kApDhcpPoolEnd);
    err = esp_netif_dhcps_option(apNetif, ESP_NETIF_OP_SET, ESP_NETIF_REQUESTED_IP_ADDRESS,
                                 &lease, sizeof(lease));
    if (err != ESP_OK) {
        Serial.printf("[RuntimeBuildPortal] Set DHCP lease pool failed: %s\n", esp_err_to_name(err));
        return false;
    }

    // RFC 8910 / option 114: state outright that this network is captive and
    // where the portal lives, rather than leaving the client to infer it from
    // hijacked DNS and a redirected probe. Modern Android prefers this path, and
    // it sidesteps the HTTPS/HSTS problems that break redirect interception. It
    // is also why this firmware is on ESP-IDF 5.x at all -- the option does not
    // exist before 5.4.2. See the platform pin in platformio.ini.
    //
    // Non-fatal: the wildcard DNS server and the probe redirects registered in
    // registerRoutes() stay as the fallback for clients that ignore it.
    snprintf(gCaptivePortalUri, sizeof(gCaptivePortalUri), "http://%s", kApIp.toString().c_str());
    err = esp_netif_dhcps_option(apNetif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI,
                                 gCaptivePortalUri, strlen(gCaptivePortalUri));
    if (err == ESP_OK) {
        Serial.printf("[RuntimeBuildPortal] DHCP captive portal URI: %s\n", gCaptivePortalUri);
    } else {
        Serial.printf("[RuntimeBuildPortal] Captive portal option rejected (%s) - DNS redirect only\n",
                      esp_err_to_name(err));
    }

    err = esp_netif_dhcps_start(apNetif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        Serial.printf("[RuntimeBuildPortal] DHCPS start failed: %s\n", esp_err_to_name(err));
        return false;
    }

    // Confirm rather than assume. A server that reports a successful start and
    // then is not in STARTED state is precisely the silent failure this function
    // exists to rule out, and it costs one call to catch it here instead of
    // inferring it later from a client stuck at 0.0.0.0.
    esp_netif_dhcp_status_t dhcpStatus = ESP_NETIF_DHCP_INIT;
    if (esp_netif_dhcps_get_status(apNetif, &dhcpStatus) != ESP_OK ||
        dhcpStatus != ESP_NETIF_DHCP_STARTED) {
        Serial.printf("[RuntimeBuildPortal] DHCP server not running after start (status %d)\n",
                      static_cast<int>(dhcpStatus));
        return false;
    }

    Serial.printf("[RuntimeBuildPortal] DHCP server started, pool %s - %s\n",
                  kApDhcpPoolStart.toString().c_str(), kApDhcpPoolEnd.toString().c_str());
    return true;
}

// Brings up the provisioning AP and waits for its address to settle.
// Cleanup on failure is the caller's RadioRestore guard.
bool startProvisioningAp(IPAddress& outIp) {
    // Ensure a clean WiFi state before starting the AP: stop any background
    // reconnect attempts that would fight over the radio channel. persistent(false)
    // is part of that -- the SDK otherwise keeps the last SSID in NVS and the STA
    // interface can retry it on its own, with no call of ours to point at.
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false);
    delay(kRadioDisconnectSettleMs);

    // Pure AP, not AP_STA. There is one radio: any STA scan or connect attempt
    // retunes it to the target network's channel and takes the AP with it, which
    // deauthenticates every associated client. The phone keeps showing the network
    // as connected while its socket is dead, so the portal simply stops answering
    // on 192.168.4.1. Nothing here needs STA -- tryStaConnect enables it later,
    // after the config is saved and the response is already sent.
    WiFi.mode(WIFI_AP);
    delay(kModeChangeSettleMs);

    // Open, deliberately. The WPA2 passphrase this AP used to carry was derived
    // from the softAP MAC, and that MAC is broadcast as the BSSID in every beacon
    // frame -- anyone in range could recompute it. It gated nothing, while costing
    // the user a 9-character transcription off a LED panel inside the join window.
    //
    // The tradeoff this accepts: the user's Wi-Fi password now crosses the air in
    // the clear (plain HTTP over an open AP). Bounded by kNoClientTimeoutMs and
    // kMaxSessionMs, and the portal hands nothing back -- see /wifi-status, which
    // reports only whether credentials exist and withholds the SSID itself.
    if (!WiFi.softAP(kApSsid)) {
        Serial.println("[RuntimeBuildPortal] Failed to start AP");
        return false;
    }

    // ESP_NETIF_STARTED_BIT is raised from the WiFi event task, not by softAP()
    // returning, so the netif can still be mid-start here. Everything below
    // operates on that interface, so wait for it rather than racing it.
    const unsigned long startedWait = millis();
    while (!WiFi.AP.started() && millis() - startedWait < kApStartedTimeoutMs) {
        delay(kApIpPollMs);
    }

    // Look the handle up by if-key rather than going through WiFi.softAPConfig().
    // The wrapper caches an esp_netif handle inside WiFi.AP, and softAP()
    // recreates the interface underneath it; by the time softAPConfig() runs, the
    // cached handle no longer reports ESP_NETIF_DHCP_SERVER, so the core's
    // NetworkInterface::config() silently takes its DHCP-*client* branch instead.
    // That branch sets a static address, marks the interface configured and
    // returns true without ever establishing a lease pool -- which is how this AP
    // came to beacon correctly, accept associations, and hand out nothing.
    esp_netif_t* apNetif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (apNetif == nullptr) {
        Serial.println("[RuntimeBuildPortal] AP netif handle not found");
        return false;
    }

    if (!configureApNetif(apNetif)) {
        return false;
    }

    // Read back rather than assuming kApIp took effect, so a silent failure in
    // configureApNetif shows up here instead of as a DNS server pointed at an
    // address the interface does not actually hold.
    esp_netif_ip_info_t assignedIp = {};
    if (esp_netif_get_ip_info(apNetif, &assignedIp) != ESP_OK || assignedIp.ip.addr == 0) {
        Serial.println("[RuntimeBuildPortal] AP IP not readable after configuration");
        return false;
    }
    outIp = IPAddress(assignedIp.ip.addr);

    return true;
}

// Snapshot of the client table as last logged, so the poll below reports changes
// instead of repeating the same lines every tick.
String gLastClientSnapshot;

// Logs each associated client with the address the DHCP server gave it, but only
// when that picture has changed.
//
// Sampling on a timer rather than on association is the whole point. A station
// enters this table the moment it associates, which is *before* it has completed
// DHCP, so the first sample of any new client is almost always 0.0.0.0. Logging
// only on the association edge -- which is what this did originally -- reported
// "no lease" for every client including the ones that got one a second later, and
// emitted no follow-up line to correct it. That turned a normal handshake into
// evidence of a bug that was not there.
//
// A client that genuinely never gets a lease still shows here: it simply stays at
// 0.0.0.0 across polls instead of transitioning to a pool address.
void logApClientsIfChanged() {
    wifi_sta_list_t wifiStations = {};
    if (esp_wifi_ap_get_sta_list(&wifiStations) != ESP_OK) {
        return;
    }

    wifi_sta_mac_ip_list_t netifStations = {};
    if (esp_wifi_ap_get_sta_list_with_ip(&wifiStations, &netifStations) != ESP_OK) {
        return;
    }

    // Built first and compared as a whole, so a client acquiring, changing or
    // losing its address all register as changes, not just a change of count.
    String snapshot;
    for (int i = 0; i < netifStations.num; i++) {
        const esp_netif_pair_mac_ip_t& station = netifStations.sta[i];
        char entry[48];
        snprintf(entry, sizeof(entry), "%02X%02X%02X%02X%02X%02X=%08X;", station.mac[0],
                 station.mac[1], station.mac[2], station.mac[3], station.mac[4],
                 station.mac[5], static_cast<unsigned>(station.ip.addr));
        snapshot += entry;
    }

    if (snapshot == gLastClientSnapshot) {
        return;
    }
    gLastClientSnapshot = snapshot;

    Serial.printf("[RuntimeBuildPortal] AP clients: %d\n", netifStations.num);
    for (int i = 0; i < netifStations.num; i++) {
        const esp_netif_pair_mac_ip_t& station = netifStations.sta[i];
        Serial.printf("[RuntimeBuildPortal]   client %02X:%02X:%02X:%02X:%02X:%02X ip=%s\n",
                      station.mac[0], station.mac[1], station.mac[2], station.mac[3],
                      station.mac[4], station.mac[5], IPAddress(station.ip.addr).toString().c_str());
    }
}

// What the panel says while the portal waits, which depends on how far along the
// user is. Both fit the panel's line width; see PortalMessages.
struct PortalMessages {
    String join;       // Nobody associated yet: how to get onto the AP.
    String connected;  // Associated: where to point the browser.
};

// Services the portal until a submission completes or a timeout expires.
//
// The panel follows the association state: once a client is on the AP the
// passphrase is spent advice, and the address to open is the only thing the user
// still needs. Association is otherwise invisible from outside the device, so
// this doubles as the only confirmation that the AP saw them at all.
void runPortalLoop(WebServer& server, DNSServer& dnsServer,
     SubmissionState& submissionState, PortalNotifyFn notify,
     const PortalMessages& messages) {

    const unsigned long sessionStart = millis();
    unsigned long idleStart = sessionStart;

    // Seeded to 0 to match the join message already on the panel, so the first
    // client to associate registers as a change and repaints.
    uint8_t lastStationCount = 0;

    // Seeded so the first poll happens immediately rather than one interval in.
    unsigned long lastClientPoll = sessionStart - kClientPollIntervalMs;

    while (!submissionState.finished) {
        // A no-op on arduino-esp32 3.x -- DNSServer was rewritten onto AsyncUDP
        // and answers from its own callback, so queries are served whether or not
        // this loop runs. Kept because the call is free and its absence would
        // read as "DNS is not being serviced", which is the opposite of the truth.
        dnsServer.processNextRequest();
        server.handleClient();

        // Polled rather than driven off the association edge, so a client's
        // address is reported when DHCP completes instead of only at the instant
        // it associates -- which is always too early to have one.
        if (millis() - lastClientPoll >= kClientPollIntervalMs) {
            lastClientPoll = millis();
            logApClientsIfChanged();
        }

        const uint8_t stationCount = WiFi.softAPgetStationNum();
        if (stationCount != lastStationCount) {
            // Repaint on the 0 <-> non-zero edge only. A second phone joining
            // does not change what the panel should say, and redrawing on every
            // count change would flicker the message while devices come and go.
            const bool hadClient = lastStationCount > 0;
            const bool hasClient = stationCount > 0;
            if (hadClient != hasClient && notify) {
                notify(hasClient ? messages.connected.c_str() : messages.join.c_str());
            }

            lastStationCount = stationCount;
        }

        if (millis() - sessionStart >= kMaxSessionMs) {
            submissionState.finished = true;
            submissionState.result = RuntimeBuildPortalResult::TimeoutNoSubmit;
            submissionState.details = "Timed out waiting for a submission";
            break;
        }

        if (stationCount > 0) {
            idleStart = millis();
        } else if (millis() - idleStart >= kNoClientTimeoutMs) {
            submissionState.finished = true;
            submissionState.result = RuntimeBuildPortalResult::TimeoutNoClient;
            submissionState.details = "Timed out waiting for AP client";
            break;
        }

        delay(kPortalLoopTickMs);
    }
}

} // namespace

RuntimeBuildPortalResult runRuntimeBuildPortal(PortalNotifyFn notify) {
    DNSServer dnsServer;
    WebServer server(kHttpPort);
    SubmissionState submissionState = {false, RuntimeBuildPortalResult::InternalError, "Internal error"};

    if (!LittleFS.begin(false)) {
        Serial.println("[RuntimeBuildPortal] LittleFS mount failed");
        return RuntimeBuildPortalResult::InternalError;
    }

    // Proves what is on flash before any client can muddy the picture.
    logPortalAssets();

    IPAddress apIp;
    RadioRestore radioRestore;  // Must outlive every return below.

    // Registered before the AP exists so the first disconnect is already covered.
    radioRestore.stationEventHandle =
        WiFi.onEvent(onApStationDisconnected, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);

    if (!startProvisioningAp(apIp)) {
        return RuntimeBuildPortalResult::InternalError;
    }

    // Unchecked until now: without DNS the OS captive-portal probes resolve
    // nowhere, so the sign-in sheet never appears and the only way in is typing
    // the address by hand -- a distinct failure from the web server being down,
    // and worth being able to tell apart in the log.
    // Logged either way. Success used to be silent, which meant a session with no
    // HTTP traffic gave no way to tell "DNS never started" from "DNS answered and
    // the client ignored it" -- the two have nothing in common except the silence.
    if (dnsServer.start(kDnsPort, "*", apIp)) {
        Serial.printf("[RuntimeBuildPortal] DNS server listening on :%u, resolving * -> %s\n",
                      kDnsPort, apIp.toString().c_str());
    } else {
        Serial.println("[RuntimeBuildPortal] DNS server failed to start - no captive portal redirect");
    }

    registerRoutes(server, apIp, submissionState);

    server.begin();
    Serial.printf("[RuntimeBuildPortal] AP started (open). SSID: %s IP: %s/%d Free heap: %u\n",
                  kApSsid, apIp.toString().c_str(), WiFi.softAPSubnetCIDR(),
                  ESP.getFreeHeap());

    // The panel is the only channel to whoever is holding the board, and the only
    // signal that the device is in setup mode at all. Still split across lines:
    // the SSID alone is nearly the panel's full width, so a run-on string clips.
    //
    // The address is built from apIp rather than written out, so it stays right
    // if the AP is ever moved off the 192.168.4.1 default.
    const PortalMessages messages = {
        String("Join WiFi\n") + kApSsid,
        String("Connected\nOpen in browser\n") + apIp.toString(),
    };

    if (notify) {
        notify(messages.join.c_str());
    }

    runPortalLoop(server, dnsServer, submissionState, notify, messages);

    server.stop();
    dnsServer.stop();

    Serial.printf("[RuntimeBuildPortal] Exit: %d (%s)\n", static_cast<int>(submissionState.result), submissionState.details);
    return submissionState.result;
}
