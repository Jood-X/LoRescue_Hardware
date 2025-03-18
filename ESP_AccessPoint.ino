#include <WiFi.h>
#include <WebServer.h>

// Access Point credentials
const char* ssid = "ESP32-Network";  // Name of ESP32 Wi-Fi
const char* password = "12345678";   // Password for ESP32 Wi-Fi

// Create a web server on port 80
WebServer server(80);

// HTML Response for root ("/")
void handleRoot() {
  server.send(200, "text/html", "<h1>ESP32 is working!</h1><p>Try <a href='/test'>/test</a> or send a POST request.</p>");
}

// Route to test GET request at "/test"
void handleTest() {
  server.send(200, "text/plain", "ESP32 test successful!");
}

// Route to handle POST request at "/send"
void handlePost() {
  if (server.hasArg("plain")) {
    String message = server.arg("plain");  // Get message from POST body
    Serial.println("Received message: " + message);
    server.send(200, "text/plain", "Message received: " + message);
  } else {
    server.send(400, "text/plain", "Bad Request: No message received");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting ESP32 Access Point...");

  // Start the ESP32 as an Access Point
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started!");

  // Print the IP address (default is 192.168.4.1)
  Serial.print("ESP32 Access Point IP: ");
  Serial.println(WiFi.softAPIP());

  // Define routes for the web server
  server.on("/", handleRoot);        // Root page
  server.on("/test", handleTest);    // GET request to /test
  server.on("/send", HTTP_POST, handlePost);  // POST request to /send

  // Start the web server
  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();  // Handle incoming client requests
}
