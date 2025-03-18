#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

const char* ssid = "T-Beam_AP";
const char* password = "Test";

WiFiServer server(80); // HTTP server on port 80

void setup() {
    Serial.begin(115200);
    // Set up the ESP32 as an Access Point
    WiFi.softAP(ssid, password);
    Serial.print("Connecting to WiFi");
    
    // Print the IP address of the AP
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    server.begin(); // Start the server
}

void loop() {
    
    WiFiClient client = server.available(); // Check for incoming clients
    
    if (client) {
        Serial.println("New Client Connected!");
        
        String currentLine = "";
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                Serial.write(c); // Print received data
                
                if (c == '\n') {
                    if (currentLine.length() == 0) {
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println();
                        client.println("ESP32 received your message!");
                        client.println();
                        break;
                    } else {
                        currentLine = "";
                    }
                } else if (c != '\r') {
                    currentLine += c;
                }
            }
        }
        client.stop();
        Serial.println("Client Disconnected.");
    }
}
