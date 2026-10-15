"""Observe one user-triggered local registration; no reset or raw audio capture.

Exit 0: saved. Exit 1: explicit device failure. Exit 2: no conclusive result.
Only exact status messages are emitted; network credentials are never printed.
"""
import argparse
import time
import serial

STATUS = {"登録します", "離してから", "呼びかけを", "話してください"}
FAILURES = {"登録タイムアウト", "登録保存に失敗", "音声が短すぎます",
            "マイク初期化失敗", "マイク読み取り失敗", "音声検出初期化失敗"}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=int, default=40)
    args = parser.parse_args()
    with serial.Serial(args.port, 115200, timeout=0.2) as connection:
        connection.reset_input_buffer()
        print("Registration check armed: use the screen button to register", flush=True)
        deadline = time.monotonic() + min(max(args.seconds, 10), 180)
        while time.monotonic() < deadline:
            line = connection.readline().decode("utf-8", errors="replace").strip()
            if line in STATUS:
                print(line, flush=True)
            elif line == "登録しました":
                print("PASS: local registration saved", flush=True)
                return 0
            elif line in FAILURES:
                print("FAIL: " + line, flush=True)
                return 1
    print("INCONCLUSIVE: no registration result received")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
