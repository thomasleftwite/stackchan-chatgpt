"""Install the reviewed initial layout after explicit consent to discard old data.

Only the new NVS and the five listed image ranges are changed. Raw device logs
and device identifiers are neither printed nor saved. No API keys are provisioned.
"""
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys


def run(esptool, port, after, operation):
    command = [sys.executable, str(esptool), "--chip", "esp32s3", "--port", port,
               "--baud", "460800", "--before", "no_reset", "--after", after] + operation
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    for line in result.stdout.splitlines():
        if line.startswith(("Chip is ", "Features:", "Flash will be erased from", "Wrote ",
                            "Hash of data verified", "Hard resetting", "Chip erase completed",
                            "Erase completed", "A fatal error occurred:")):
            print(line, flush=True)
    if result.returncode:
        raise SystemExit(result.returncode)
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--esptool", type=Path, required=True)
    parser.add_argument("--boot-app", type=Path, required=True)
    parser.add_argument("--confirm-discard-old-data", action="store_true", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = root / ".pio/build/atoms3r"
    images = [
        (0x0000, build / "bootloader.bin", "1776e4dd896a69d0a5c2e79957b0e2a88aa4129b1381d6478683515a1f6af343", 0x8000),
        (0x8000, build / "partitions.bin", "a67752ec6219ff63f32ee2416bfa0005ed34d0e6dba59a36eba21f6c666319c5", 0x1000),
        (0xE000, args.boot_app, "f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0", 0x2000),
        (0x10000, build / "firmware.bin", "8f59313c8000835103c71e4427eec15682b63da04f9f3aee83535c056f3b9042", 0x300000),
        (0x610000, build / "spiffs.bin", "8f1304f122703a23d346844f4834708f792b9e0ab5412c37f6b67557f862e741", 0x1F0000),
    ]
    # Validate every artifact before the first erase; do not rebuild mid-migration.
    for address, image, digest, maximum in images:
        content = image.read_bytes()
        if not content or len(content) > maximum:
            raise SystemExit(f"Invalid image size at {address:#x}")
        if digest and hashlib.sha256(content).hexdigest() != digest:
            raise SystemExit(f"Image changed at {address:#x}; review the new artifact first")
        print(f"Reviewed image: offset={address:#x}, bytes={len(content)}", flush=True)
    print("Initialize new NVS: 0x9000..0xDFFF", flush=True)
    run(args.esptool, args.port, "no_reset", ["erase_region", "0x9000", "0x5000"])
    operation = ["write_flash", "--flash_mode", "keep", "--flash_freq", "keep", "--flash_size", "8MB"]
    for address, image, _, _ in images:
        operation.extend([hex(address), str(image)])
    result = run(args.esptool, args.port, "hard_reset", operation)
    verified = result.count("Hash of data verified")
    if verified != len(images):
        raise SystemExit(f"Expected {len(images)} image hash confirmations; received {verified}")
    print(f"Migration image hashes verified: {verified}/{len(images)}", flush=True)


if __name__ == "__main__":
    main()
