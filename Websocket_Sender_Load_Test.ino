#include <WiFi.h>
#include <WebSocketsServer.h>

const char* ssid = "ESP32_LoadTest";
const char* password = "12345678";

WebSocketsServer webSocket(81);

unsigned long lastSendTime = 0;
unsigned long sendInterval = 100; // Send message every 100 ms
unsigned long messageCount = 0;

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  Serial.println("📡 ESP32 AP Started");
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("🧪 WebSocket Server for Load Testing Started");
}

void loop() {
  webSocket.loop();

  // Send a test message to all connected clients at the defined interval
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();

    String testMessage = "LoadTest Message #" + String(messageCount++);
    webSocket.broadcastTXT(testMessage);
    Serial.println("📤 Sent: " + testMessage);
  }
}

// WebSocket event handler
void webSocketEvent(uint8_t clientID, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("🟢 Client %u connected\n", clientID);
      webSocket.sendTXT(clientID, "👋 Welcome to the Load Test Server!");
      break;

    case WStype_TEXT:
      Serial.printf("📥 Received from %u: %s\n", clientID, payload);
      break;

    case WStype_DISCONNECTED:
      Serial.printf("🔴 Client %u disconnected\n", clientID);
      break;
  }
}
