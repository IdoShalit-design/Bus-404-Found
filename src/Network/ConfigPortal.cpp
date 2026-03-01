/**
 * @file ConfigPortal.cpp
 * @brief Implementation of the configuration portal for Bus-404-Found.
 *
 * Provides a web-based UI for configuring WiFi credentials and bus display
 * settings. Works in both AP mode (captive portal) and STA mode (mDNS).
 */

#include "Network/ConfigPortal.h"

// =========================================
// HTML Page Template (stored in flash)
// =========================================

static const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Bus-404-Found Setup</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: 'Segoe UI', Tahoma, Arial, sans-serif;
            background: #1a1a2e; color: #eee;
            min-height: 100vh; display: flex;
            justify-content: center; align-items: flex-start;
            padding: 20px;
        }
        .container { max-width: 480px; width: 100%; }
        .logo { text-align: center; padding: 20px 0; }
        .logo-placeholder {
            width: 80px; height: 80px; margin: 0 auto 12px;
            background: #16213e; border-radius: 16px;
            display: flex; align-items: center; justify-content: center;
            font-size: 2.5em;
        }
        .logo h1 { font-size: 1.6em; color: #e94560; }
        .logo p { color: #888; font-size: 0.85em; margin-top: 4px; }
        .card {
            background: #16213e; border-radius: 12px;
            padding: 24px; margin-bottom: 16px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.3);
        }
        .card h2 {
            color: #e94560; margin-bottom: 16px;
            font-size: 1.15em;
            border-bottom: 1px solid #333; padding-bottom: 8px;
        }
        label { display: block; margin-bottom: 4px; color: #aaa; font-size: 0.9em; }
        input[type=text], input[type=password] {
            width: 100%; padding: 10px 12px; margin-bottom: 14px;
            border: 1px solid #333; border-radius: 8px;
            background: #0f3460; color: #eee; font-size: 1em;
        }
        input:focus { outline: none; border-color: #e94560; }
        .btn {
            width: 100%; padding: 14px; border: none; border-radius: 8px;
            background: #e94560; color: #fff; font-size: 1.1em;
            cursor: pointer; font-weight: bold; margin-top: 4px;
        }
        .btn:hover { background: #c73652; }
        .status {
            text-align: center; padding: 12px; margin-top: 8px;
            border-radius: 8px; font-size: 0.95em;
        }
        .status.ok { background: #1b4332; color: #95d5b2; }
        .status.err { background: #3d0000; color: #ff6b6b; }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">
            <div class="logo-placeholder">&#x1F68D;</div>
            <h1>Bus-404-Found</h1>
            <p>ESP32-S3 Bus Arrival Display</p>
        </div>
        <form action="/save" method="POST">
            <div class="card">
                <h2>&#x1F4F6; WiFi Settings</h2>
                <label for="ssid">SSID</label>
                <input type="text" id="ssid" name="ssid" value="{{SSID}}" placeholder="Your WiFi network name">
                <label for="password">Password</label>
                <input type="password" id="password" name="password" value="{{PASSWORD}}" placeholder="Your WiFi password">
            </div>
            <div class="card">
                <h2>&#x1F68F; Bus Display Settings</h2>
                <label for="stopId">Stop ID</label>
                <input type="text" id="stopId" name="stopId" value="{{STOPID}}" placeholder="e.g. 1570">
                <label for="lines">Line Numbers (comma-separated)</label>
                <input type="text" id="lines" name="lines" value="{{LINES}}" placeholder="e.g. 7,19,72">
            </div>
            <button class="btn" type="submit">&#x1F4BE; Save Settings</button>
        </form>
        {{STATUS}}
    </div>
</body>
</html>
)rawliteral";

// =========================================
// Constructor
// =========================================

ConfigPortal::ConfigPortal(NVSManager& nvs)
    : _nvs(nvs), _server(WEB_SERVER_PORT), _apMode(false) {
}

// =========================================
// AP Mode Management
// =========================================

void ConfigPortal::startAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID);
    delay(100); // Allow AP to stabilize

    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[ConfigPortal] AP started: %s (IP: %s)\n", AP_SSID, apIP.toString().c_str());

    // Start DNS server to redirect all domains to our IP (captive portal)
    _dnsServer.start(DNS_PORT, "*", apIP);
    _apMode = true;
}

void ConfigPortal::stopAP() {
    _dnsServer.stop();
    WiFi.softAPdisconnect(true);
    _apMode = false;
    Serial.println("[ConfigPortal] AP stopped");
}

bool ConfigPortal::hasClientConnected() {
    return WiFi.softAPgetStationNum() > 0;
}

// =========================================
// Web Server
// =========================================

void ConfigPortal::startWebServer() {
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/save", HTTP_POST, [this]() { handleSave(); });
    _server.onNotFound([this]() { handleNotFound(); });
    _server.begin();
    Serial.println("[ConfigPortal] Web server started on port 80");
}

void ConfigPortal::handleClient() {
    if (_apMode) {
        _dnsServer.processNextRequest();
    }
    _server.handleClient();
}

// =========================================
// mDNS
// =========================================

void ConfigPortal::startMDNS() {
    if (MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", WEB_SERVER_PORT);
        Serial.printf("[ConfigPortal] mDNS started: http://%s.local\n", MDNS_HOSTNAME);
    } else {
        Serial.println("[ConfigPortal] mDNS failed to start");
    }
}

bool ConfigPortal::isAPMode() const {
    return _apMode;
}

// =========================================
// Request Handlers
// =========================================

void ConfigPortal::handleRoot() {
    _server.send(200, "text/html", buildPage());
}

void ConfigPortal::handleSave() {
    // Read form values
    String ssid   = _server.arg("ssid");
    String pass   = _server.arg("password");
    String stopId = _server.arg("stopId");
    String lines  = _server.arg("lines");

    // Write to NVS
    _nvs.setSSID(ssid);
    _nvs.setPassword(pass);
    _nvs.setStopID(stopId);
    _nvs.setLineNumbers(lines);

    Serial.println("[ConfigPortal] Settings saved to NVS");
    Serial.printf("  SSID: %s, StopID: %s, Lines: %s\n",
                  ssid.c_str(), stopId.c_str(), lines.c_str());

    // Send success page, then restart
    String page = buildPage("<div class='status ok'>Settings saved! Restarting in 3 seconds...</div>"
                            "<script>setTimeout(function(){},3000);</script>");
    _server.send(200, "text/html", page);

    // Give the response time to send before restarting
    delay(3000);
    ESP.restart();
}

void ConfigPortal::handleNotFound() {
    if (_apMode) {
        // Captive portal: redirect all requests to root
        _server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
        _server.send(302, "text/plain", "");
    } else {
        _server.send(404, "text/plain", "Not Found");
    }
}

// =========================================
// Page Builder
// =========================================

String ConfigPortal::buildPage(const String& statusMsg) {
    String page = FPSTR(PAGE_HTML);

    // Replace placeholders with current NVS values
    page.replace("{{SSID}}",     htmlEscape(_nvs.getSSID()));
    page.replace("{{PASSWORD}}", htmlEscape(_nvs.getPassword()));
    page.replace("{{STOPID}}",   htmlEscape(_nvs.getStopID()));
    page.replace("{{LINES}}",    htmlEscape(_nvs.getLineNumbers()));
    page.replace("{{STATUS}}",   statusMsg);

    return page;
}

String ConfigPortal::htmlEscape(const String& text) {
    String escaped = text;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&#39;");
    return escaped;
}
