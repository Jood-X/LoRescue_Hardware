#include <WiFi.h>

const char* ssid = "T-Beam_AP"; // Name of the Wi-Fi network
const char* password = "password"; // Password for the network

void setup() {
  Serial.begin(115200);

  // Set up the ESP32 as an Access Point
  WiFi.softAP(ssid, password);

  // Print the IP address of the AP
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
}

void loop() {
  // Your main code here
}