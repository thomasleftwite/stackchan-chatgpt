"""Read only bounded, non-sensitive diagnostic status. Never record audio/raw logs."""
import argparse
import re
import time
import serial

ALLOWED = re.compile(r"PROBE (?:READY v=1 input=16000 output=24000 mono=1 bits=16|MEMORY FAIL|TONE (?:PASS rate=24000 samples=24000|FAIL)|CAPTURE (?:FAIL|DONE rate=16000 samples=48000 ms=\d{1,6} rms=\d{1,5} peak=\d{1,5} clipped=\d{1,5})|PLAYBACK (?:PASS rate=24000 samples=72000|FAIL))")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', required=True)
    parser.add_argument('--seconds', type=int, default=20)
    parser.add_argument('--tone', action='store_true')
    args = parser.parse_args()
    deadline = time.monotonic() + min(max(args.seconds, 1), 55)
    seen = 0
    with serial.Serial(args.port, 115200, timeout=0.2) as connection:
        connection.reset_input_buffer()
        connection.write(b'PROBE INFO\n')
        pending_tone = args.tone
        while time.monotonic() < deadline:
            raw = connection.read_until(b'\n', 200)
            line = raw.decode('ascii', errors='replace').strip()
            if ALLOWED.fullmatch(line):
                print(line, flush=True)
                seen += 1
                if pending_tone and line.startswith('PROBE READY '):
                    connection.write(b'PROBE TONE\n')
                    pending_tone = False
        print(f'Approved diagnostic statuses: {seen}', flush=True)


if __name__ == '__main__':
    main()
