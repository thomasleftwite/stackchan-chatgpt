"""Read only the ESP32-S3 partition table; suppress device identifiers.

The bootloader is left running (no application reset) for the subsequent upload.
No firmware, Wi-Fi settings, audio or API secrets are read or saved.
"""
import argparse
from pathlib import Path
import struct
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--esptool", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = root / ".pio" / "device-check" / "partitions-before.bin"
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [sys.executable, str(args.esptool), "--chip", "esp32s3", "--port", args.port,
               "--baud", "115200", "--before", "default_reset", "--after", "no_reset",
               "read_flash", "0x8000", "0xC00", str(output)]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    # esptool prints the device MAC; only emit selected non-identifying status.
    for line in result.stdout.splitlines():
        if line.startswith(("Chip is ", "Features:", "Crystal is ", "Read ", "A fatal error occurred:")):
            print(line)
    if result.returncode:
        raise SystemExit(result.returncode)
    data = output.read_bytes()
    for offset in range(0, len(data), 32):
        magic, kind, subtype, start, size, label, flags = struct.unpack_from("<HBBLL16sL", data, offset)
        if magic != 0x50AA:
            break
        name = label.rstrip(b"\0").decode("ascii", errors="replace")
        print(f"{name}: type={kind:#x} subtype={subtype:#x} offset={start:#x} size={size:#x} flags={flags:#x}")


if __name__ == "__main__":
    main()
