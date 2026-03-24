import paho.mqtt.client as mqtt
import time

# ================= ตั้งค่า MQTT จำลอง =================
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883

client = mqtt.Client()

def on_connect(client, userdata, flags, rc):
    print(f"✅ เชื่อมต่อ MQTT Broker จำลองสำเร็จ (รหัส {rc})")

client.on_connect = on_connect

client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_start()

# ================= ล็อคค่าคงที่ (Fixed Values) =================
# อิงตามตัวเลขสวยๆ จากรูป UI ที่คุณเจนมาเลยครับ
fixed_data = {
    2: {"level": 0, "temp": 26.5, "humid": 40.0, "status": "NORMAL"},
    3: {"level": 79, "temp": 29.1, "humid": 45.0, "status": "NORMAL"},   # ถังนี้ตั้งใจให้เกิน 80% เพื่อโชว์แจ้งเตือน
    4: {"level": 62, "temp": 27.8, "humid": 41.0, "status": "NORMAL"}
}

print("🚀 เริ่มต้นส่งข้อมูลจำลองแบบ Fixed (ถัง 2, 3, 4)... กด Ctrl+C เพื่อหยุด")
print("-" * 50)

while True:
    for bin_id, data in fixed_data.items():
        base_topic = f"smartbin/fleet/{bin_id}"

        # ดึงค่าที่ล็อคไว้ออกมา
        current_level = data["level"]
        current_temp = data["temp"]
        current_humid = data["humid"]
        status = data["status"]

        # ส่งข้อมูลทั้งหมดขึ้น HiveMQ
        client.publish(f"{base_topic}/level", str(current_level))
        client.publish(f"{base_topic}/temp", str(current_temp))
        client.publish(f"{base_topic}/humidity", str(current_humid))
        
        # ส่ง Alert ทันทีถ้าสถานะถูกตั้งไว้ว่า FULL
        if status == "FULL":
            client.publish(f"{base_topic}/alert", "FULL")

        print(f"[{time.strftime('%H:%M:%S')}] Bin {bin_id} (Fixed) -> Level: {current_level}%, Temp: {current_temp}°C, Humid: {current_humid}%")

    print("-" * 30)
    # ส่งข้อมูลซ้ำทุกๆ 5 วินาที (เพื่อให้หน้าเว็บที่เพิ่งกดเข้ามาใหม่ ได้รับข้อมูลไปแสดงผลด้วย)
    time.sleep(5)