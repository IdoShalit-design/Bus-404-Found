#include <Arduino.h>
#include "NetworkManager.h"


#define MY_WIFI_SSID "Littlebluedoor"
#define MY_WIFI_PASSWORD "Mennashe"


const char* ssid = (const char*) MY_WIFI_SSID;
const char* password = (const char*) MY_WIFI_PASSWORD;
NetworkManager* network_manager = nullptr;


void setup() {
  // Initialize serial for output
  Serial.begin(115200);

  //wifi connection init
  network_manager = new NetworkManager(ssid, password);
  network_manager ->print_networks();
  bool conected = network_manager->connect_to_wifi();
  if (conected){
    Serial.printf("connected to %s\n", ssid);
  }
  else{
    Serial.printf("failed to connect to to %s\n", ssid);
  }
  
  network_manager->print_wifi_status();
  
}


void loop() {
}

