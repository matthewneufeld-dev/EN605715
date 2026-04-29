#!/usr/bin/env python3
 
from smbus2 import SMBus
import socket
import time
import gps
 
ARDUINO_ADDR = 0x08
I2C_BUS_NUM = 1
I2C_BYTES = 32
 
HOST_IP = "192.168.1.33"
HOST_PORT = 5005
 
SEND_RATE_SEC = 0.12
 
bus = SMBus(I2C_BUS_NUM)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
gps_session = gps.gps(mode=gps.WATCH_ENABLE | gps.WATCH_NEWSTYLE)
 
latest_imu = "R:NA,P:NA,Y:NA"
latest_lat = "NA"
latest_lon = "NA"
latest_alt = "NA"
 
def reopen_i2c():
    global bus
    try:
        bus.close()
    except Exception:
        pass
    time.sleep(0.5)
    bus = SMBus(I2C_BUS_NUM)
 
def read_imu_i2c():
    global latest_imu
 
    try:
        data = bus.read_i2c_block_data(ARDUINO_ADDR, 0, I2C_BYTES)
        text = ''.join(chr(b) for b in data if b not in (0, 255)).strip()
 
        if text.startswith("R:") and ",P:" in text and ",Y:" in text:
            latest_imu = text
 
    except OSError as e:
        print(f"I2C read error, keeping last IMU: {e}")
        reopen_i2c()
 
    return latest_imu
 
def update_gps():
    global latest_lat, latest_lon, latest_alt
 
    try:
        report = gps_session.next()
 
        if report["class"] == "TPV":
            if hasattr(report, "lat"):
                latest_lat = f"{report.lat:.6f}"
            if hasattr(report, "lon"):
                latest_lon = f"{report.lon:.6f}"
            if hasattr(report, "alt"):
                latest_alt = f"{report.alt:.2f}"
 
    except Exception as e:
        print(f"GPS read error: {e}")
 
print(f"Streaming UDP telemetry to {HOST_IP}:{HOST_PORT}")
 
while True:
    imu = read_imu_i2c()
    update_gps()
 
    msg = f"{imu},LAT:{latest_lat},LON:{latest_lon},ALT:{latest_alt}\n"
    print(msg)
 
    sock.sendto(msg.encode("utf-8"), (HOST_IP, HOST_PORT))
 
    time.sleep(SEND_RATE_SEC)