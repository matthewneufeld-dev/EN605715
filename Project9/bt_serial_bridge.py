#!/usr/bin/env python3
import os
import glob
import time
import subprocess
import serial

RFCOMM_DEV = "/dev/rfcomm0"
RFCOMM_BAUD = 9600
SERIAL_BAUD = 115200

def log(msg):
    print(f"[DEBUG] {msg}", flush=True)

def list_com_ports():
    patterns = ["/dev/ttyACM*", "/dev/ttyUSB*", "/dev/ttyS*", "/dev/ttyAMA*"]
    ports = []
    for pattern in patterns:
        ports.extend(glob.glob(pattern))
    return sorted(set(ports))

def send_line(bt_ser, text):
    bt_ser.write((text + "\r\n").encode())
    bt_ser.flush()
    log(f"Sent to Bluetooth: {text}")

def main():
    listener = None
    bt_ser = None
    src_ser = None

    mode = "WAIT_CONNECT"
    selected_port = None
    buffer = b""
    banner_sent = False

    log("Script started")

    while True:
        try:
            if listener is None:
                log("Starting rfcomm listener on channel 1")
                listener = subprocess.Popen(
                    ["rfcomm", "listen", "hci0", "1"],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )

            if mode == "WAIT_CONNECT":
                if os.path.exists(RFCOMM_DEV):
                    log(f"{RFCOMM_DEV} detected")
                    bt_ser = serial.Serial(RFCOMM_DEV, RFCOMM_BAUD, timeout=0.1)
                    log("Bluetooth serial opened")
                    mode = "COMMAND"
                    buffer = b""
                    banner_sent = False
                else:
                    time.sleep(0.2)
                    continue

            if not os.path.exists(RFCOMM_DEV):
                log("RFCOMM disconnected")

                if bt_ser:
                    try:
                        bt_ser.close()
                        log("Closed Bluetooth serial")
                    except Exception:
                        pass
                    bt_ser = None

                if src_ser:
                    try:
                        src_ser.close()
                        log("Closed source serial")
                    except Exception:
                        pass
                    src_ser = None

                subprocess.run(
                    ["rfcomm", "release", "0"],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL
                )
                log("Released rfcomm0")

                mode = "WAIT_CONNECT"
                selected_port = None
                buffer = b""
                banner_sent = False
                time.sleep(1)
                continue

            if mode == "COMMAND":
                if not banner_sent:
                    log("Entering COMMAND mode")
                    send_line(bt_ser, "Bluetooth console online")
                    send_line(bt_ser, "Commands:")
                    send_line(bt_ser, "  listcom")
                    send_line(bt_ser, "  select /dev/ttyACM*")
                    send_line(bt_ser, "  shutdown")
                    send_line(bt_ser, "  During streaming, press z to stop stream")
                    banner_sent = True

                ch = bt_ser.read(1)
                if not ch:
                    continue

                if ch in (b"\n", b"\r"):
                    cmd = buffer.decode(errors="ignore").strip()
                    buffer = b""

                    if not cmd:
                        continue

                    log(f"Received command: {cmd}")

                    if cmd == "listcom":
                        ports = list_com_ports()
                        log(f"Detected ports: {ports}")
                        if ports:
                            for p in ports:
                                send_line(bt_ser, p)
                        else:
                            send_line(bt_ser, "No COM ports found")
                            
                    elif cmd == "shutdown":
                        send_line(bt_ser, "Shutting down now")
                        log("Shutdown command received")
                        subprocess.Popen(["shutdown", "now"])
                        break

                    elif cmd.startswith("select "):
                        parts = cmd.split(maxsplit=1)
                        port_name = parts[1].strip()

                        log(f"Select requested for port: {port_name}")

                        if not os.path.exists(port_name):
                            send_line(bt_ser, f"Port not found: {port_name}")
                            log(f"Port not found: {port_name}")
                            continue

                        try:
                            if src_ser:
                                src_ser.close()
                                log("Closed previous source serial")

                            src_ser = serial.Serial(port_name, SERIAL_BAUD, timeout=0.1)
                            selected_port = port_name
                            log(f"Opened source serial: {selected_port} @ {SERIAL_BAUD}")
                            send_line(bt_ser, f"Connected to {selected_port} at {SERIAL_BAUD}")
                            send_line(bt_ser, "Streaming started - press z to stop")
                            mode = "STREAM"

                        except Exception as e:
                            send_line(bt_ser, f"Error opening {port_name}: {e}")
                            log(f"Error opening source serial: {e}")

                    else:
                        send_line(bt_ser, f"Unknown command: {cmd}")
                        log(f"Unknown command: {cmd}")

                else:
                    buffer += ch

            elif mode == "STREAM":
                if src_ser is None:
                    log("STREAM mode but source serial is None, returning to COMMAND")
                    mode = "COMMAND"
                    continue
                #1. Check for control input (non-blocking)
                if bt_ser.in_waiting > 0:
                    ch = bt_ser.read(1)
                    
                    if ch == b"z":
                        log("Stop command received (z)")
                        send_line(bt_ser, "")
                        send_line(bt_ser, "Streaming stopped")
                        send_line(bt_ser, "Command mode")

                        try:
                            src_ser.close()
                        except Exception:
                            pass

                        src_ser = None
                        selected_port = None
                        mode = "COMMAND"
                        continue
                
                # 2. Forward serial data
                data = src_ser.read(256)
                if data:
                    log(f"Forwarding {len(data)} bytes from {selected_port} to Bluetooth")
                    bt_ser.write(data)
                    bt_ser.flush()

        except Exception as e:
            log(f"Main loop exception: {e}")

            try:
                if bt_ser:
                    send_line(bt_ser, f"Error: {e}")
            except Exception:
                pass

            try:
                if bt_ser:
                    bt_ser.close()
                    log("Closed Bluetooth serial after exception")
            except Exception:
                pass
            bt_ser = None

            try:
                if src_ser:
                    src_ser.close()
                    log("Closed source serial after exception")
            except Exception:
                pass
            src_ser = None

            subprocess.run(
                ["rfcomm", "release", "0"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )
            log("Released rfcomm0 after exception")

            mode = "WAIT_CONNECT"
            selected_port = None
            buffer = b""
            banner_sent = False
            time.sleep(1)

if __name__ == "__main__":
    main()