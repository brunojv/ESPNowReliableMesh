/*
   Programmer: Bruno Villalobos
   Version: R1.0
   Description: ESP_NOW mesh with packet transmition check for duplicated and TTL, this is the repeater code and the base for the information sender's code.


*/

#include <WiFi.h>
#include <esp_now.h>
#include <vector>
#include "timerClass.h"
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  1800        /* Time ESP32 will go to sleep (in seconds) */


timerClass timer1;

// Expanded telemetry struct
typedef struct struct_message {
  int packetId;        // unique ID for duplicate detection
  int ttl;             // time-to-live (max hops)
  char sender[10];     // node identifier

  // Environmental data
  float temperature;   // °C
  float humidity;      // %

  // Vehicle-related data (non-CAN)
  float km_runned;     // distance traveled
  float petro_spent;   // fuel consumed

  // Suggested extras
  float speed;         // km/h
  float gps_lat;       // latitude
  float gps_lon;       // longitude
  float battery_volt;  // car battery voltage
  float accel_x;       // acceleration X
  float accel_y;       // acceleration Y
  float accel_z;       // acceleration Z
}struct_message;

struct_message myData;
struct_message incomingData;

int packetCounter = 0;
std::vector<int> seenPackets;

// --- Duplicate detection helpers ---
bool isDuplicate(int packetId) {
  for (int pid : seenPackets) {
    if (pid == packetId) return true;
  }
  return false;
}

void rememberPacket(int packetId) {
  seenPackets.push_back(packetId);
  if (seenPackets.size() > 50) { // limit memory
    seenPackets.erase(seenPackets.begin());
  }
}

void addPeer(const uint8_t *mac) {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = WiFi.channel();  // must match across all nodes
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial.printf("Peer %02X:%02X:%02X:%02X:%02X:%02X added\n",
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    Serial.println("Failed to add peer");
  }
}

// --- Receiver callback ---
void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *incomingDataBytes, int len) {
  struct_message incomingData;
  // Use len guard to avoid over-read
  int copyLen = min(len, (int)sizeof(incomingData));
  memcpy(&incomingData, incomingDataBytes, copyLen);

  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
           recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);

  Serial.println("--------------------------------------------------");
  Serial.printf("%s received from %s (MAC: %s)\n", myData.sender, incomingData.sender, macStr);
  Serial.printf("PacketID: %d | TTL: %d | Len: %d\n", incomingData.packetId, incomingData.ttl, len);
  Serial.printf("Temp: %.1f °C | Humidity: %.1f %%\n", incomingData.temperature, incomingData.humidity);
  Serial.printf("Distance: %.1f km | Fuel: %.1f L\n", incomingData.km_runned, incomingData.petro_spent);
  Serial.printf("Speed: %.1f km/h | GPS: (%.5f, %.5f)\n", incomingData.speed, incomingData.gps_lat, incomingData.gps_lon);
  Serial.printf("Battery: %.2f V\n", incomingData.battery_volt);
  Serial.printf("Accel: X=%.2f Y=%.2f Z=%.2f\n", incomingData.accel_x, incomingData.accel_y, incomingData.accel_z);
  Serial.println("--------------------------------------------------");

  if (!isDuplicate(incomingData.packetId)) {
    rememberPacket(incomingData.packetId);

    if (strcmp(incomingData.sender, myData.sender) != 0 && incomingData.ttl > 0) {
      incomingData.ttl--;
      esp_err_t result = esp_now_send(NULL, (uint8_t *)&incomingData, sizeof(incomingData));
      if (result == ESP_OK) {
        Serial.println("Forwarded packet");
      } else {
        Serial.printf("Error forwarding packet, code: %d\n", result);
      }
    }
  } else {
    Serial.println("Duplicate packet ignored");
  }
  

}


// --- Setup ---
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  
  // ESP_NOW init 
  esp_err_t espInitEstatus = esp_now_init();
  delay(100); // short pause after esp_now_init()
 
  if (espInitEstatus != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  Serial.println (String(espInitEstatus));
  Serial.printf("Struct size: %d bytes\n", sizeof(myData));
  Serial.printf("Size of struct_message: %d\n", sizeof(struct_message));
  Serial.printf("Size of float: %d\n", sizeof(float));
  Serial.printf("Size of int: %d\n", sizeof(int));
  Serial.printf("Size of sender: %d\n", sizeof(myData.sender));
    
  esp_now_peer_info_t peerInfo = {};
  //uint8_t peerAddress[] = {0xCC, 0xBA, 0x97, 0x33, 0xCD, 0x64}; // your target MAC
  //uint8_t peerAddress[] = {0x94, 0x54, 0xC5, 0x4D, 0xA5, 0xA1}; // your target MAC, original A0
  
  
  //Node A: Waweshare 6DOR
  String Nodename = "NodeA";
  uint8_t macB[] = {0x94,0x54,0xC5,0x4D,0xA5,0xA0}; //ESP Wroom in breadboard
  uint8_t macC[] = {0x84,0xFC,0xE6,0x51,0x4E,0x3C}; // ESP-USB Dongle
  addPeer(macB);
  addPeer(macC);
  
  
 /*
  //Node B: ESP32S3 USB-Dongle
  String  Nodename = "NodeB";
  uint8_t macA[] = {0xCC,0xBA,0x97,0x33,0xCD,0x64}; //Waweshare 6DOR
  uint8_t macC[] = {0x94,0x54,0xC5,0x4D,0xA5,0xA0}; //ESP Wroom in breadboard
  addPeer(macA);
  addPeer(macC);
  
*/
  /*

  //Node C: ESP32-Wroom
  String Nodename = "NodeC";   
  uint8_t macA[] = {0xCC,0xBA,0x97,0x33,0xCD,0x64}; //Waweshare 6DOR
  uint8_t macB[] = {0x84,0xFC,0xE6,0x51,0x4E,0x3C}; // ESP-USB Dongle
  addPeer(macA);
  addPeer(macB);
  
*/
  Serial.println(WiFi.macAddress());
  delay(4000);

  esp_now_register_recv_cb(OnDataRecv);

  // Identify this node (change per board)
  strcpy(myData.sender, Nodename.c_str());  // NodeA, NodeB or NodeC
  
}

// --- Broadcast telemetry ---
void broadcastData() {
  myData.packetId = packetCounter++;
  myData.ttl = 2;  // allow up to 2 hops

  // Dummy sensor values (replace with real sensors later)
  myData.temperature = random(0, 40) / 10.0;;             // 0-40 C
  myData.humidity    = random(0, 100) / 10.0;;   // 0 -100 %
  myData.km_runned   = random(0, 1000) / 10.0;    // 0–100 km
  myData.petro_spent = random(0, 500) / 10.0;     // 0–50 L
  myData.speed       = random(0, 120);            // 0–120 km/h
  myData.gps_lat     = 51.8985;                   // Example latitude
  myData.gps_lon     = -8.4756;                   // Example longitude
  myData.battery_volt= random(115, 135) / 10.0;   // 11.5–13.5 V
  myData.accel_x     = random(-100, 100) / 10.0;
  myData.accel_y     = random(-100, 100) / 10.0;
  myData.accel_z     = random(-100, 100) / 10.0;
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Broadcasting MAC
  // Broadcast packet
  esp_err_t result = esp_now_send(NULL, (uint8_t*)&myData, sizeof(myData));
  Serial.printf("Send result: %d\n", result);
  if (result == ESP_OK) {
    Serial.println("Broadcasted telemetry packet");
  } else {
    Serial.println("Error broadcasting packet");
  }

}

// --- Loop ---
void loop() {
  bool timer1Done;
  
  timer1Done = timer1.timer_LOOP(true, 5000);
  if (timer1Done){
    
    //Loops
    broadcastData();
  }

  
}
