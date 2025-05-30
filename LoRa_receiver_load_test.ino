#include <SPI.h>
#include <LoRa.h>

// ==== LoRa T-Beam Pins ====
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_SS 18
#define LORA_RST 23
#define LORA_DIO0 26

// ==== Setup ====
void setup() {
  Serial.begin(115200);
  // LoRa Init
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(868E6)) {
    Serial.println("LoRa init failed!");
    while (true);
  }
  Serial.println("LoRa Ready");
}

// ==== Main Loop ====
void loop() {
  receiveLoraMessage();
}

// ==== Receive LoRa Messages ====
void receiveLoraMessage() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.println("LoRa Packet Received:");
    String received = "";
    
    while (LoRa.available()) {
      char c = (char)LoRa.read();
      received += c;
    }
    Serial.println(); // Newline after raw message
    Serial.println(received);
  }
}
