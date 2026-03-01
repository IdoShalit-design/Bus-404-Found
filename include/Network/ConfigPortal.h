#ifndef CONFIG_PORTAL_H
#define CONFIG_PORTAL_H

#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include "NVSManager.h"

#define AP_SSID          "Bus-404-Setup"
#define MDNS_HOSTNAME    "bus"
#define SETUP_TIMEOUT_SEC 10
#define DNS_PORT          53
#define WEB_SERVER_PORT          80
#define AP_STABILIZE_DELAY_MS   100
#define AP_CHECK_INTERVAL_MS    100

/**
 * @brief Handles the configuration portal: AP mode, captive portal,
 *        web server, and mDNS for the Bus-404-Found project.
 *
 * In AP mode, a captive portal redirects all DNS to the config page.
 * In STA mode, the web server + mDNS (bus.local) remain active so
 * the user can update settings anytime.
 */
class ConfigPortal {
public:
    ConfigPortal(NVSManager& nvs);

    /** @brief Start the WiFi Access Point ("Bus-404-Setup"). */
    void startAP();

    /** @brief Stop the Access Point and DNS server. */
    void stopAP();

    /** @brief Check if any client has connected to the AP. */
    bool hasClientConnected();

    /** @brief Start the HTTP web server (works in both AP and STA modes). */
    void startWebServer();

    /** @brief Must be called in loop() to process web/DNS requests. */
    void handleClient();

    /** @brief Start mDNS so device is reachable at http://bus.local */
    void startMDNS();

    /** @brief Returns true if currently in AP mode. */
    bool isAPMode() const;

private:
    NVSManager& _nvs;
    WebServer   _server;
    DNSServer   _dnsServer;
    bool        _apMode;

    void handleRoot();
    void handleSave();
    void handleNotFound();
    String buildPage(const String& statusMsg = "");
    String htmlEscape(const String& text);
};

#endif // CONFIG_PORTAL_H
