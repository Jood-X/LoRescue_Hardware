#include <WiFi.h>
#include <WebSocketsServer.h>

const char* ssid = "ESP32_Chat";
const char* password = "12345678";
WebSocketsServer webSocket(81);

void setup() {
    Serial.begin(115200);
    WiFi.softAP(ssid, password);
    Serial.println("WiFi Started!");
    
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("WebSocket Server Started!");
}

void loop() {
    webSocket.loop();
}

// WebSocket event handler
void webSocketEvent(uint8_t clientID, WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            Serial.print("New Client Connected: ");
            Serial.println(clientID);
            webSocket.sendTXT(clientID, "Welcome to ESP32 WebSocket!");
            break;
        
        case WStype_TEXT:
            Serial.print("Received message: ");
            Serial.println((char*)payload);
            webSocket.broadcastTXT((char*)payload);
            break;

        case WStype_DISCONNECTED:
            Serial.print("Client Disconnected: ");
            Serial.println(clientID);
            break;
    }
}

