"""Wireless host lifecycle and DPAPI-protected device enrollment. No USB imports."""
import base64
import json
import hashlib
import shutil
import os
from pathlib import Path
import queue
import re
import subprocess
import threading
from settings import crypt

PORT=48443

def normalize_code(value):
    value=re.sub(r'\s','',value).lower()
    if not re.fullmatch('[0-9a-f]{32}',value): raise ValueError('INVALID_PAIR_CODE')
    return value

def save_pairing(path,code):
    code=normalize_code(code);path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    data={'version':1,'protectedDeviceKey':base64.b64encode(crypt(code.encode())).decode('ascii')}
    temp=path.with_suffix('.new');temp.write_text(json.dumps(data),encoding='utf-8');os.replace(temp,path)

def load_pairing(path):
    path=Path(path)
    if not path.exists(): return ''
    if path.stat().st_size>4096: raise ValueError('PAIRING_TOO_LARGE')
    data=json.loads(path.read_text(encoding='utf-8'))
    if data.get('version')!=1: raise ValueError('INVALID_PAIRING')
    return normalize_code(crypt(base64.b64decode(data['protectedDeviceKey'],validate=True),True).decode('ascii'))

def runtime_node(runtime, destination=None):
    # A stable, verified executable path lets the firewall restrict this runtime
    # across one-file EXE extractions. Do not reuse other installations' node.exe.
    source=Path(runtime)/'node.exe'
    digest=hashlib.sha256(source.read_bytes()).hexdigest()
    root=Path(destination) if destination else Path(os.environ['LOCALAPPDATA'])/'StackchanHost'/'runtime'
    target=root/digest/'node.exe';target.parent.mkdir(parents=True,exist_ok=True)
    if not target.is_file() or hashlib.sha256(target.read_bytes()).hexdigest()!=digest:
        temp=target.with_suffix('.new');shutil.copyfile(source,temp);os.replace(temp,target)
    return target

def run_wireless(runtime,node,host,code,env,stop_event,on_status):
    # Only the selected LAN is bound. Parent stdin carries the device credential.
    from network import local_addresses
    if host not in local_addresses():
        on_status('WIRELESS stale_address');return 1
    code=normalize_code(code)
    child=subprocess.Popen([str(node),str(Path(runtime)/'server/dist/wireless-host.js')],cwd=Path(runtime)/'server',
        stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True,env=env,
        creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    events=queue.Queue(maxsize=64)
    def read():
        for line in child.stdout:
            try: events.put_nowait(line)
            except queue.Full: pass
    threading.Thread(target=read,daemon=True).start()
    try:
        child.stdin.write(json.dumps({'host':host,'port':PORT,'keyHex':code})+'\n');child.stdin.flush()
        ready=False
        while not stop_event.wait(.1):
            if child.poll() is not None:
                on_status('WIRELESS failed');return 1
            try: event=json.loads(events.get(timeout=.1))
            except queue.Empty: continue
            except (ValueError,TypeError): continue
            kind=event.get('event') if isinstance(event,dict) else None
            if kind in ('ready','device-online','turn-complete','turn-failed','failed'):
                on_status('WIRELESS '+kind)
                if kind=='failed':return 1
                if kind=='ready':ready=True
        return 0 if ready else 1
    finally:
        if child.poll() is None:
            try:child.stdin.close();child.wait(timeout=5)
            except (OSError,subprocess.TimeoutExpired):child.terminate();child.wait(timeout=5)
        on_status('WIRELESS stopped')
