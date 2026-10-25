"""Offline checks for diagnostic boundaries and restoring after HTTP failure."""
import ctypes as C
from pathlib import Path
import sys
import unittest
import threading
from unittest.mock import patch, Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import probe_stackchan_ap as probe


class FakeWlan:
    bssids = 1

    def __init__(self):
        self.handle = None
        self.api = self
        self.connected = False
        self.disconnected = False
        self.closed = False

    def interfaces(self):
        interface = probe.Interface()
        interface.state = 4
        return [interface]

    def WlanScan(self, *args):
        return 0

    check = staticmethod(probe.Wlan.check)

    def networks(self, guid):
        network = probe.Network()
        network.ssid.length = len(probe.TARGET)
        network.ssid.data[:len(probe.TARGET)] = probe.TARGET
        network.bssids = self.bssids
        network.connectable = network.secure = 1
        network.bssType, network.auth, network.cipher = 1, 7, 4
        return [network]

    def connection(self, guid):
        if not self.connected:
            return None
        value = probe.Connection()
        value.state = 1
        value.association.ssid.length = len(probe.TARGET)
        value.association.ssid.data[:len(probe.TARGET)] = probe.TARGET
        return value

    def connect(self, *args):
        self.connected = True

    def disconnect(self, *args):
        self.disconnected = True
        self.connected = False

    def close(self):
        self.closed = True


class ProvisioningProbeTest(unittest.TestCase):
    def test_existing_wifimanager_relative_action_and_fixed_absolute_action(self):
        for action in ('wifisave', '/wifisave'):
            form = probe.WifiForm()
            form.feed(f"<form method='POST' action='{action}'><input name='s'><input name='p'></form>")
            self.assertTrue(form.valid)

    def test_other_destinations_missing_password_and_wrong_method_rejected(self):
        for method, action, fields in (
            ('GET', 'wifisave', "<input name='s'><input name='p'>"),
            ('POST', 'http://other/wifisave', "<input name='s'><input name='p'>"),
            ('POST', '//other/wifisave', "<input name='s'><input name='p'>"),
            ('POST', '/wifisave?other=1', "<input name='s'><input name='p'>"),
            ('POST', 'wifisave', "<input name='s'>"),
        ):
            form = probe.WifiForm()
            form.feed(f"<form method='{method}' action='{action}'>{fields}</form>")
            self.assertFalse(form.valid)

    @unittest.skipUnless(sys.platform == 'win32', 'Native layouts are Windows-specific')
    def test_windows_abi_layout(self):
        self.assertEqual([C.sizeof(t) for t in (probe.Interface, probe.Network, probe.Connection, probe.Parameters)], [532, 628, 604, 40])

    def exercise(self, wlan, *, http_failure=False, ready=True, provision=False, form_valid=True):
        state = dict(wiredInternet=ready, hotspotOn=True, twoPointFourGHz=True,
                     credentialsReadable=True, clientCount=0)
        with patch.object(probe, 'Wlan', return_value=wlan), \
             patch.object(probe.time, 'sleep'), \
             patch.object(probe, 'hotspot_snapshot', side_effect=[state, state, dict(state, clientCount=1), dict(state, clientCount=1)]), \
             patch.object(probe, 'hotspot_credentials', return_value={'ssid': 'fixture', 'password': 'x' * 12}), \
             patch.object(probe, 'local_source', return_value='192.168.4.2'), \
             patch.object(probe, 'temporary_profile', return_value='<fixture/>'), \
             patch.object(probe, 'read_form', **({'side_effect': OSError()} if http_failure else
                          {'return_value': {'wifiFormVerified': form_valid, 'httpStatus': 200}})):
            return probe.run(connect=True, provision=provision)

    @unittest.skipUnless(sys.platform == 'win32', 'Windows probe')
    def test_authorized_provision_calls_sender_once_then_restores_and_waits_for_join(self):
        wlan = FakeWlan()
        with patch.object(probe, 'send_configuration') as sender:
            result = self.exercise(wlan, provision=True)
        sender.assert_called_once()
        self.assertTrue(result['restoredDisconnected'])
        self.assertTrue(result['hotspotClientCountIncreased'])
        self.assertTrue(result['hotspotCredentialsUnchanged'])
        self.assertTrue(wlan.closed)

    def test_post_response_timeout_is_unknown_and_never_retried(self):
        connection = Mock()
        connection.getresponse.side_effect = TimeoutError()
        report = {'configurationSent': False}
        with patch.object(probe.http.client, 'HTTPConnection', return_value=connection):
            probe.send_configuration('192.168.4.2', {'ssid': 'fixture', 'password': 'x' * 12}, report)
        connection.request.assert_called_once()
        self.assertEqual(connection.request.call_args.args[:2], ('POST', '/wifisave'))
        self.assertTrue(report['configurationSent'])
        self.assertTrue(report['configurationResponseUnknown'])
        connection.close.assert_called_once()

    def test_cancel_during_tcp_connect_prevents_post(self):
        cancel=threading.Event();connection=Mock();connection.connect.side_effect=cancel.set
        report={}
        with patch.object(probe.http.client,'HTTPConnection',return_value=connection):
            probe.send_configuration('192.168.4.2',{'ssid':'fixture','password':'x'*12},report,cancel)
        connection.request.assert_not_called()
        connection.close.assert_called_once()
        self.assertTrue(report['configurationCancelled'])
        self.assertFalse(report.get('configurationAttempted',False))

    def test_http_200_requires_expected_wifimanager_receipt_and_redacts_values(self):
        for body, accepted in ((b'Unrelated page', False), (b'Saving Credentials', True)):
            connection = Mock()
            response = connection.getresponse.return_value
            response.status = 200
            response.read.return_value = body
            report = {}
            with patch.object(probe.http.client, 'HTTPConnection', return_value=connection):
                probe.send_configuration('192.168.4.2', {'ssid': 'fixture', 'password': 'x' * 12}, report)
            self.assertEqual(report['configurationAccepted'], accepted)
            self.assertNotIn('fixture', str(report))
            self.assertNotIn('x' * 12, str(report))

    @unittest.skipUnless(sys.platform == 'win32', 'Windows probe')
    def test_invalid_form_prevents_provisioning_but_still_restores(self):
        wlan = FakeWlan()
        with patch.object(probe, 'send_configuration') as sender:
            result = self.exercise(wlan, provision=True, form_valid=False)
        sender.assert_not_called()
        self.assertEqual(result['error'], 'WIFI_FORM_NOT_VERIFIED')
        self.assertTrue(result['restoredDisconnected'])

    def test_scan_and_saved_ssid_comparison_keeps_names_out_of_result(self):
        form = probe.WifiForm('fixture & name')
        form.feed("<a data-ssid='fixture &amp; name'></a><input name='s' placeholder='fixture &amp; name'>")
        self.assertTrue(form.target_seen)
        self.assertTrue(form.saved_ssid_matches)

    @unittest.skipUnless(sys.platform == 'win32', 'Windows probe')
    def test_success_restores_and_never_saves_or_submits_configuration(self):
        wlan = FakeWlan()
        result = self.exercise(wlan)
        self.assertTrue(result['wifiFormVerified'])
        self.assertTrue(result['restoredDisconnected'])
        self.assertFalse(result['configurationSent'])
        self.assertFalse(result['profileSaved'])
        self.assertTrue(wlan.closed)

    @unittest.skipUnless(sys.platform == 'win32', 'Windows probe')
    def test_http_failure_still_disconnects_and_closes(self):
        wlan = FakeWlan()
        result = self.exercise(wlan, http_failure=True)
        self.assertEqual(result['error'], 'PROBE_FAILED')
        self.assertTrue(wlan.disconnected)
        self.assertTrue(result['restoredDisconnected'])
        self.assertTrue(wlan.closed)

    @unittest.skipUnless(sys.platform == 'win32', 'Windows probe')
    def test_multiple_target_bssids_do_not_connect(self):
        wlan = FakeWlan()
        wlan.bssids = 2
        result = self.exercise(wlan)
        self.assertEqual(result['error'], 'REQUIRE_SINGLE_TARGET_AP')
        self.assertFalse(wlan.connected)
        self.assertFalse(wlan.disconnected)

    @unittest.skipUnless(sys.platform == 'win32', 'Windows probe')
    def test_unready_preflight_does_not_connect(self):
        wlan = FakeWlan()
        result = self.exercise(wlan, ready=False)
        self.assertEqual(result['error'], 'PREFLIGHT_NOT_READY')
        self.assertFalse(wlan.connected)
        self.assertFalse(wlan.disconnected)


if __name__ == '__main__':
    unittest.main()
