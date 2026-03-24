import streamlit as st
import json
import time

# ตั้งค่าหน้าเว็บเบื้องต้น
st.set_page_config(page_title="Smart Bin Dashboard", page_icon="🗑️", layout="centered")

st.title("🏥 Smart Medical Waste Bin")
st.markdown("ระบบติดตามปริมาณขยะทางการแพทย์แบบ Real-time ผ่าน MQTT")
st.markdown("---")

# สร้าง "กล่องเปล่า" (Placeholder) เตรียมไว้อัปเดตข้อมูลแบบสดๆ
col1, col2, col3 = st.columns(3)
pct_box = col1.empty()
dist_box = col2.empty()
status_box = col3.empty()

st.write("### ระดับความจุของถัง")
progress_box = st.empty()

st.caption("🔴 Live Data from ESP32 -> HiveMQ -> Python")

# ลูปทำงานแบบ Real-time ดึงข้อมูลมาโชว์เรื่อยๆ
while True:
    try:
        # อ่านข้อมูลล่าสุดจากไฟล์ JSON ที่ Backend เซฟไว้
        with open("latest_data.json", "r") as f:
            data = json.load(f)
        
        percent = data.get("percent", 0)
        dist = data.get("distance_cm", 0.0)
        status = data.get("status", "UNKNOWN")

        # อัปเดตตัวเลขในกล่อง
        pct_box.metric("ปริมาณขยะ (Fill Level)", f"{percent} %")
        dist_box.metric("ระยะเซนเซอร์ (Distance)", f"{dist} cm")
        
        # อัปเดตสถานะและสี
        if status == "FULL":
            status_box.error("🚨 สถานะ: เต็ม! (FULL)")
        else:
            status_box.success("✅ สถานะ: ปกติ (NORMAL)")
        
        # อัปเดตหลอด Progress Bar
        progress_box.progress(percent / 100.0)

    except (FileNotFoundError, json.JSONDecodeError):
        # กรณีที่เปิดเว็บก่อนที่ ESP32 จะส่งข้อมูลมา
        pass
    
    # หน่วงเวลา 1 วินาทีก่อนดึงข้อมูลรอบถัดไป
    time.sleep(1)