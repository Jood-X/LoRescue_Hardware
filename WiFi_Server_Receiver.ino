#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// OLED Display settings
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // I2C address for the OLED display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid = "ESP32-Network";  // Wi-Fi SSID
const char* password = "12345678";   // Wi-Fi Password

WebServer server(80);  // Start HTTP server on port 80

// Handle GET request (Check if ESP32 is working)
void handleTest() {
  server.send(200, "text/plain", "ESP32 is running!");
}

// Handle POST request to receive messages from mobile app
void handlePost() {
  if (server.hasArg("plain")) {
    String message = server.arg("plain");  // Get the message from the request body
    Serial.print("Received message: ");
    display.println(message);
      // Show message on Serial Monitor
      display.display();

    // Send a response back to the mobile app
    server.send(200, "text/plain", message+ "... Message received." );
  } else {
    server.send(400, "text/plain", "No message received");
  }
}

void setup() {
  Serial.begin(115200);  // Start Serial Monitor
  WiFi.softAP(ssid, password);  // Start Wi-Fi Access Point
  // Initialize the OLED display
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("SSD1306 allocation failed");
        for (;;); // Halt if the display initialization fails
    }
    Serial.println("OLED display initialized.");

    // Clear the display buffer
    display.clearDisplay();
    display.display();

    // Set text size and color
    display.setTextSize(2); // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text

    // Display "Hello, World!"
    display.setCursor(0, 0); // Set cursor to top-left corner


  Serial.println("ESP32 Access Point Started!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());  // Print ESP32's IP address

  // Set up routes
  server.on("/test", HTTP_GET, handleTest);
  server.on("/send", HTTP_POST, handlePost);

  server.begin();
  Serial.println("HTTP Server started.");

  
}

void loop() {
  server.handleClient();  // Listen for HTTP requests
}

