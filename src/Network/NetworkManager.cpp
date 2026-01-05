#include "Network/NetworkManager.h"

NetworkManager::NetworkManager(const WifiCredentials& creds) 
: _credentials(creds) {
}

bool NetworkManager::connect_to_wifi() {
    WiFi.mode(WIFI_STA);
    
    Serial.println("Starting WiFi Connection...");
    

    WiFi.begin(_credentials.ssid, _credentials.password);
    
    unsigned long startAttemptTime = millis();
    
    // connection attempt for 20 sec
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nConnection Failed.");
        print_wifi_status();
        return false;
    } else {
        Serial.println("\nConnected successfully!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.printf("Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());
        return true;
    }
}

void NetworkManager::print_networks() {
    int n = WiFi.scanNetworks();
    Serial.print("Scan done. ");
    Serial.print(n);
    Serial.println(" networks found");

    for (int i = 0; i < n; ++i) {
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(WiFi.SSID(i));
        Serial.print(" (");
        Serial.print(WiFi.RSSI(i));
        Serial.print(" dBm) ");
        Serial.println(WiFi.channel(i));
    }
    Serial.println("-----------------------");
}

void NetworkManager::print_wifi_status() {
    switch (WiFi.status()) {
        case WL_IDLE_STATUS:     Serial.println("Status: IDLE (Changing states)"); break;
        case WL_NO_SSID_AVAIL:   Serial.println("Status: SSID NOT FOUND (Check your wifi_name)"); break;
        case WL_SCAN_COMPLETED:  Serial.println("Status: SCAN COMPLETED"); break;
        case WL_CONNECTED:       Serial.println("Status: CONNECTED"); break;
        case WL_CONNECT_FAILED:  Serial.println("Status: CONNECTION FAILED (Likely WRONG PASSWORD)"); break;
        case WL_CONNECTION_LOST: Serial.println("Status: CONNECTION LOST"); break;
        case WL_DISCONNECTED:    Serial.println("Status: DISCONNECTED"); break;
        default:                 Serial.println("Status: UNKNOWN"); break;
    }
}