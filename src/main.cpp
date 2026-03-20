#include <Arduino.h>
#include <TFT_eSPI.h> 
#include <WiFi.h>
#include <PubSubClient.h> // 🚨 ไลบรารี MQTT
#include "secrets.h"

TFT_eSPI tft = TFT_eSPI(); 

#define PIN_TRIG  12  
#define PIN_ECHO  14  

const float BIN_EMPTY_CM = 40.0;
const float BIN_FULL_CM = 8.0;

// === 🚨 ข้อมูล WiFi ===
// 🚨 ดึงค่าจาก secrets.h มาใช้
const char* ssid = SECRET_WIFI_SSID;        
const char* password = SECRET_WIFI_PASS;

// === 🚨 ข้อมูล MQTT Broker (ใช้ของฟรี HiveMQ) ===
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
// หัวข้อ (Topic) ที่เราจะส่งข้อมูลไป (ตั้งชื่อให้ไม่ซ้ำกับคนอื่นในโลก)
const char* mqtt_topic = "arsu/smartbin/zoneA"; 

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastUpdate = 0;

// ฟังก์ชันสำหรับเชื่อมต่อ MQTT อัตโนมัติเวลาหลุด
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // สร้างชื่อ Client ID แบบสุ่มให้ ESP32
    String clientId = "ESP32Client-SmartBin";
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  tft.begin();
  tft.setRotation(1); 
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(3);
  tft.setCursor(10, 20);
  tft.println("SMART WASTE BIN");

  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(10, 80);
  tft.print("Connecting WiFi...");
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  tft.fillRect(0, 60, 320, 180, TFT_BLACK); 
  tft.drawLine(10, 60, 310, 60, TFT_WHITE);
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(10, 75);
  tft.print("WiFi Connected!");

  // ตั้งค่า MQTT Server
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  // รักษาการเชื่อมต่อ MQTT ไว้ตลอดเวลา
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // อัปเดตข้อมูลทุกๆ 2 วินาที (ไม่ควรส่งถี่เกินไปเดี๋ยว Broker เตะออก)
  if (millis() - lastUpdate > 2000) { 
    lastUpdate = millis();

    float totalDistance = 0;
    int validReads = 0;

    for (int i = 0; i < 3; i++) {
      digitalWrite(PIN_TRIG, LOW);
      delayMicroseconds(2);
      digitalWrite(PIN_TRIG, HIGH);
      delayMicroseconds(10);
      digitalWrite(PIN_TRIG, LOW);

      long duration = pulseIn(PIN_ECHO, HIGH, 30000); 
      if (duration > 0) {
        float dist = (duration * 0.0343) / 2.0;
        if (dist > 0 && dist < 400) { 
          totalDistance += dist;
          validReads++;
        }
      }
      delay(10);
    }

    float distanceCm = BIN_EMPTY_CM; 
    if (validReads > 0) {
      distanceCm = totalDistance / validReads;
    }

    int percent = 0;
    if (distanceCm >= BIN_EMPTY_CM) {
      percent = 0;
    } else if (distanceCm <= BIN_FULL_CM) {
      percent = 100;
    } else {
      percent = map(distanceCm, BIN_EMPTY_CM, BIN_FULL_CM, 0, 100);
    }
    percent = constrain(percent, 0, 100);

    // สร้างสถานะข้อความ
    String statusMsg = (percent >= 80) ? "FULL" : "NORMAL";

    // --- 🚨 สร้างข้อมูลแบบ JSON และส่งขึ้น MQTT ---
    String jsonPayload = "{";
    jsonPayload += "\"percent\": " + String(percent) + ", ";
    jsonPayload += "\"distance_cm\": " + String(distanceCm, 1) + ", ";
    jsonPayload += "\"status\": \"" + statusMsg + "\"";
    jsonPayload += "}";

    // ส่งข้อความไปที่ Topic
    client.publish(mqtt_topic, jsonPayload.c_str());
    Serial.println("Published: " + jsonPayload);

    // --- อัปเดตหน้าจอ TFT ให้เราดูด้วย ---
    tft.fillRect(0, 110, 320, 130, TFT_BLACK); 
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK); 
    tft.setTextSize(5); 
    char percentStr[10];
    sprintf(percentStr, "%3d %%", percent); 
    tft.setCursor(10, 115);
    tft.print(percentStr);

    tft.setTextSize(2);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.setCursor(10, 165);
    tft.printf("Distance: %5.1f cm", distanceCm); 

    tft.setTextSize(2);
    tft.setCursor(10, 205);
    if (percent >= 80) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.print("STATUS: FULL!              ");
    } else {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.print("STATUS: Normal             "); 
    }
  }
}