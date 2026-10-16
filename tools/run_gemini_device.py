"""Private-LAN/USB coordinator. Only allowlisted status/numbers leave this process."""
import argparse
import json
from pathlib import Path
import queue
import re
import secrets
import shutil
import subprocess
import sys
import threading
import time
import serial
from run_lan_probe import choose_host

ROOT = Path(__file__).resolve().parents[1]
READY = 'GEM READY v=2 input=16000 output=24000 mono=1 bits=16'
STATUS = re.compile(r'GEM (?:READY v=2 input=16000 output=24000 mono=1 bits=16|STATE configured=[01] wifi=[01] pc=[01]|START BLOCKED reason=(?:setup|wifi|pc|button)|PC OFFLINE|CONFIG OK|WIFI (?:CONNECTED|FAIL)|MEMORY FAIL|CAPTURE (?:NONE|FAIL|EMPTY(?: ms=\d{1,6} captured=\d{1,6})?|CANCEL|NOISY|DONE samples=\d{1,5} ms=\d{1,6} rms=\d{1,5} peak=\d{1,5} clipped=\d{1,5}(?: captured=\d{1,6} end=(?:silence|limit))?)|VAD CALIBRATED rms=\d{1,5} threshold=\d{1,5}|VADTEST (?:PASS samples=\d{1,5}|FAIL)|RESULT (?:NONE|(?:PASS|FAIL) gen=\d{1,10} input=\d{1,5} output=\d{1,6} frames=\d{1,3} ms=\d{1,6} played=[01])|FAULT code=\d{1,2})')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', required=True)
    parser.add_argument('--mode', choices=['fixture', 'gemini'], default='fixture')
    parser.add_argument('--host')
    parser.add_argument('--seconds', type=int, default=580)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    report = args.report.resolve() if args.report else None
    if report and (not report.is_relative_to((ROOT / '.pio').resolve()) or report.exists()):
        raise ValueError('New report under .pio required')
    host, token = choose_host(args.host), secrets.token_hex(32)
    node = shutil.which('node')
    if not node:
        raise ValueError('Node required')
    child = subprocess.Popen([node, str(ROOT / 'server/dist/local-gemini.js')], cwd=ROOT / 'server',
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                             text=True, bufsize=1)
    events, controls, statuses = queue.Queue(maxsize=64), queue.Queue(), []
    def read_server():
        for line in child.stdout:
            events.put(line)
    def read_control():
        for line in sys.stdin:
            if line.strip() == 'stop':
                controls.put('stop')
        controls.put('stop')
    threading.Thread(target=read_server, daemon=True).start()
    threading.Thread(target=read_control, daemon=True).start()
    try:
        child.stdin.write(json.dumps({'host':host, 'token':token, 'mode':args.mode}) + '\n'); child.stdin.flush()
        state = json.loads(events.get(timeout=20))
        if state.get('event') != 'ready' or state.get('preflight') is not True:
            raise ValueError('Server failed')
        port = state.get('port')
        if not isinstance(port, int) or not 1 <= port <= 65535:
            raise ValueError('Invalid port')
        print(f'Gemini device server ready; mode={args.mode}; missing/wrong auth rejected', flush=True)
        with serial.Serial(args.port, 115200, timeout=0.2) as device:
            device.reset_input_buffer(); device.write(b'GEM INFO\n')
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline:
                if device.read_until(b'\n', 200).decode('ascii', errors='replace').strip() == READY:
                    break
            else:
                raise ValueError('Wrong firmware')
            device.write(f'GEM CONFIG {host} {port} {token}\n'.encode('ascii'))
            deadline = time.monotonic() + min(max(args.seconds, 30), 580)
            configured, synthetic_sent, seen = False, False, set()
            queried = time.monotonic()
            while time.monotonic() < deadline and controls.empty():
                if child.poll() is not None:
                    raise ValueError('Server exited')
                while not events.empty():
                    event = json.loads(events.get_nowait())
                    if event.get('event') == 'api-result' and event.get('status') in ('PASS', 'FAIL'):
                        safe = {k:v for k,v in event.items() if k in ('inputSamples','outputSamples','elapsedMs') and isinstance(v,int) and 0 <= v <= 1000000}
                        safe.update(event='api-result', status=event['status']); print(json.dumps(safe), flush=True)
                        statuses.append(safe)
                line = device.read_until(b'\n', 200).decode('ascii', errors='replace').strip()
                if STATUS.fullmatch(line):
                    if line == 'GEM CONFIG OK': configured = True
                    if configured and line not in seen:
                        print(line, flush=True); statuses.append(line); seen.add(line)
                    if line == 'GEM WIFI FAIL': raise ValueError('WiFi failed')
                    if line == 'GEM WIFI CONNECTED' and args.mode == 'fixture' and not synthetic_sent:
                        device.write(b'GEM PROBE\n'); synthetic_sent = True
                    if line == 'GEM WIFI CONNECTED' and args.mode == 'gemini':
                        print('Ready for physical-button question; service lasts about 10 minutes; stop to finish', flush=True)
                if time.monotonic() - queried > 5:
                    device.write(b'GEM PING\n'); queried = time.monotonic()
            device.write(b'GEM STOP\n'); device.flush()
            stopped_until = time.monotonic() + 1
            while time.monotonic() < stopped_until:
                line = device.read_until(b'\n', 200).decode('ascii', errors='replace').strip()
                if line == 'GEM PC OFFLINE':
                    print(line, flush=True); statuses.append(line); break
    finally:
        if child.poll() is None:
            child.stdin.close()
            try: child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.terminate(); child.wait(timeout=5)
        if report:
            report.parent.mkdir(parents=True, exist_ok=True)
            with report.open('x', encoding='utf-8') as output:
                json.dump({'version':1, 'mode':args.mode, 'statuses':statuses}, output, indent=2)
        print('Temporary Gemini device server stopped; no audio/token saved', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError, queue.Empty, json.JSONDecodeError):
        print('Gemini device test failed (details suppressed)', flush=True)
        raise SystemExit(1)
