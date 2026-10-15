"""Observe only approved, non-sensitive startup status lines; never save raw logs."""
import argparse
import time
import serial

ALLOWED = {
    "PSRAMが必要です", "設定領域なし", "Upload Filesystem", "Imageを実行",
    "設定エラー", "Wi-Fi接続中", "Wi-Fi設定", "マイク初期化失敗", "音声検出停止",
    "待機中", "APIキー未設定", "Wi-Fi未接続", "呼びかけ未登録",
    "ca.pem missing/invalid", "CA memory exhausted", "CA read failed",
    "config.yaml missing/too large", "Invalid config.yaml/schema", "Invalid wake_word settings",
    "Invalid phrase/template path", "Invalid audio limits", "Invalid API settings",
    "Invalid secrets.json", "Invalid API key format",
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=int, default=20)
    args = parser.parse_args()
    deadline = time.monotonic() + min(max(args.seconds, 1), 40)
    seen = set()
    while time.monotonic() < deadline:
        try:
            with serial.Serial(args.port, 115200, timeout=0.2) as connection:
                print("Serial observer connected", flush=True)
                while time.monotonic() < deadline:
                    line = connection.readline().decode("utf-8", errors="replace").strip()
                    if line in ALLOWED and line not in seen:
                        print(line, flush=True)
                        seen.add(line)
                    if "Guru Meditation Error" in line or "Brownout detector" in line:
                        print("Boot fault detected (raw line suppressed)", flush=True)
        except serial.SerialException:
            time.sleep(0.3)
    print(f"Distinct approved status lines: {len(seen)}")


if __name__ == "__main__":
    main()
