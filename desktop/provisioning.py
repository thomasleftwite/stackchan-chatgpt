"""One-click device setup with a finite, always-restored radio transaction."""
import ctypes as C
import os
import time
from network import hotspot_networks
from windows_wifi import (ProbeError, Wlan, Interface, Network, Connection, Parameters,
                          TARGET, hotspot_credentials, temporary_profile, local_source,
                          read_form, send_configuration)


class Cancelled(Exception):
    pass


MESSAGES = {
    'WIRED_REQUIRED': '初回の自動設定は、有線でインターネット接続したPCに対応しています。既に接続済みの端末はそのまま待ち受けます。',
    'HOTSPOT_REQUIRED': 'PCのモバイルホットスポットをオンにしてください。',
    'BAND_REQUIRED': 'PCホットスポットを2.4GHzにしてください。',
    'AUTH_REQUIRED': '初回の自動設定はWPA2のホットスポットに対応しています。',
    'WINDOWS_ACCESS': 'WindowsのWi-Fi情報を取得できません。位置情報・Wi-Fiのアクセス許可を確認してください。',
    'REQUIRE_ONE_WLAN': '初回の自動設定はWi-Fiアダプタ1台の構成に対応しています。',
    'REQUIRE_DISCONNECTED': 'PCのWi-Fiが使用中のため自動切替を見送りました。有線接続にし、PCのWi-Fiを未接続にして再実行してください。',
    'MULTIPLE_APS': '同名の設定APが複数あります。対象以外の端末を停止して再実行してください。',
    'AP_SECURITY': '設定APの接続方式を確認できません。対象端末を確認してください。',
    'ASSOCIATION_TIMEOUT': '設定APへ接続できませんでした。端末のWi-Fi設定画面を確認してください。',
    'AP_DHCP_TIMEOUT': '設定APのアドレスを取得できませんでした。',
    'WIFI_FORM_NOT_VERIFIED': '端末の設定画面を確認できないため、Wi-Fi情報を送信しませんでした。',
    'TARGET_CONNECTION_LOST': '設定APとの接続が変わったため、送信を中止しました。',
    'HOTSPOT_CHANGED': 'PCホットスポットの設定・アドレスが変わりました。停止し、再検出して接続し直してください。',
    'RESTORE_FAILED': 'PCのWi-Fiを元の未接続状態へ戻せませんでした。WindowsのWi-Fi状態を確認してください。',
    'SELECT_HOTSPOT': 'PCホットスポットの候補が複数あります。使用するアドレスを選んでください。',
}


def setup_message(code):
    if code.endswith('_5'):
        code = 'WINDOWS_ACCESS'
    return MESSAGES.get(code, '自動設定を完了できませんでした。PCと端末のWi-Fi状態を確認してください。設定を自動再送せず待ち受けを続けます。')


def select_hotspot(selected, networks):
    if not networks:
        raise ProbeError('HOTSPOT_REQUIRED')
    if len(networks) == 1:
        return next(iter(networks))
    if selected in networks:
        return selected
    raise ProbeError('SELECT_HOTSPOT')


class WindowsSetup:
    """Windows adapter restricted to the successfully probed wired-PC topology."""
    def __init__(self):
        if os.name != 'nt' or (C.sizeof(Interface), C.sizeof(Network), C.sizeof(Connection), C.sizeof(Parameters)) != (532, 628, 604, 40):
            raise ProbeError('WINDOWS_ACCESS')
        self.wlan = Wlan()
        self.interface = None
        self.credentials = None
        self.touched = False

    def prepare(self, host, cancel):
        # Read only. All conditions are established before changing the radio.
        networks = hotspot_networks()
        if len(networks) != 1 or host not in networks:
            raise ProbeError('HOTSPOT_CHANGED')
        self.credentials = hotspot_credentials()
        interfaces = [i for i in self.wlan.interfaces() if 'Wi-Fi Direct' not in i.description]
        if len(interfaces) != 1:
            raise ProbeError('REQUIRE_ONE_WLAN')
        self.interface = interfaces[0]
        if self.interface.state != 4 or self.wlan.connection(self.interface.guid) is not None:
            raise ProbeError('REQUIRE_DISCONNECTED')
        self.wlan.check(self.wlan.api.WlanScan(self.wlan.handle, C.byref(self.interface.guid), None, None, None), 'SCAN')
        if cancel.wait(4):
            raise Cancelled()
        targets = [n for n in self.wlan.networks(self.interface.guid) if n.ssid.value() == TARGET]
        if not targets:
            return False
        if max(n.bssids for n in targets) != 1:
            raise ProbeError('MULTIPLE_APS')
        if any(n.bssType != 1 or not n.secure or n.auth != 7 or n.cipher != 4 or not n.connectable for n in targets):
            raise ProbeError('AP_SECURITY')
        return True

    def connect(self, cancel):
        if self.wlan.connection(self.interface.guid) is not None:
            raise ProbeError('REQUIRE_DISCONNECTED')
        self.touched = True  # Cleanup also applies to a failed async request.
        self.wlan.connect(self.interface.guid, temporary_profile())
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            current = self.wlan.connection(self.interface.guid)
            if current and current.state == 1 and current.association.ssid.value() == TARGET:
                return
            if cancel.wait(.5):
                raise Cancelled()
        raise ProbeError('ASSOCIATION_TIMEOUT')

    def form_source(self, cancel):
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            source = local_source(self.interface.guid)
            if source:
                if not read_form(source, self.credentials['ssid'])['wifiFormVerified']:
                    raise ProbeError('WIFI_FORM_NOT_VERIFIED')
                return source
            if cancel.wait(.5):
                raise Cancelled()
        raise ProbeError('AP_DHCP_TIMEOUT')

    def submit(self, source, cancel):
        current = self.wlan.connection(self.interface.guid)
        if not current or current.state != 1 or current.association.ssid.value() != TARGET:
            raise ProbeError('TARGET_CONNECTION_LOST')
        if hotspot_credentials() != self.credentials:
            raise ProbeError('HOTSPOT_CHANGED')
        if cancel.is_set():
            raise Cancelled()
        report = {}
        send_configuration(source, self.credentials, report, cancel)
        if report.get('configurationCancelled'):
            raise Cancelled()
        if not report.get('configurationAttempted'):
            raise ProbeError('SETUP_SEND_FAILED')
        return report.get('configurationAccepted') is True

    def settle(self):
        # Once submitted, cancellation cannot revoke the saved Wi-Fi settings.
        # Preserve the proven 2s portal delay + 15s association window before
        # changing the PC radio, including when Stop/Close was clicked.
        time.sleep(20)

    def restore(self):
        if not self.touched:
            return
        guid = self.interface.guid
        self.wlan.disconnect(guid)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if any(bytes(i.guid.raw) == bytes(guid.raw) and i.state == 4 for i in self.wlan.interfaces()):
                self.touched = False
                return
            time.sleep(.25)
        raise ProbeError('RESTORE_FAILED')

    def verify_after(self, host):
        if host not in hotspot_networks() or hotspot_credentials() != self.credentials:
            raise ProbeError('HOTSPOT_CHANGED')

    def close(self):
        if self.credentials:
            self.credentials.clear()
        self.wlan.close()


def provision_device(host, cancel, online, on_status, *, backend_factory=WindowsSetup):
    """Return configured/waiting/online/cancelled/failed; never retry a POST.

    Online is set only by the running host's remote-peer event, not by HTTP200,
    AP disappearance or a local health probe. Backend is the OS/HTTP boundary.
    """
    backend = None
    outcome = 'failed'
    try:
        if cancel.is_set():
            return 'cancelled'
        if online.is_set():
            return 'online'
        on_status('設定APを確認しています…')
        backend = backend_factory()
        if not backend.prepare(host, cancel):
            on_status('設定APが見つかりません。端末の接続を待っています。初回は端末をWi-Fi設定画面にしてください。')
            return 'waiting'
        if cancel.is_set():
            raise Cancelled()
        if online.is_set():
            return 'online'
        on_status('端末の設定APへ一時接続しています…')
        backend.connect(cancel)
        source = backend.form_source(cancel)
        if cancel.is_set():
            raise Cancelled()
        on_status('PCホットスポットの接続情報を端末へ送信しています…')
        accepted = backend.submit(source, cancel)
        on_status('設定受付後のWi-Fi接続を待っています。停止時もPCの復帰までお待ちください…' if accepted else
                  '設定応答を確認できません。自動再送せず、Wi-Fi接続結果を待っています…')
        backend.settle()
        outcome = 'configured'
    except Cancelled:
        outcome = 'cancelled'
    except ProbeError as error:
        on_status(setup_message(str(error)))
    except Exception:
        on_status(setup_message('FAILED'))
    finally:
        if backend:
            try:
                backend.restore()
            except Exception:
                outcome = 'failed'
                on_status(setup_message('RESTORE_FAILED'))
            else:
                if outcome == 'configured':
                    try:
                        backend.verify_after(host)
                    except ProbeError as error:
                        outcome = 'failed'
                        on_status(setup_message(str(error)))
                    except Exception:
                        outcome = 'failed'
                        on_status(setup_message('FAILED'))
            finally:
                backend.close()
    if cancel.is_set():
        return 'cancelled'
    if outcome == 'configured':
        on_status('PCのWi-Fiを元の状態へ戻しました。端末からPCへの接続を待っています…')
    return outcome
