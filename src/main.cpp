#include <Arduino.h>
#include "NetworkManager.h"



#define MY_WIFI_SSID "Littlebluedoor"
#define MY_WIFI_PASSWORD "Mennashe"

// Print all nearby Wi-Fi networks (SSID, RSSI, encryption, channel)

const char* ssid = (const char*) MY_WIFI_SSID;
const char* password = (const char*) MY_WIFI_PASSWORD;


void setup() {

  // Initialize serial for output
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  
  // Scan for nearby networks before attempting connection
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
    wifi_auth_mode_t auth = WiFi.encryptionType(i);
    if (auth == WIFI_AUTH_OPEN) Serial.print("Open"); else Serial.print("Encrypted");
    Serial.print(" Channel:");
    Serial.println(WiFi.channel(i));
    delay(10);
  }
  Serial.println("-----------------------");

  //connect to wifi
  Serial.println("Starting WiFi scan...");
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();
  
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    printWiFiStatus();
  } else {
    Serial.println("\nConnected successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.printf("Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());
  }
}


void loop() {
  // Perform a blocking scan (synchronous)
  if (WiFi.status() == WL_CONNECTED) {
  } else {
    printWiFiStatus;
    WiFi.begin(ssid, password);
    delay(5000);
  }
}

