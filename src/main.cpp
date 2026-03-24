#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Wire.h> // --- เพิ่ม Library สำหรับ I2C ---
#include <MFRC522.h>
#include <secrets.h>

// --- Library จอ ST7789 ---
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h> 

// --- Library เซนเซอร์ AHT20 & BMP280 ---
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>

// ---------------- ตั้งค่า WiFi & MQTT ----------------
const char* ssid = SECRET_WIFI_SSID;
const char* password = SECRET_WIFI_PASS;
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// ---------------- ตั้งค่า PIN ----------------
#define TRIG_PIN 32
#define ECHO_PIN 33
#define BUZZER_PIN 14
#define RFID_SS_PIN 5
#define RFID_RST_PIN 27

// --- ขาของจอ TFT ---
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4

// --- ขา I2C ที่เราย้ายใหม่ ---
#define I2C_SDA 25
#define I2C_SCL 26

// ---------------- ตัวแปรระบบ ----------------
MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// สร้าง Object สำหรับเซนเซอร์
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

String knownCard = "c6 fb 34 06"; 
bool isBinFull = false;
unsigned long lastMeasureTime = 0;

void beep(int times, int duration) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, LOW);
    delay(duration);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
  }
}

void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "SmartBin-ESP32-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) { } 
    else { delay(5000); }
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  
  // --- 1. เริ่มต้น I2C ที่ขา 25, 26 ---
  Wire.begin(I2C_SDA, I2C_SCL);

  // --- 2. เริ่มต้นจอ TFT ---
  tft.init(240, 320); 
  tft.setRotation(1); 
  tft.fillScreen(ST77XX_BLACK); 
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); 
  tft.setTextSize(2);
  tft.setCursor(40, 110);
  tft.println("System Starting...");

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  
  SPI.begin(); 
  mfrc522.PCD_Init(); 

  // --- 3. เริ่มต้น AHT & BMP280 ---
  if (!aht.begin()) {
    Serial.println("❌ หาเซนเซอร์ AHT ไม่เจอ!");
  }
  // โมดูลรวมแบบนี้ BMP280 มักจะใช้ Address 0x76 (ถ้าไม่ได้ให้แก้เป็น 0x77)
  if (!bmp.begin(0x76)) {
    Serial.println("❌ หาเซนเซอร์ BMP280 ไม่เจอ!");
  }
  
  tft.fillScreen(ST77XX_BLUE); 
  tft.setCursor(50, 110);
  tft.setTextColor(ST77XX_WHITE);
  tft.println("Smart Bin Ready!");
  
  beep(2, 100); 
}

void loop() {
  if (!client.connected()) { reconnect(); }
  client.loop();

  // 1. ตรวจสอบข้อมูลทุก 5 วินาที
  if (millis() - lastMeasureTime > 5000) {
    lastMeasureTime = millis();
    
    // --- อ่านค่าขยะ ---
    digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH);
    int distance = duration * 0.034 / 2;
    int percent = constrain(map(distance, 30, 5, 0, 100), 0, 100);
    
    // --- อ่านค่าอุณหภูมิและความชื้น ---
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    float t = temp.temperature;
    float h = humidity.relative_humidity;
    float p = bmp.readPressure() / 100.0F; // ความดัน (hPa)
    
    // --- ส่งข้อมูลขึ้น MQTT แบบเหมาเข่ง ---
    client.publish("smartbin/level", String(percent).c_str());
    client.publish("smartbin/temp", String(t).c_str());
    client.publish("smartbin/humidity", String(h).c_str());

    // --- อัปเดตหน้าจอสถานะปกติ (แนวนอน) ---
    if (!isBinFull) {
      tft.fillScreen(ST77XX_BLACK); 
      
      tft.setTextSize(2);
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(60, 15); 
      tft.println("SMART BIN SYSTEM");
      tft.drawFastHLine(10, 40, 300, ST77XX_WHITE); 

      // หลอดขยะ
      int barX = 20;      
      int barY = 60;      
      int barWidth = 60;  
      int barHeight = 150; 
      int fillHeight = map(percent, 0, 100, 0, barHeight);

      uint16_t barColor;
      if (percent < 50) barColor = ST77XX_GREEN;
      else if (percent < 80) barColor = ST77XX_ORANGE;
      else barColor = ST77XX_RED;

      tft.drawRect(barX, barY, barWidth, barHeight, ST77XX_WHITE); 
      tft.fillRect(barX + 2, barY + barHeight - fillHeight + 2, barWidth - 4, fillHeight - 4, barColor);

      // % ขยะ
      tft.setTextSize(5); 
      tft.setTextColor(barColor);
      tft.setCursor(110, 60);
      tft.print(percent);
      tft.println("%");

      // ระยะทาง ขยับขึ้นมานิดหน่อย
      tft.setTextSize(2);
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(110, 120);
      tft.print("Dist : ");
      tft.setTextColor(ST77XX_CYAN);
      tft.print(distance);
      tft.println(" cm");

      // สถานะ
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(110, 150);
      tft.print("Stat : ");
      tft.setTextColor(ST77XX_GREEN);
      tft.println("NORMAL");
      
      // 🔽 แสดงอุณหภูมิและความชื้น 🔽
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(110, 180);
      tft.print("Temp : ");
      tft.setTextColor(ST77XX_YELLOW);
      tft.print(t, 1); // โชว์ทศนิยม 1 ตำแหน่ง
      tft.println(" C");
      
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(110, 210);
      tft.print("Humid: ");
      tft.setTextColor(ST77XX_ORANGE);
      tft.print(h, 1);
      tft.println(" %");
    }

    if (percent > 80 && !isBinFull) {
      isBinFull = true;
      client.publish("smartbin/alert", "FULL"); 
      
      tft.fillScreen(ST77XX_RED);
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(5);
      tft.setCursor(40, 70);
      tft.println("BIN FULL");
      
      tft.setTextSize(2);
      tft.setTextColor(ST77XX_YELLOW);
      tft.setCursor(70, 150);
      tft.println("Tap RFID Card");
      
      beep(3, 200); 
    }
  }

  // 2. ระบบ RFID รอรับการยืนยันตัวตน (เหมือนเดิมเป๊ะ)
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String cardUID = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      cardUID += String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      cardUID += String(mfrc522.uid.uidByte[i], HEX);
    }
    cardUID.trim(); 

    if (cardUID.equalsIgnoreCase(knownCard)) {
      client.publish("smartbin/worker", "Arsu Maehae acknowledged the task");
      isBinFull = false; 
      
      tft.fillScreen(ST77XX_GREEN);
      tft.setTextColor(ST77XX_BLACK);
      tft.setTextSize(4);
      tft.setCursor(65, 80);
      tft.println("GRANTED");
      
      tft.setTextSize(2);
      tft.setCursor(60, 150);
      tft.println("Worker: Arsu M.");
      
      beep(1, 400); 
      delay(3000); 
      
    } else {
      tft.fillScreen(ST77XX_ORANGE);
      tft.setTextColor(ST77XX_BLACK);
      tft.setTextSize(4);
      tft.setCursor(80, 100);
      tft.println("DENIED");
      
      beep(3, 80); 
      delay(2000);
    }
    mfrc522.PICC_HaltA(); 
  }
}