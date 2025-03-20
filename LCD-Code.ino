#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Display settings
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // I2C address for the OLED display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
    Serial.begin(115200);

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
    display.setTextSize(5); // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text

    // Display "Hello, World!"
    display.setCursor(0, 0); // Set cursor to top-left corner
    display.print("Hello, World!");
    display.display(); // Send the text to the display
}

void loop() {
    // Nothing to do here
}