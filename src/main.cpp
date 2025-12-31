#include <Arduino.h>
#include <WiFi.h>

const char* ssid = "LittleBlueDoor";
const char* password = "Mennashe";

void setup() {  
  Serial.begin(115200);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, password);
Serial.print("Connecting to WiFi...");

while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    int status = WiFi.status();
    
    if (status == WL_NO_SSID_AVAIL) {
      Serial.println("Network not found. Check SSID.");
    } else if (status == WL_CONNECT_FAILED) {
      Serial.println("Password wrong or connection failed.");
    } else if (status == WL_IDLE_STATUS) {
      Serial.println("WiFi changing status...");
    } else {
      Serial.print("Status: ");
      Serial.println(status);
    }
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}

void loop() {
}