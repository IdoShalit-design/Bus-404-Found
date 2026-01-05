#include <Arduino.h>
#include "Network/NetworkManager.h"
#include "Config.h"
#include "TimeManager.h"

// Global pointer to the NetworkManager instance
NetworkManager* network_manager = nullptr;
TimeManager time_manager(TIME_ZONE);

void setup() {
  // Initialize serial for output
  Serial.begin(115200);

  // 1. Create a credentials object from the Config defines
  WifiCredentials credentials(WIFI_SSID, WIFI_PASS);

  // 2. Initialize the NetworkManager using the 'new' keyword as requested
  // We pass the credentials object to the constructor
  network_manager = new NetworkManager(credentials);

  // Scan and print available networks
  network_manager->print_networks();

  // Attempt to connect to WiFi
  bool connected = network_manager->connect_to_wifi();

  // Output connection result
  if (connected) {
    Serial.printf("Successfully connected to %s\n", WIFI_SSID);
  } else {
    Serial.printf("Failed to connect to %s\n", WIFI_SSID);
  }
  
  // Print detailed connection status
  network_manager->print_wifi_status();


  // syncronize clock
  time_manager.init_and_sync();
  Serial.println("The time now is:");
  Serial.println(time_manager.get_formatted_time());

  Serial.println(time_manager.get_minutes_until("22:10"));


  

  

}

void loop() {
  // Main execution loop
}