#include "RuntimeBuild/RuntimeBuildPortal.h"

#include <DNSServer.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "Config.h"
#include "RuntimeBuild/BuildStateNames.h"
#include "RuntimeBuild/ConfigLoader.h"
#include "RuntimeBuild/ConfigManager.h"

namespace {

constexpr const char* kApSsid = "Bus-404-Found-Setup";

// Prefix for the generated WPA2 passphrase; the MAC suffix is appended to it.
constexpr const char* kApPasswordPrefix = "bus";

constexpr uint16_t kHttpPort = 80;
constexpr uint16_t kDnsPort = 53;

// Two separate settle points: after tearing down any prior STA connection, and
// after the AP+STA mode switch. Same duration today, but they are not the same
// wait, so changing one should not silently change the other.
constexpr unsigned long kRadioDisconnectSettleMs = 100UL;
constexpr unsigned long kModeChangeSettleMs = 100UL;

// softAPIP() briefly reports 0.0.0.0 after softAP() returns; how long to wait for
// the real address, and how often to re-check.
constexpr unsigned long kApIpReadyTimeoutMs = 3000UL;
constexpr unsigned long kApIpPollMs = 50UL;

// Yield between service passes so the portal loop does not starve WiFi/system tasks.
constexpr unsigned long kPortalLoopTickMs = 10UL;

// Poll interval while waiting on the STA join.
constexpr unsigned long kStaPollIntervalMs = 200UL;

// Covers the result page's fixed markup plus a typical message, so the String
// does not reallocate while being assembled. Only an optimisation: a longer
// message still grows it.
constexpr size_t kResultHtmlReserveBytes = 512;

// How long to keep the AP up with nobody associated.
constexpr unsigned long kNoClientTimeoutMs = 40000UL;

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

static_assert(sizeof(kLineStationFields) / sizeof(kLineStationFields[0]) == MAX_RUNTIME_TARGETS,
              "Add a station field name per runtime target, here and in data/portal/index.html");
static_assert(sizeof(kLineNumberFields) / sizeof(kLineNumberFields[0]) == MAX_RUNTIME_TARGETS,
              "Add a line field name per runtime target, here and in data/portal/index.html");

struct SubmissionState {
    bool finished;
    RuntimeBuildPortalResult result;
    const char* details;
};

// Builds the AP passphrase from the last three bytes of the interface MAC.
// Deterministic, so the user sees the same passphrase on every boot; unique per
// device, so one leaked passphrase does not open every unit; and derived rather
// than stored, so there is no secret to commit. Must be called after WiFi.mode().
String makeApPassword() {
    uint8_t mac[6] = {};
    WiFi.softAPmacAddress(mac);

    char suffix[7];
    snprintf(suffix, sizeof(suffix), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    return String(kApPasswordPrefix) + suffix;  // 9 chars, above the WPA2 minimum of 8
}

bool tryStaConnect(const char* ssid, const char* password, unsigned long timeoutMs) {
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

bool serveStaticFile(WebServer& server, const char* path, const char* contentType) {
    File file = LittleFS.open(path, "r");
    if (!file) {
        return false;
    }

    server.streamFile(file, contentType);
    file.close();
    return true;
}

// The (stationId, line) pairs a submission asked for, owning its own storage.
// `error` follows the ConfigResult convention in ConfigLoader.h: nullptr on
// success, otherwise a string literal, so there is nothing to free.
struct LineTargets {
    char stations[MAX_RUNTIME_TARGETS][MAX_STATION_ID_LEN];
    char lines[MAX_RUNTIME_TARGETS][MAX_LINE_LEN];
    size_t count;
    const char* error;
};

// Reads up to MAX_RUNTIME_TARGETS (stationId, line) pairs from the form. A pair
// is included only if both fields are filled in; one field on its own is a
// validation error rather than a partially usable target.
LineTargets parseLinePairs(WebServer& server) {
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
        case BuildState::BUS_BY_STATION:
        case BuildState::NY_METRO_BY_STATION: {
            const String stationValue = readTrimmedArg(server, "stationId");
            if (!isLengthValid(stationValue.c_str(), MAX_STATION_ID_LEN)) {
                return stepRejected("Station ID is required");
            }

            return saveBuildInfoStationConfig(stationValue.c_str()) ? stepOk()
                                                                    : stepFailed("Failed to save build info");
        }
        case BuildState::BUS_BY_LINES: {
            const LineTargets targets = parseLinePairs(server);
            if (targets.error != nullptr) {
                return stepRejected(targets.error);
            }

            // saveBuildInfoLinesConfig takes parallel pointer arrays; build them
            // here so that storage shape stays out of the parser's signature.
            const char* stationPtrs[MAX_RUNTIME_TARGETS] = {};
            const char* linePtrs[MAX_RUNTIME_TARGETS] = {};
            for (size_t i = 0; i < targets.count; i++) {
                stationPtrs[i] = targets.stations[i];
                linePtrs[i] = targets.lines[i];
            }

            return saveBuildInfoLinesConfig(stationPtrs, linePtrs, targets.count)
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
    const bool connected =
        tryStaConnect(ssid.c_str(), password.c_str(), kStaConnectTimeoutMs);

    submissionState.finished = true;
    submissionState.result =
        connected ? RuntimeBuildPortalResult::SaveAndConnectOk : RuntimeBuildPortalResult::SaveOkConnectFailed;
    submissionState.details = connected ? "Config saved. Wi-Fi connected." : "Config saved. Wi-Fi connect failed.";
}

void handleSubmit(WebServer& server, SubmissionState& submissionState) {
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
    server.send(200, "text/html", buildResultHtml("Config saved. Connecting to Wi-Fi…"));
    finishWithStaConnect(submissionState, ssid, password);
}

void registerRoutes(WebServer& server, IPAddress apIp, SubmissionState& submissionState) {
    server.on("/", HTTP_GET, [&server]() {
        if (!serveStaticFile(server, "/portal/index.html", "text/html")) {
            server.send(500, "text/plain", "Missing /portal/index.html");
        }
    });

    // Answering the OS probes with a redirect rather than the success response
    // they expect is what keeps the network in "captive portal" state. On
    // Android in particular, a 204 would mark the network as "has internet" and
    // cancel the WiFi binding, so every later request -- including the portal
    // page itself -- would be routed over cellular and never reach us.
    auto redirectToPortal = [apIp, &server]() {
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

// Puts the radio back the way the rest of the firmware expects to find it:
// station mode, no access point, auto-reconnect on. Doing this in a destructor
// rather than at each return is what makes the early-error paths correct -- they
// previously restored three different subsets of the state, and two of them left
// the chip in WIFI_AP_STA, recovered only by App::connectWifi() happening to set
// the mode again.
struct RadioRestore {
    ~RadioRestore() {
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
    }
};

// Brings up the WPA2 provisioning AP and waits for its address to settle.
// Cleanup on failure is the caller's RadioRestore guard.
bool startProvisioningAp(String& outPassword, IPAddress& outIp) {
    // Ensure a clean WiFi state before starting the AP: stop any background
    // reconnect attempts that would fight over the radio channel.
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false);
    delay(kRadioDisconnectSettleMs);

    WiFi.mode(WIFI_AP_STA);
    delay(kModeChangeSettleMs);

    outPassword = makeApPassword();
    if (!WiFi.softAP(kApSsid, outPassword.c_str())) {
        Serial.println("[RuntimeBuildPortal] Failed to start AP");
        return false;
    }

    // softAPIP() can return 0.0.0.0 for a brief window after softAP() returns.
    // Wait until the interface has its real address before handing it to the
    // DNS server, otherwise the wildcard redirect points at 0.0.0.0.
    const unsigned long ipWait = millis();
    while ((outIp = WiFi.softAPIP()) == IPAddress(0, 0, 0, 0) && millis() - ipWait < kApIpReadyTimeoutMs) {
        delay(kApIpPollMs);
    }

    if (outIp == IPAddress(0, 0, 0, 0)) {
        Serial.println("[RuntimeBuildPortal] AP IP not ready after timeout");
        return false;
    }

    return true;
}

// Services the portal until a submission completes or a timeout expires.
void runPortalLoop(WebServer& server, DNSServer& dnsServer,
     SubmissionState& submissionState, PortalNotifyFn notify) {
    
    const unsigned long sessionStart = millis();
    unsigned long idleStart = sessionStart;

    while (!submissionState.finished) {
        dnsServer.processNextRequest();
        server.handleClient();

        if (millis() - sessionStart >= kMaxSessionMs) {
            submissionState.finished = true;
            submissionState.result = RuntimeBuildPortalResult::TimeoutNoSubmit;
            submissionState.details = "Timed out waiting for a submission";
            break;
        }

        if (WiFi.softAPgetStationNum() > 0) {
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

    String apPassword;
    IPAddress apIp;
    RadioRestore radioRestore;  // Must outlive every return below.
    if (!startProvisioningAp(apPassword, apIp)) {
        return RuntimeBuildPortalResult::InternalError;
    }

    dnsServer.start(kDnsPort, "*", apIp);
    registerRoutes(server, apIp, submissionState);

    server.begin();
    Serial.printf("[RuntimeBuildPortal] AP started. SSID: %s Password: %s IP: %s\n", kApSsid,
                  apPassword.c_str(), apIp.toString().c_str());

    
    notify((String("Connect to wifi network: Bus-404-Found-Setup \n PW ") + 
            apPassword).c_str());


    runPortalLoop(server, dnsServer, submissionState, notify);

    server.stop();
    dnsServer.stop();

    Serial.printf("[RuntimeBuildPortal] Exit: %d (%s)\n", static_cast<int>(submissionState.result), submissionState.details);
    return submissionState.result;
}
