#include <TinyGPS++.h>
#include <HardwareSerial.h>

#define GPS_RX 34  
#define GPS_TX 12  

TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
int UTC_OFFSET = 3;  // Jordan is UTC+3

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("Waiting for GPS fix...");
}

void loop() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  if (gps.location.isValid()) {
    Serial.print("Latitude: ");
    Serial.print(gps.location.lat(), 6);
    Serial.print("\tLongitude: ");
    Serial.println(gps.location.lng(), 6);
  }

  if (gps.time.isValid()) {
    int hour = gps.time.hour() + UTC_OFFSET;
    if (hour >= 24) hour -= 24;  // Adjust for next day

    Serial.print("Time (Local): ");
    Serial.print(hour);
    Serial.print(":");
    Serial.print(gps.time.minute());
    Serial.print(":");
    Serial.println(gps.time.second());
  }

  delay(1000);
}
