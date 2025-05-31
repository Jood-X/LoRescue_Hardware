#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <sqlite3.h>
#include "FS.h"
#include "SPIFFS.h"

// ==== LoRa T-Beam Pins ====
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_SS 18
#define LORA_RST 23
#define LORA_DIO0 26

// ==== GPS Config ====
#define GPS_RX 34
#define GPS_TX 12
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
int UTC_OFFSET = 3;  // For Jordan
double latitude = 0;
double longitude = 0;
unsigned long lastGpsSendTime = 0;
const unsigned long gpsInterval = 5000;

// ==== OLED Config ====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ==== Wi-Fi Config ====
const char *ssid = "Lorescue_Zone1";
const char *password = "12345678";
const char *CurrentZoneId = "Zone_1";

// ==== WebSocket ====
WebSocketsServer webSocket(81);

// ==== SQLite ====
sqlite3 *db;
char *zErrMsg = 0;
int rc;
unsigned long lastPrintTime = 0;


// Declarations of functions
void displayMessage(String msg);
void initDatabase();
void storeMessage(String senderID, String content, String type, String channelID, String zoneID, String receiverZone);
void webSocketEvent(uint8_t clientID, WStype_t type, uint8_t *payload, size_t length);
void sendLoraMessage(String Id, String name, String type, String content, String ZoneID, String receiverZoneId) 
void receiveLoraMessage();
static int printCallback(void *NotUsed, int argc, char **argv, char **azColName);
void printAllMessages();

// ==== Display Helper ====
void displayMessage(String msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println(msg);
  display.display();
}

// ==== SQLite Table Init ====
void initDatabase() {
  const char *createMessages = "CREATE TABLE IF NOT EXISTS messages (id INTEGER PRIMARY KEY, senderID TEXT, content TEXT, type TEXT, channelID TEXT, timestamp TEXT, zoneID TEXT,receiverZone TEXT);";
  rc = sqlite3_exec(db, createMessages, NULL, NULL, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.print("Create messages table failed: ");
    Serial.println(zErrMsg);
    sqlite3_free(zErrMsg);
    Serial.print("Error Code: ");
    Serial.println(rc);
  } else {
    Serial.println("✅ Messages table created or already exists.");
  }

  const char *createUsers = "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, national_id TEXT, name TEXT, role TEXT, verified INTEGER DEFAULT 0);";

  rc = sqlite3_exec(db, createUsers, NULL, NULL, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.print("Create users table failed: ");
    Serial.println(zErrMsg);
    sqlite3_free(zErrMsg);
    Serial.print("Error Code: ");
    Serial.println(rc);
  } else {
    Serial.println("✅ Users table created or already exists.");
  }

  const char *createZones = "CREATE TABLE IF NOT EXISTS zones (id INTEGER PRIMARY KEY, name TEXT);";
  rc = sqlite3_exec(db, createZones, NULL, NULL, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.print("Create zones table failed: ");
    Serial.println(zErrMsg);
    sqlite3_free(zErrMsg);
    Serial.print("Error Code: "); 
    Serial.println(rc);
  } else {
    Serial.println("✅ Zones table created or already exists.");
  }
}

// ==== Store Message in DB - Fixed Version ====
void storeMessage(String senderID, String content, String type, String channelID, String zoneID, String receiverZone) {
  content.replace("'", "''");
  senderID.replace("'", "''");
  type.replace("'", "''");
  channelID.replace("'", "''");
  zoneID.replace("'", "''");
  receiverZone.replace("'", "''");

  const char *query = "INSERT INTO messages (senderID, content, type, channelID, timestamp, zoneID, receiverZone) VALUES (?, ?, ?, ?, datetime('now'), ?, ?);";

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
    Serial.print("Failed to prepare statement: ");
    Serial.println(sqlite3_errmsg(db));
    return;
  }

  sqlite3_bind_text(stmt, 1, senderID.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, content.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, channelID.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 5, zoneID.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 6, receiverZone.c_str(), -1, SQLITE_STATIC);

  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    Serial.print("Failed to insert message: ");
    Serial.println(sqlite3_errmsg(db));
  } else {
    Serial.println("✅ Message inserted successfully into DB.");
  }
  sqlite3_finalize(stmt);
}

// ==== Store User in DB ====
void storeUser(String nationalId, String name, String role) {
  // Insert new user
  String insertQuery = String("INSERT INTO users (national_id, name, role) VALUES ('") + nationalId + "', '" + name + "', '" + role + "');";

  rc = sqlite3_exec(db, insertQuery.c_str(), NULL, NULL, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.print("❌ Failed to insert user: ");
    Serial.println(zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    Serial.println("✅ User " + nationalId + " inserted successfully.");
  }
}

static int printCallback(void *NotUsed, int argc, char **argv, char **azColName) {
  String id;
  String name;

  for (int i = 0; i < argc; i++) {
    Serial.print(azColName[i]);
    Serial.print(": ");
    Serial.println(argv[i] ? argv[i] : "NULL");

    if (strcmp(azColName[i], "national_id") == 0 && argv[i]) {
      id = argv[i];
    } else if (strcmp(azColName[i], "name") == 0 && argv[i]) {
      name = argv[i];
    }
  }

  StaticJsonDocument<256> user;
  user["type"] = "GetUsers";
  user["national_id"] = id;
  user["name"] = name;

  String message;
  serializeJson(user, message);
  webSocket.broadcastTXT(message);

  Serial.println("------------------------");
  return 0;
}

void printAllMessages() {
  const char *query = "SELECT * FROM messages;";
  int rc = sqlite3_exec(db, query, printCallback, NULL, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.print("Error selecting messages: ");
    Serial.println(zErrMsg);
    sqlite3_free(zErrMsg);
  }
}

void printAllUsers() {
  const char *query = "SELECT national_id,name FROM users;";
  int rc = sqlite3_exec(db, query, printCallback, NULL, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.print("Error selecting users: ");
    Serial.println(zErrMsg);
    sqlite3_free(zErrMsg);
  }
}
// === License queue struct and vector ===
struct PendingLicense {
  String id;
  String jsonStr;
  unsigned long lastSentAt;
};
std::vector<PendingLicense> licenseQueue;

void sendPendingLicenses() {
  unsigned long now = millis();
  for (auto &entry : licenseQueue) {
    if (now - entry.lastSentAt >= 10000) {
      Serial.println("📡 Resending license_text to admins: " + entry.id);
      webSocket.broadcastTXT(entry.jsonStr);
      entry.lastSentAt = now;
    }
  }
}

struct PendingRejection {
  String userId;
  unsigned long rejectAfter;
  bool sent = false;
};
std::vector<PendingRejection> rejectQueue;

void checkRejectionQueue() {
  unsigned long now = millis();
  for (int i = rejectQueue.size() - 1; i >= 0; --i) {
    if (!rejectQueue[i].sent && now >= rejectQueue[i].rejectAfter) {
      StaticJsonDocument<200> doc;
      doc["type"] = "verify";
      doc["id"] = rejectQueue[i].userId;
      doc["status"] = "rejected";
      String out;
      serializeJson(doc, out);
      webSocket.broadcastTXT(out);

      Serial.println("📤 Sent delayed rejection for: " + rejectQueue[i].userId);
      rejectQueue[i].sent = true;
      rejectQueue.erase(rejectQueue.begin() + i);
    }
  }
}

struct PendingApproval {
  String userId;
  unsigned long nextSendAt;
  bool sent = false;
};
std::vector<PendingApproval> approvalQueue;

void checkApprovalQueue() {
  unsigned long now = millis();
  for (int i = approvalQueue.size() - 1; i >= 0; --i) {
    if (!approvalQueue[i].sent && now >= approvalQueue[i].nextSendAt) {
      StaticJsonDocument<200> doc;
      doc["type"] = "verify";
      doc["id"] = approvalQueue[i].userId;
      doc["status"] = "approved";
      String out;
      serializeJson(doc, out);
      webSocket.broadcastTXT(out);

      Serial.println("📤 Sent delayed approval for: " + approvalQueue[i].userId);
      approvalQueue[i].sent = true;
      approvalQueue.erase(approvalQueue.begin() + i);
    }
  }
}
std::vector<String> blockedUsers;

struct Channel {
  String name;
  String type;
};

std::vector<Channel> channels;

// ==== Handle WebSocket Messages ====
void webSocketEvent(uint8_t clientID, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      {
        Serial.println("✅ New Client Connected: " + String(clientID));
        break;
      }
    case WStype_TEXT:
      {
        String raw = String((char *)payload);
        Serial.println("🧾 RAW payload:");
        Serial.println(raw);
        Serial.print("📥 Payload length (bytes): ");
        Serial.println(length);

        StaticJsonDocument<16384> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.print("❌ JSON Parse Failed: ");
          Serial.println(error.c_str());
          return;
        }
        String msgType = doc["type"].as<String>();
        Serial.println("📨 Received type: " + msgType);
        String Name, Role;
        String NationalID;
        String senderId, UserName, content, channelId, ZoneID, ReceiverZone;
        String location;
        if (msgType.equalsIgnoreCase("register")) {
          Name = doc["name"] | "";
          NationalID = doc["national_id"].as<String>();
          Role = doc["role"] | "";
          storeUser(NationalID, Name, Role);

          StaticJsonDocument<256> user;
          user["name"] = Name;
          user["national_id"] = doc["national_id"];
          user["role"] = Role;
          user["type"] = "NewUser";
          user["zoneID"] = CurrentZoneId;
          String message;
          serializeJson(user, message);
          webSocket.broadcastTXT(message);
          Serial.println("Broadcasting" + message);
        } else if (msgType == "Chat" || msgType == "SOS") {
          senderId = doc["senderID"] | "";
          UserName = doc["username"] | "";
          content = doc["content"] | "";
          channelId = doc["channelID"] | "";
          ZoneID = doc["zoneId"] | "";
          ReceiverZone = doc["receiverZone"] | "";
          location = String(doc["location"].as<const char *>());

          storeMessage(senderId, content, msgType, channelId, ZoneID, ReceiverZone);

          if (ReceiverZone == String(CurrentZoneId)) {
            webSocket.broadcastTXT(payload);
            Serial.println("📡 Broadcasting to local zone");
          } else {
            sendLoraMessage(senderId, UserName, msgType, content, ZoneID, ReceiverZone);
            Serial.println("📡 Forwarding via LoRa");
          }
        } else if (msgType == "Alert") {
          content = doc["content"] | "";
          senderId = String(doc["senderID"].as<const char *>());
          ZoneID = doc["zoneID"] | "";
          ReceiverZone = doc["receiver"] | "";

          if (ReceiverZone == String(CurrentZoneId)) {
            webSocket.broadcastTXT(payload);
            Serial.println("📢 Broadcasting Local alert");
          } else {
            sendLoraMessage(senderId, "EMR", msgType, content, ZoneID, ReceiverZone);
            Serial.println("📢 Alert sent via LoRa");
          }
        } else if (msgType == "license_text") {
          String id = doc["senderID"] | "";
          String name = doc["username"] | "";
          String role = doc["role"] | "";
          String desc = doc["description"] | "";
          Serial.println("📥 Text verification from: " + id + " - " + name);
          String outgoing;
          serializeJson(doc, outgoing);
          bool exists = false;
          for (auto &entry : licenseQueue) {
            if (entry.id == id) {
              exists = true;
              break;
            }
          }
          if (!exists) {
            licenseQueue.push_back({ id, outgoing, 0 });
            Serial.println("⏳ License stored to be sent every 10 seconds.");
          }
        } else if (msgType.equalsIgnoreCase("verify")) {
          String userId = doc["id"] | "";
          String status = doc["status"] | "";
          Serial.println("✅ Verification: " + userId + " → " + status);
          for (int i = licenseQueue.size() - 1; i >= 0; --i) {
            if (licenseQueue[i].id == userId) {
              licenseQueue.erase(licenseQueue.begin() + i);
              Serial.println("🗑️ Removed from license resend queue.");
              break;
            }
          }
          if (status.equalsIgnoreCase("approved")) {
            String sql = "UPDATE users SET verified = 1 WHERE id = '" + userId + "';";
            int rc = sqlite3_exec(db, sql.c_str(), NULL, NULL, &zErrMsg);
            if (rc != SQLITE_OK) {
              Serial.print("❌ DB update failed: ");
              Serial.println(zErrMsg);
              sqlite3_free(zErrMsg);
            } else {
              Serial.println("✅ User marked as verified.");
              approvalQueue.push_back({ userId, millis() + 10000, false });
            }
          } else if (status.equalsIgnoreCase("rejected")) {
            String sql = "DELETE FROM users WHERE id = '" + userId + "';";
            int rc = sqlite3_exec(db, sql.c_str(), NULL, NULL, &zErrMsg);
            if (rc != SQLITE_OK) {
              Serial.print("❌ Failed to delete user: ");
              Serial.println(zErrMsg);
              sqlite3_free(zErrMsg);
            } else {
              Serial.println("🗑️ User rejected and removed.");
              rejectQueue.push_back({ userId, millis() + 10000, false });
            }
          }

          StaticJsonDocument<200> response;
          response["type"] = "verify";
          response["id"] = userId;
          response["status"] = status;
          String out;
          serializeJson(response, out);
          webSocket.broadcastTXT(out);
        } else if (msgType.equalsIgnoreCase("block")) {
          String blockedId = doc["id"] | "";
          Serial.println("🚫 Blocking user ID: " + blockedId);
          blockedUsers.push_back(blockedId);
        } else if (msgType.equalsIgnoreCase("unblock")) {
          String unblockedId = doc["id"] | "";
          Serial.println("🔓 Unblocking user ID: " + unblockedId);
          for (int i = blockedUsers.size() - 1; i >= 0; --i) {
            if (blockedUsers[i] == unblockedId) {
              blockedUsers.erase(blockedUsers.begin() + i);
              Serial.println("✅ Removed from blocked users list.");
              break;
            }
          }
          StaticJsonDocument<128> resp;
          resp["type"] = "unblock";
          resp["id"] = unblockedId;
          String out;
          serializeJson(resp, out);
          webSocket.broadcastTXT(out);
        } else if (msgType.equalsIgnoreCase("NetworkInfo")) {
          StaticJsonDocument<256> infoDoc;
          infoDoc["type"] = "NetworkInfo";
          infoDoc["zoneId"] = String(CurrentZoneId);
          String jsonStr;
          serializeJson(infoDoc, jsonStr);
          webSocket.broadcastTXT(jsonStr);
          Serial.println("📡 Broadcasted Network info");
        } else if (msgType == "ZonesCheck") {
          StaticJsonDocument<256> doc;
          doc["type"] = "ZonesCheck";
          doc["senderZone"] = String(CurrentZoneId);
          doc["receiverZone"] = " ";
          doc["lat"] = latitude;
          doc["lng"] = longitude;
          String jsonStr;
          serializeJson(doc, jsonStr);
          webSocket.broadcastTXT(jsonStr);
          Serial.println("Broadcasting current zone");
          sendLoraMessage(" ", "Admin", msgType, " ", String(CurrentZoneId), "ALL");
          Serial.println("Checking other zones");
        } else if (msgType.equalsIgnoreCase("GetUsers")) {
          printAllUsers();
        } else if (msgType.equalsIgnoreCase("NewChannel")) {
          String name = doc["name"] | "";
          String channelType = doc["channelType"] | "chat";
          bool exists = false;
          for (auto &ch : channels) {
            if (ch.name.equalsIgnoreCase(name)) {
              exists = true;
              break;
            }
          }
          if (!exists) {
            channels.push_back({ name, channelType });
            Serial.println(":white_check_mark: Channel added: " + name + " (" + channelType + ")");
          }
          String outgoing;
          serializeJson(doc, outgoing);
          webSocket.broadcastTXT(outgoing);
        } else if (
          msgType == "chat" || msgType == "alert" || msgType == "news" || msgType == "main" || msgType == "sos") {
          senderId = doc["senderID"] | "";
          UserName = doc["username"] | "";
          content = doc["content"] | "";
          channelId = doc["channelID"] | "";
          ZoneID = doc["zoneId"] | "";
          ReceiverZone = doc["receiverZone"] | "";
          Role = doc["role"] | "";

          storeMessage(senderId, content, msgType, channelId, ZoneID, ReceiverZone);

          if (ReceiverZone == String(CurrentZoneId)) {
            webSocket.broadcastTXT(payload);
            Serial.println(":satellite: Broadcasted to local zone");
          } else {
            sendLoraMessage(senderId, UserName,msgType, content, ZoneID, ReceiverZone);
            Serial.println(":satellite: Forwarded via LoRa");
          }
        }
        break;
      }
    case WStype_DISCONNECTED:
      Serial.println("⚠️ Client disconnected.");
      break;
  }
}

// ==== Send over LoRa ====
void sendLoraMessage(String Id, String name, String type, String content, String ZoneID, String receiverZoneId) {
  StaticJsonDocument<256> doc;
  if (type == "ZonesCheck" || type == "ZoneAnnounce") {
    doc["type"] = type;
    doc["senderZone"] = ZoneID;
    doc["receiverZone"] = receiverZoneId;
    doc["lat"] = latitude;
    doc["lng"] = longitude;
  } else {
    doc["senderID"] = Id;
    doc["username"] = name;
    doc["content"] = content;
    doc["type"] = type;
    doc["receiverZone"] = receiverZoneId;
    doc["zoneId"] = ZoneID;
  }
  String jsonStr;
  serializeJson(doc, jsonStr);

  LoRa.beginPacket();
  LoRa.print(jsonStr);
  LoRa.endPacket();
  Serial.print("LoRa Sent: " + jsonStr);
}

// ==== Receive LoRa Messages ====
void receiveLoraMessage() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.println("LoRa Packet Received:");
    String received = "";
    while (LoRa.available()) {
      char c = (char)LoRa.read();
      Serial.print(c);  // Raw character
      received += c;
    }
    Serial.println();
    StaticJsonDocument<512> doc;
    if (!deserializeJson(doc, received)) {
      String msgType = doc["type"];
      if (msgType == "ZonesCheck") {
        String SenderZone = doc["senderZone"];
        String ReceiverZone = String(CurrentZoneId);
        sendLoraMessage(" ", "Admin", "ZoneAnnounce", " ", SenderZone, ReceiverZone);
      } else if (msgType == "ZoneAnnounce") {
        webSocket.broadcastTXT(received);
      } else {
        String senderId = doc["senderID"];
        String username = doc["username"];
        String content = doc["content"];
        String ReceiverZone = doc["receiverZone"];
        String channelId = "5";
        String ZoneID = doc["zoneId"];
        storeMessage(senderId, content, msgType, channelId, ZoneID, ReceiverZone);
        webSocket.broadcastTXT(received);
        Serial.println("📡 Broadcasting to mobile");
      }
    } else {
      Serial.println("JSON Parse Error");
    }
  }
}

// Function to broadcast new zone message
void broadcastNewZoneOnStartup() {
  StaticJsonDocument<256> doc;
  doc["type"] = "ZoneAnnounce";
  doc["zoneID"] = CurrentZoneId;
  String message;
  serializeJson(doc, message);

  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
  Serial.println("📡 Broadcasted new zone: " + String(CurrentZoneId));
}

void sendLocationOverWebSocket() {
  StaticJsonDocument<128> doc;
  doc["type"] = "location";
  doc["lat"] = latitude;
  doc["lng"] = longitude;

  String message;
  serializeJson(doc, message);
  webSocket.broadcastTXT(message); 
}

// ==== Setup ====
void setup() {
  Serial.begin(115200);
  // OLED Init
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed");
    while (true)
      ;
  }
  displayMessage("Starting...");

  Serial.println("\n\n=== Starting LoRescue System ===");
  esp_log_level_set("*", ESP_LOG_VERBOSE);

  // Format and mount SPIFFS
  if (!SPIFFS.begin(true)) {  // true means format if mount fails
    Serial.println("SPIFFS Mount Failed or Formatted!");
    return;  // Stop the program if SPIFFS cannot be initialized
  } else {
    Serial.println("SPIFFS mounted successfully");
  }

  // Make sure DB file exists
  if (!SPIFFS.exists("/lorescue.db")) {
    Serial.println("Creating a new database file...");
    File file = SPIFFS.open("/lorescue.db", FILE_WRITE);
    if (file) {
      file.close();
      Serial.println("Database file created");
    } else {
      Serial.println("Failed to create database file");
      return;
    }
  } else {
    Serial.println("Database file exists");
  }

  // Open SQLite database
  if (sqlite3_open("/spiffs/lorescue.db", &db) != SQLITE_OK) {
    Serial.printf("Can't open database: %s\n", sqlite3_errmsg(db));
    return;
  }
  // Execute pragma commands to optimize SQLite for ESP32
  sqlite3_exec(db, "PRAGMA journal_mode=OFF;", NULL, NULL, &zErrMsg);
  sqlite3_exec(db, "PRAGMA temp_store=MEMORY;", NULL, NULL, &zErrMsg);
  sqlite3_exec(db, "PRAGMA synchronous=OFF;", NULL, NULL, &zErrMsg);

  // Create tables
  initDatabase();

  // GPS initializing
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("Waiting for GPS fix...");

  // Wi-Fi Init
  WiFi.softAP(ssid, password);
  Serial.print("Access Point started. IP Address: ");
  Serial.println(WiFi.softAPIP());

  // WebSocket Init
  webSocket.begin();
  webSocket.enableHeartbeat(15000, 3000, 2);  // 15s ping interval, 3s pong timeout, 2 failed attempts before disconnect

  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started on Port:81");

  // LoRa Init
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(868E6)) {
    Serial.println("Starting LoRa failed!");
    displayMessage("LoRa Failed!");
    while (true)
      ;
  }
  Serial.println("LoRa initialized successfully");
  displayMessage(CurrentZoneId);

  Serial.println("Setup complete!");
  broadcastNewZoneOnStartup();
}

// ==== Main Loop ====
void loop() {
  webSocket.loop();
  receiveLoraMessage();
  sendPendingLicenses();
  checkRejectionQueue();
  checkApprovalQueue();

  // Read GPS data if available
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  // Send GPS location every 5 seconds if valid
  if (gps.location.isValid() && millis() - lastGpsSendTime >= gpsInterval) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    //sendLocationOverWebSocket(); // 🔄 Send to WebSocket clients
    lastGpsSendTime = millis();  // Reset the timer
  } 

  delay(10);
}