"""R01: bounded Native Wi-Fi/AP diagnostic; never saves a PC profile.

Default: scan only. --connect-readonly: only with wired Internet, active hotspot,
one physical WLAN interface initially disconnected, and one target AP visible.
Temporarily connect, GET the form, then disconnect in finally. No USB or cloud API.
--provision-once additionally permits one POST with the active hotspot credentials.
All printed fields are allowlisted statuses/counts; never print exceptions.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
from html.parser import HTMLParser
import http.client
import json
import os
from pathlib import Path
import re
import subprocess
import time
import uuid
from urllib.parse import urlencode
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[1]
TARGET = b'Stackchan-Setup'


class ProbeError(Exception):
    pass


class Guid(C.Structure):
    _fields_ = [('raw', C.c_ubyte * 16)]


class Ssid(C.Structure):
    _fields_ = [('length', W.DWORD), ('data', C.c_ubyte * 32)]

    def value(self):
        if self.length > 32:
            raise ProbeError('INVALID_SSID_LENGTH')
        return bytes(self.data[:self.length])


class Interface(C.Structure):
    _fields_ = [('guid', Guid), ('description', W.WCHAR * 256), ('state', W.DWORD)]


class Network(C.Structure):
    _fields_ = [('profile', W.WCHAR * 256), ('ssid', Ssid), ('bssType', W.DWORD),
                ('bssids', W.DWORD), ('connectable', W.BOOL), ('reason', W.DWORD),
                ('phyCount', W.DWORD), ('phys', W.DWORD * 8), ('morePhys', W.BOOL),
                ('quality', W.DWORD), ('secure', W.BOOL), ('auth', W.DWORD),
                ('cipher', W.DWORD), ('flags', W.DWORD), ('reserved', W.DWORD)]


class Association(C.Structure):
    _fields_ = [('ssid', Ssid), ('bssType', W.DWORD), ('bssid', C.c_ubyte * 6),
                ('phyType', W.DWORD), ('phyIndex', W.DWORD), ('quality', W.DWORD),
                ('rxRate', W.DWORD), ('txRate', W.DWORD)]


class Security(C.Structure):
    _fields_ = [('enabled', W.BOOL), ('oneX', W.BOOL), ('auth', W.DWORD), ('cipher', W.DWORD)]


class Connection(C.Structure):
    _fields_ = [('state', W.DWORD), ('mode', W.DWORD), ('profile', W.WCHAR * 256),
                ('association', Association), ('security', Security)]


class Parameters(C.Structure):
    _fields_ = [('mode', W.DWORD), ('profile', W.LPCWSTR), ('ssid', C.c_void_p),
                ('bssids', C.c_void_p), ('bssType', W.DWORD), ('flags', W.DWORD)]


class Wlan:
    def __init__(self):
        self.api = C.WinDLL('wlanapi.dll')
        p = C.c_void_p
        self.api.WlanOpenHandle.argtypes = [W.DWORD, p, C.POINTER(W.DWORD), C.POINTER(W.HANDLE)]
        self.api.WlanCloseHandle.argtypes = [W.HANDLE, p]
        self.api.WlanEnumInterfaces.argtypes = [W.HANDLE, p, C.POINTER(p)]
        self.api.WlanGetAvailableNetworkList.argtypes = [W.HANDLE, C.POINTER(Guid), W.DWORD, p, C.POINTER(p)]
        self.api.WlanScan.argtypes = [W.HANDLE, C.POINTER(Guid), p, p, p]
        self.api.WlanQueryInterface.argtypes = [W.HANDLE, C.POINTER(Guid), W.DWORD, p, C.POINTER(W.DWORD), C.POINTER(p), C.POINTER(W.DWORD)]
        self.api.WlanConnect.argtypes = [W.HANDLE, C.POINTER(Guid), C.POINTER(Parameters), p]
        self.api.WlanDisconnect.argtypes = [W.HANDLE, C.POINTER(Guid), p]
        self.api.WlanFreeMemory.argtypes = [p]
        self.api.WlanFreeMemory.restype = None
        for name in ('WlanOpenHandle', 'WlanCloseHandle', 'WlanEnumInterfaces',
                     'WlanGetAvailableNetworkList', 'WlanScan', 'WlanQueryInterface',
                     'WlanConnect', 'WlanDisconnect'):
            getattr(self.api, name).restype = W.DWORD
        self.handle = W.HANDLE()
        version = W.DWORD()
        self.check(self.api.WlanOpenHandle(2, None, C.byref(version), C.byref(self.handle)), 'OPEN')

    @staticmethod
    def check(code, operation):
        if code:
            raise ProbeError(f'{operation}_{int(code)}')

    def interfaces(self):
        ptr = C.c_void_p()
        self.check(self.api.WlanEnumInterfaces(self.handle, None, C.byref(ptr)), 'ENUM')
        try:
            count = W.DWORD.from_address(ptr.value).value
            if count > 64:
                raise ProbeError('INTERFACE_COUNT')
            return [Interface.from_buffer_copy(C.string_at(ptr.value + 8 + i * C.sizeof(Interface), C.sizeof(Interface))) for i in range(count)]
        finally:
            self.api.WlanFreeMemory(ptr)

    def networks(self, guid):
        ptr = C.c_void_p()
        self.check(self.api.WlanGetAvailableNetworkList(self.handle, C.byref(guid), 0, None, C.byref(ptr)), 'NETWORK_LIST')
        try:
            count = W.DWORD.from_address(ptr.value).value
            if count > 4096:
                raise ProbeError('NETWORK_COUNT')
            return [Network.from_buffer_copy(C.string_at(ptr.value + 8 + i * C.sizeof(Network), C.sizeof(Network))) for i in range(count)]
        finally:
            self.api.WlanFreeMemory(ptr)

    def connection(self, guid):
        ptr = C.c_void_p()
        size, kind = W.DWORD(), W.DWORD()
        code = self.api.WlanQueryInterface(self.handle, C.byref(guid), 7, None, C.byref(size), C.byref(ptr), C.byref(kind))
        if code == 5023:  # ERROR_INVALID_STATE: disconnected.
            return None
        self.check(code, 'QUERY')
        try:
            if size.value < C.sizeof(Connection):
                raise ProbeError('CONNECTION_SIZE')
            return Connection.from_buffer_copy(C.string_at(ptr.value, C.sizeof(Connection)))
        finally:
            self.api.WlanFreeMemory(ptr)

    def connect(self, guid, profile):
        params = Parameters(1, profile, None, None, 1, 0)
        self.check(self.api.WlanConnect(self.handle, C.byref(guid), C.byref(params), None), 'CONNECT')

    def disconnect(self, guid):
        self.check(self.api.WlanDisconnect(self.handle, C.byref(guid), None), 'DISCONNECT')

    def close(self):
        self.api.WlanCloseHandle(self.handle, None)


def powershell(script=None):
    shell = Path(os.environ['SystemRoot']) / 'System32/WindowsPowerShell/v1.0/powershell.exe'
    command = ['-Command', script] if script else ['-File', str(ROOT / 'tools/probe_windows_provisioning.ps1')]
    result = subprocess.run([str(shell), '-NoProfile', '-NonInteractive', *command],
                            capture_output=True, text=True, encoding='utf-8', timeout=15,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    if result.returncode:
        raise ProbeError('POWERSHELL_FAILED')
    return json.loads(result.stdout)


def hotspot_snapshot():
    data = powershell()
    profiles = data.get('profiles', [])
    profile = profiles[0] if len(profiles) == 1 else {}
    return {'wiredInternet': profile.get('wifi') is False and profile.get('connectivity') == 'InternetAccess',
            'hotspotOn': profile.get('tetheringState') == 'On',
            'twoPointFourGHz': profile.get('band') == 'TwoPointFourGigahertz',
            'authenticationKind': profile.get('authenticationKind'),
            'credentialsReadable': profile.get('ssidReadable') is True and profile.get('passphraseReadable') is True,
            'clientCount': profile.get('clientCount')}


def local_source(guid):
    # IDs and IPs stay inside the subprocess capture/Python memory, never printed.
    script = """[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new();
$items=@([Net.NetworkInformation.NetworkInterface]::GetAllNetworkInterfaces() | ForEach-Object {
$n=$_; $n.GetIPProperties().UnicastAddresses | Where-Object {$_.Address.AddressFamily -eq 'InterNetwork'} |
ForEach-Object {@{id=$n.Id;ip=$_.Address.IPAddressToString;mask=$_.IPv4Mask.IPAddressToString}}});
ConvertTo-Json -Compress -InputObject $items"""
    expected = uuid.UUID(bytes_le=bytes(guid.raw))
    candidates = [item['ip'] for item in powershell(script)
                  if uuid.UUID(item['id'].strip('{}')) == expected
                  and item['mask'] == '255.255.255.0'
                  and item['ip'].startswith('192.168.4.') and item['ip'] != '192.168.4.1']
    return candidates[0] if len(candidates) == 1 else None


class WifiForm(HTMLParser):
    def __init__(self, expected_ssid=None):
        super().__init__()
        self.active = False
        self.fields = set()
        self.valid = False
        self.expected_ssid = expected_ssid
        self.target_seen = False
        self.saved_ssid_matches = False

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if self.expected_ssid is not None:
            self.target_seen |= attrs.get('data-ssid') == self.expected_ssid
            if tag == 'input' and attrs.get('name') == 's':
                self.saved_ssid_matches |= attrs.get('placeholder') == self.expected_ssid or attrs.get('value') == self.expected_ssid
        if tag == 'form':
            # GET /wifi resolves either fixed spelling to the same local path.
            self.active = attrs.get('method', '').lower() == 'post' and attrs.get('action') in ('/wifisave', 'wifisave')
            self.fields = set()
        if tag == 'input' and self.active:
            self.fields.add(attrs.get('name'))

    def handle_endtag(self, tag):
        if tag == 'form':
            self.valid |= self.active and {'s', 'p'} <= self.fields
            self.active = False


def read_form(source, expected_ssid=None):
    conn = http.client.HTTPConnection('192.168.4.1', timeout=4, source_address=(source, 0))
    try:
        conn.request('GET', '/wifi', headers={'Connection': 'close'})
        response = conn.getresponse()
        if response.status != 200 or 'text/html' not in response.getheader('Content-Type', '').lower():
            return {'httpStatus': response.status, 'wifiFormVerified': False}
        body = response.read(131073)
        if len(body) > 131072:
            return {'httpStatus': response.status, 'wifiFormVerified': False, 'bodyWithinLimit': False}
        text = body.decode('utf-8', errors='replace')
        form = WifiForm(expected_ssid)
        form.feed(text)
        return {'httpStatus': response.status, 'wifiFormVerified': form.valid, 'bodyWithinLimit': True,
                'targetHotspotInDeviceScan': form.target_seen,
                'savedSsidMatchesHotspot': form.saved_ssid_matches,
                'deviceReportsAuthFailure': 'Authentication failure' in text,
                'deviceReportsApNotFound': 'AP not found' in text,
                'deviceReportsConnectFailed': 'Could not connect' in text}
    finally:
        conn.close()


def temporary_profile():
    # Existing public setup credential; do not duplicate it in probe files/logs.
    source = (ROOT / 'src/LocalGemini.cpp').read_text(encoding='utf-8')
    match = re.search(r'char password\[\]\s*=\s*"([^"]+)"', source)
    if not match:
        raise ProbeError('SETUP_CREDENTIAL_NOT_FOUND')
    return f'''<WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1">
<name>Stackchan-Temporary-Probe</name><SSIDConfig><SSID><hex>{TARGET.hex()}</hex></SSID></SSIDConfig>
<connectionType>ESS</connectionType><connectionMode>manual</connectionMode><MSM><security>
<authEncryption><authentication>WPA2PSK</authentication><encryption>AES</encryption><useOneX>false</useOneX></authEncryption>
<sharedKey><keyType>passPhrase</keyType><protected>false</protected><keyMaterial>{escape(match[1])}</keyMaterial></sharedKey>
</security></MSM></WLANProfile>'''


def hotspot_credentials(include_password=True):
    # Captured subprocess stdout is private IPC, never forwarded to the report.
    script = """$ErrorActionPreference='Stop'; [Console]::OutputEncoding=[System.Text.UTF8Encoding]::new();
$null=[Windows.Networking.Connectivity.NetworkInformation,Windows.Networking.Connectivity,ContentType=WindowsRuntime];
$null=[Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager,Windows.Networking.NetworkOperators,ContentType=WindowsRuntime];
$p=[Windows.Networking.Connectivity.NetworkInformation]::GetInternetConnectionProfile();
if($null -eq $p -or $p.IsWlanConnectionProfile -or $p.GetNetworkConnectivityLevel().ToString() -ne 'InternetAccess'){exit 2};
$m=[Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager]::CreateFromConnectionProfile($p);
if($m.TetheringOperationalState.ToString() -ne 'On'){exit 3};
$c=$m.GetCurrentAccessPointConfiguration(); if($c.Band.ToString() -ne 'TwoPointFourGigahertz'){exit 4};
@{ssid=$c.Ssid;password=$c.Passphrase} | ConvertTo-Json -Compress"""
    if not include_password:
        script = script.replace('password=$c.Passphrase', "password=''" )
    credentials = powershell(script)
    ssid, password = credentials.get('ssid'), credentials.get('password')
    if not isinstance(ssid, str) or not 1 <= len(ssid.encode('utf-8')) <= 32 or ssid == TARGET.decode():
        raise ProbeError('INVALID_HOTSPOT_SSID')
    if include_password and (not isinstance(password, str) or not 8 <= len(password) <= 63 or any(not 32 <= ord(c) <= 126 for c in password)):
        raise ProbeError('INVALID_HOTSPOT_PASSWORD')
    return credentials


def send_configuration(source, credentials, report):
    conn = http.client.HTTPConnection('192.168.4.1', timeout=5, source_address=(source, 0))
    payload = urlencode({'s': credentials['ssid'], 'p': credentials['password']}).encode('ascii')
    report['configurationAttempted'] = True
    try:
        # Exactly one POST; timeout after sending is ambiguous and never retried.
        conn.request('POST', '/wifisave', body=payload,
                     headers={'Content-Type': 'application/x-www-form-urlencoded', 'Connection': 'close'})
        report['configurationSent'] = True
        response = conn.getresponse()
        report['configurationHttpStatus'] = response.status
        body = response.read(131073)
        report['configurationAccepted'] = response.status == 200 and b'Saving Credentials' in body
    except (OSError, http.client.HTTPException):
        report['configurationResponseUnknown'] = True
    finally:
        conn.close()


def run(connect=False, provision=False):
    if os.name != 'nt':
        raise ProbeError('WINDOWS_REQUIRED')
    if (C.sizeof(Interface), C.sizeof(Network), C.sizeof(Connection), C.sizeof(Parameters)) != (532, 628, 604, 40):
        raise ProbeError('NATIVE_LAYOUT_MISMATCH')
    if provision and not connect:
        raise ProbeError('PROVISION_REQUIRES_CONNECTION')
    report = {'probeVersion': 'R01', 'mode': 'provision-once' if provision else 'connect-readonly' if connect else 'scan-only',
              'configurationSent': False, 'profileSaved': False}
    wlan = Wlan()
    try:
        interfaces = [i for i in wlan.interfaces() if 'Wi-Fi Direct' not in i.description]
        report['physicalWlanCount'] = len(interfaces)
        if len(interfaces) != 1:
            raise ProbeError('REQUIRE_ONE_WLAN')
        interface = interfaces[0]
        report['initiallyDisconnected'] = interface.state == 4
        wlan.check(wlan.api.WlanScan(wlan.handle, C.byref(interface.guid), None, None, None), 'SCAN')
        time.sleep(4)
        targets = [n for n in wlan.networks(interface.guid) if n.ssid.value() == TARGET]
        report['targetEntryCount'] = len(targets)
        report['targetBssidCount'] = max((n.bssids for n in targets), default=0)
        report['targetConnectable'] = any(n.connectable for n in targets)
        if not connect:
            return report
        if interface.state != 4 or wlan.connection(interface.guid) is not None:
            raise ProbeError('REQUIRE_INITIALLY_DISCONNECTED')
        if not targets or report['targetBssidCount'] != 1:
            raise ProbeError('REQUIRE_SINGLE_TARGET_AP')
        if any(n.bssType != 1 or not n.secure or n.auth != 7 or n.cipher != 4 for n in targets):
            raise ProbeError('REQUIRE_WPA2_PSK_AES_TARGET')
        before = hotspot_snapshot()
        report['before'] = before
        if not all(before.get(key) is True for key in ('wiredInternet', 'hotspotOn', 'twoPointFourGHz', 'credentialsReadable')):
            raise ProbeError('PREFLIGHT_NOT_READY')
        credentials = hotspot_credentials(include_password=provision)
        # Set cleanup obligation before submitting the asynchronous request.
        try:
            wlan.connect(interface.guid, temporary_profile())
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                current = wlan.connection(interface.guid)
                if current and current.state == 1 and current.association.ssid.value() == TARGET:
                    report['associatedWithTarget'] = True
                    break
                time.sleep(.5)
            else:
                raise ProbeError('ASSOCIATION_TIMEOUT')
            during = hotspot_snapshot()
            report['during'] = during
            if not during['hotspotOn']:
                raise ProbeError('HOTSPOT_STOPPED_DURING_AP')
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                source = local_source(interface.guid)
                if source:
                    report['apIpv4Acquired'] = True
                    report.update(read_form(source, credentials['ssid']))
                    if provision:
                        if not report.get('wifiFormVerified'):
                            raise ProbeError('WIFI_FORM_NOT_VERIFIED')
                        current = wlan.connection(interface.guid)
                        if current is None or current.state != 1 or current.association.ssid.value() != TARGET:
                            raise ProbeError('TARGET_CONNECTION_LOST')
                        if hotspot_credentials() != credentials:
                            raise ProbeError('HOTSPOT_CONFIGURATION_CHANGED')
                        send_configuration(source, credentials, report)
                        # WiFiManager delays 2 seconds after replying, then has a
                        # 15-second connect timeout. Keep radio topology stable.
                        report['postSettleSeconds'] = 20
                        time.sleep(20)
                    break
                time.sleep(1)
            else:
                raise ProbeError('AP_DHCP_TIMEOUT')
        finally:
            try:
                wlan.disconnect(interface.guid)
                deadline = time.monotonic() + 10
                while time.monotonic() < deadline:
                    if any(bytes(i.guid.raw) == bytes(interface.guid.raw) and i.state == 4 for i in wlan.interfaces()):
                        report['restoredDisconnected'] = True
                        break
                    time.sleep(.5)
                report.setdefault('restoredDisconnected', False)
            except ProbeError as error:
                report['restoreError'] = str(error)
            report['after'] = hotspot_snapshot()
        if provision:
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline:
                state = hotspot_snapshot()
                report['after'] = state
                if state['hotspotOn'] and state['wiredInternet'] and state.get('clientCount', 0) > before.get('clientCount', 0):
                    report['hotspotClientCountIncreased'] = True
                    break
                time.sleep(2)
            report.setdefault('hotspotClientCountIncreased', False)
            report['hotspotCredentialsUnchanged'] = hotspot_credentials() == credentials
            credentials = None
        return report
    except ProbeError as error:
        report['error'] = str(error)
        return report
    except Exception:
        report['error'] = 'PROBE_FAILED'
        return report
    finally:
        wlan.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--connect-readonly', action='store_true')
    parser.add_argument('--provision-once', action='store_true', help='Explicitly authorize one WiFiManager POST using the active hotspot credentials.')
    args = parser.parse_args()
    try:
        result = run(args.connect_readonly or args.provision_once, args.provision_once)
    except ProbeError as error:
        result = {'error': str(error), 'configurationSent': False}
    except Exception as error:
        result = {'error': 'PROBE_START_FAILED', 'errorType': type(error).__name__,
                  'winerror': getattr(error, 'winerror', None), 'configurationSent': False}
    print(json.dumps(result, ensure_ascii=True))
    ok = 'error' not in result
    if args.connect_readonly or args.provision_once:
        ok = ok and result.get('wifiFormVerified') is True and result.get('restoredDisconnected') is True
        ok = ok and result.get('after', {}).get('hotspotOn') is True and result.get('after', {}).get('wiredInternet') is True
    if args.provision_once:
        ok = ok and result.get('configurationAccepted') is True
        ok = ok and result.get('hotspotClientCountIncreased') is True and result.get('hotspotCredentialsUnchanged') is True
    raise SystemExit(0 if ok else 1)
