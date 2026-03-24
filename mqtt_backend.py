import paho.mqtt.client as mqtt
import json
import requests # ไลบรารีสำหรับยิง HTTP Request (ใช้ส่ง LINE)
from dotenv import load_dotenv
import os

load_dotenv()


# === 🚨 ตั้งค่า LINE Messaging API ของคุณ ===
CHANNEL_TOKEN = os.getenv("LINE_TOKEN")
USER_ID = os.getenv("LINE_USER_ID")

MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "arsu/smartbin/zoneA"

is_line_notified = False # สร้างตัวล็อคกันส่ง LINE รัวๆ ไว้ที่ฝั่ง Server เลย

# --- ฟังก์ชันสำหรับยิง LINE จาก Python ---
def send_line_message(text):
    url = "https://api.line.me/v2/bot/message/push"
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {CHANNEL_TOKEN}"
    }
    payload = {
        "to": USER_ID,
        "messages": [{"type": "text", "text": text}]
    }
    response = requests.post(url, headers=headers, json=payload)
    if response.status_code == 200:
        print("   ✅ ยิงข้อความเข้า LINE สำเร็จ!")
    else:
        print(f"   ❌ ส่ง LINE พลาด: {response.text}")

# --- อัปเดตฟังก์ชันเชื่อมต่อเป็นเวอร์ชัน 2 (แก้ Warning) ---
def on_connect(client, userdata, flags, reason_code, properties):
    print("✅ เชื่อมต่อ MQTT Broker สำเร็จ!")
    client.subscribe(MQTT_TOPIC)
    print(f"📡 กำลังดักฟังข้อมูลที่ Topic: {MQTT_TOPIC} ...\n")

# --- ฟังก์ชันประมวลผลเมื่อได้รับข้อมูล ---
def on_message(client, userdata, msg):
    global is_line_notified # เรียกใช้ตัวแปรล็อค
    
    payload = msg.payload.decode('utf-8')
    data = json.loads(payload)
    
    percent = data.get("percent")
    status = data.get("status")
    
    print(f"📥 ระดับขยะ: {percent}% | สถานะ: {status}")
    
    # 👈 1. เพิ่มโค้ดส่วนนี้ เพื่อเซฟข้อมูลล่าสุดลงไฟล์ JSON (ใช้เป็นฐานข้อมูลชั่วคราว)
    with open("latest_data.json", "w") as f:
        json.dump(data, f)
    
    # === สร้างความฉลาด (Logic) ไว้ที่นี่ ===
    if status == "FULL":
        if not is_line_notified:
            print("   🚨 ตรวจพบขยะเต็ม! กำลังส่ง LINE เรียกแม่บ้าน...")
            send_line_message(f"⚠️ แจ้งเตือนด่วน: ถังขยะการแพทย์เต็มแล้ว ({percent}%)")
            is_line_notified = True # ล็อคไว้ จะได้ไม่ส่งซ้ำ
            
    else: # ถ้าน้อยกว่า 80%
        if is_line_notified and percent < 70:
            print("   🧹 ขยะถูกเก็บแล้ว กำลังส่ง LINE ปิดจ๊อบ...")
            send_line_message("✅ ขอบคุณครับ! ถังขยะถูกจัดเก็บเรียบร้อยแล้ว")
            is_line_notified = False # ปลดล็อค รอเตือนรอบหน้า

# อัปเดตการเรียกใช้ API เป็น V2 ตามที่ระบบแนะนำ (แก้ Warning สีเหลือง)
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message

print("🔄 Starting Python Backend Server...")
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_forever()