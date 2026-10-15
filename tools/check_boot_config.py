"""Reset and check configuration boot with an exact, secret-free verdict.

Exit 0: reached Wi-Fi setup/idle. Exit 1: known config failure. Exit 2: no verdict.
"""
import argparse
import time
import serial


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--manual-reset", action="store_true", help="Arm observer; press reset once on the device")
    args = parser.parse_args()
    connection = serial.Serial(args.port, 115200, timeout=0.2)
    connection.reset_input_buffer()
    # esptool 4.5.1 HardReset(uses_usb=True) sequence.
    if not args.manual_reset:
        connection.setRTS(True)
        time.sleep(0.2)
        connection.setRTS(False)
        time.sleep(0.2)
    print("Boot configuration check armed", flush=True)
    deadline = time.monotonic() + (40 if args.manual_reset else 20)
    try:
        while time.monotonic() < deadline:
            try:
                line = connection.readline().decode("utf-8", errors="replace").strip()
            except serial.SerialException:
                connection.close()
                time.sleep(0.3)
                connection = serial.Serial(args.port, 115200, timeout=0.2)
                continue
            if "waiting for download" in line.lower() or "DOWNLOAD(USB" in line:
                print("INCONCLUSIVE: device remained in download mode")
                return 2
            if "Guru Meditation Error" in line or "Brownout detector" in line:
                print("FAIL: boot fault (raw line suppressed)")
                return 1
            if line in {"Wi-Fi接続中", "待機中"}:
                print("PASS: configuration loaded; reached Wi-Fi/idle")
                return 0
            if line in {"Invalid secrets.json", "Invalid config.yaml/schema", "Invalid audio limits"}:
                print("FAIL: " + line)
                return 1
    finally:
        connection.close()
    print("INCONCLUSIVE: no approved boot verdict received")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
