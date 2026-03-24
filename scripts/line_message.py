import paho.mqtt.client as mqtt
import requests
import datetime
from dotenv import load_dotenv
import os

load_dotenv()

# ================= ตั้งค่าระบบ MQTT =================
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "smartbin/fleet/1/#"

# ================= ตั้งค่า LINE Messaging API =================
CHANNEL_ACCESS_TOKEN = os.getenv("LINE_TOKEN")
LINE_USER_ID = os.getenv("LINE_USER_ID")
LINE_API_URL = "https://api.line.me/v2/bot/message/push"

# ================= ตั้งค่าเกณฑ์อันตราย (เปลี่ยนตัวเลขได้ตามต้องการ) =================
DANGER_TEMP = 35.0   # องศาเซลเซียส
DANGER_HUMID = 80.0  # เปอร์เซ็นต์

# ตัวแปรจำสถานะเพื่อป้องกันการแจ้งเตือนซ้ำ (Anti-Spam)
alert_state = {
    "temp": False,
    "humid": False
}

# ================= ฟังก์ชันส่ง LINE (แบบ Bot Official) =================
def send_line_notify(message):
    headers = {
        'Content-Type': 'application/json',
        'Authorization': f'Bearer {CHANNEL_ACCESS_TOKEN}'
    }
    
    data = {
        "to": LINE_USER_ID,
        "messages": [
            {
                "type": "text",
                "text": message
            }
        ]
    }
    
    try:
        response = requests.post(LINE_API_URL, headers=headers, json=data, timeout=10)
        if response.status_code == 200:
            print("✅ [LINE Bot] ส่งการแจ้งเตือนสำเร็จ!")
        else:
            print(f"❌ [LINE Bot] ส่งล้มเหลว (Code {response.status_code}): {response.text}")
    except Exception as e:
        print(f"❌ [ERROR] อินเทอร์เน็ตมีปัญหา หรือเกิดข้อผิดพลาด: {e}")

# ================= ฟังก์ชันจัดการ MQTT =================
def on_connect(client, userdata, flags, rc):
    print(f"🌐 เชื่อมต่อ MQTT Broker สำเร็จ (รหัส {rc})")
    print(f"📡 กำลังเฝ้าฟังข้อมูลแบบอัจฉริยะจาก Topic: {MQTT_TOPIC}...\n")
    print("-" * 50)
    client.subscribe(MQTT_TOPIC)

def on_message(client, userdata, msg):
    global alert_state
    topic = msg.topic
    payload = msg.payload.decode('utf-8')
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    # --- วิธีใหม่: แยก Topic ออกเป็นส่วนๆ ---
    # เช่น "smartbin/fleet/1/level" จะกลายเป็น list: ['smartbin', 'fleet', '1', 'level']
    parts = topic.split('/')
    
    # ตรวจสอบว่าโครงสร้างถูกตั้งมาแบบ Fleet (มี 4 ส่วน) หรือไม่
    if len(parts) == 4 and parts[0] == "smartbin" and parts[1] == "fleet":
        bin_id = parts[2]
        data_type = parts[3]

        # 1. รับค่าปริมาณขยะ (แค่แสดงผลในจอคอม ไม่ส่งไลน์)
        if data_type == "level":
            print(f"[{now}] 🗑️ ถัง {bin_id} - ปริมาณขยะ: {payload}%")
            
        # 2. ตรวจสอบอุณหภูมิอันตราย
        elif data_type == "temp":
            temp_val = float(payload)
            print(f"[{now}] 🌡️ ถัง {bin_id} - อุณหภูมิ: {temp_val} °C")
            
            if temp_val >= DANGER_TEMP and not alert_state["temp"]:
                alert_state["temp"] = True # ล็อกการแจ้งเตือนไว้
                print(f"🚨 [ALERT] ถัง {bin_id} อุณหภูมิอันตราย! กำลังส่ง LINE...")
                send_line_notify(f"🔥 [เตือนภัยถังขยะติดเชื้อ ถังที่ {bin_id}]\nอุณหภูมิสูงผิดปกติ: {temp_val} °C\nเสี่ยงต่อการสะสมของเชื้อโรคและกลิ่นเน่าเหม็นครับ!")
                
            elif temp_val < DANGER_TEMP - 2.0: 
                alert_state["temp"] = False # ปลดล็อกการแจ้งเตือน

        # 3. ตรวจสอบความชื้นอันตราย
        elif data_type == "humidity":
            humid_val = float(payload)
            print(f"[{now}] 💧 ถัง {bin_id} - ความชื้น: {humid_val}%")
            
            if humid_val >= DANGER_HUMID and not alert_state["humid"]:
                alert_state["humid"] = True
                print(f"🚨 [ALERT] ถัง {bin_id} ความชื้นอันตราย! กำลังส่ง LINE...")
                send_line_notify(f"💦 [เตือนภัยถังขยะติดเชื้อ ถังที่ {bin_id}]\nความชื้นสูงเกินไป: {humid_val}%\nระวังการรั่วซึมของของเหลวและแบคทีเรียครับ!")
                
            elif humid_val < DANGER_HUMID - 5.0:
                alert_state["humid"] = False
                
        # 4. แจ้งเตือนเมื่อขยะเต็ม (จากเซนเซอร์ Ultrasonic)
        elif data_type == "alert":
            if payload == "FULL":
                print(f"\n🚨 [ALERT] ถัง {bin_id} ขยะเต็ม! กำลังส่ง LINE...")
                send_line_notify(f"⚠️ [แจ้งเตือนด่วน]\nถังขยะติดเชื้อที่ {bin_id} เต็มแล้ว! (ความจุ 100%)\nกรุณาส่งเจ้าหน้าที่/แม่บ้านไปจัดการเคลียร์ด่วนครับ 🏃‍♂️💨")
                
        # 5. แจ้งเตือนเมื่อแม่บ้านมาแตะบัตรรับงาน
        elif data_type == "worker":
            print(f"\n👷 [WORKER] ถัง {bin_id} มีการสแกนบัตร! กำลังส่ง LINE...")
            send_line_notify(f"✅ [อัปเดตสถานะ ถังที่ {bin_id}]\n{payload}\nกำลังดำเนินการเคลียร์ถังขยะเรียบร้อยแล้วครับ 🧹✨")

    else:
        # ถ้ารูปแบบ Topic ไม่ตรงกับที่กำหนด (เช่น พวก bin_info ที่ไม่เกี่ยว) ให้ข้ามไป
        pass
    
# ================= เริ่มต้นการทำงาน =================
if __name__ == "__main__":
    print("🚀 เริ่มต้นระบบ Smart Bin Backend (Official Bot Edition)...")
    
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_forever() 
    except KeyboardInterrupt:
        print("\n🛑 ปิดระบบ Monitoring")
        client.disconnect()