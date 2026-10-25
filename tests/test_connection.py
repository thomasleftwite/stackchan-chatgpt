"""Public connection lifecycle tests with OS and host-process boundaries replaced."""
from pathlib import Path
import sys
import threading
import unittest
from unittest.mock import patch, Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'desktop'))
from connection import run_connection
from provisioning import provision_device, select_hotspot, WindowsSetup, Cancelled
from windows_wifi import ProbeError


class FakeSetup:
    def __init__(self, *, present=True, failure=None, cancel=None, cancel_at=None, accepted=True):
        self.calls=[]; self.present=present; self.failure=failure
        self.cancel=cancel; self.cancel_at=cancel_at; self.accepted=accepted

    def step(self, name):
        self.calls.append(name)
        if name==self.cancel_at: self.cancel.set()
        if name==self.failure: raise OSError('PRIVATE_SSID_AND_PASSWORD')

    def prepare(self, host, cancel): self.step('prepare'); return self.present
    def connect(self, cancel): self.step('connect')
    def form_source(self, cancel): self.step('form'); return 'fixture-source'
    def submit(self, source, cancel): self.step('submit'); return self.accepted
    def settle(self): self.step('settle')
    def restore(self): self.step('restore')
    def verify_after(self, host): self.step('verify')
    def close(self): self.step('close')


class ProvisioningLifecycleTests(unittest.TestCase):
    def run_setup(self, backend, cancel=None, online=None):
        messages=[]
        result=provision_device('192.168.137.1',cancel or threading.Event(), online or threading.Event(),
                                messages.append,backend_factory=lambda:backend)
        return result,messages

    def test_existing_connection_does_not_open_wifi_backend(self):
        online=threading.Event();online.set();backend=FakeSetup()
        self.assertEqual(self.run_setup(backend,online=online)[0],'online')
        self.assertEqual(backend.calls,[])

    def test_absent_ap_never_sends_and_closes_adapter(self):
        backend=FakeSetup(present=False)
        self.assertEqual(self.run_setup(backend)[0],'waiting')
        self.assertEqual(backend.calls,['prepare','restore','close'])

    def test_post_settles_before_radio_restore_and_does_not_claim_online(self):
        backend=FakeSetup()
        result,messages=self.run_setup(backend)
        self.assertEqual(result,'configured')
        self.assertEqual(backend.calls,['prepare','connect','form','submit','settle','restore','verify','close'])
        self.assertNotIn('質問できます',''.join(messages))

    def test_cancel_before_submit_restores_without_post(self):
        cancel=threading.Event();backend=FakeSetup(cancel=cancel,cancel_at='form')
        self.assertEqual(self.run_setup(backend,cancel)[0],'cancelled')
        self.assertEqual(backend.calls,['prepare','connect','form','restore','close'])

    def test_cancel_after_submit_still_settles_and_restores(self):
        cancel=threading.Event();backend=FakeSetup(cancel=cancel,cancel_at='submit')
        self.assertEqual(self.run_setup(backend,cancel)[0],'cancelled')
        self.assertEqual(backend.calls,['prepare','connect','form','submit','settle','restore','verify','close'])

    def test_unknown_response_is_not_retried(self):
        backend=FakeSetup(accepted=False)
        result,messages=self.run_setup(backend)
        self.assertEqual(result,'configured')
        self.assertEqual(backend.calls.count('submit'),1)
        self.assertTrue(any('自動再送せず' in m for m in messages))

    def test_form_failure_restores_without_leaking_exception(self):
        backend=FakeSetup(failure='form')
        result,messages=self.run_setup(backend)
        self.assertEqual(result,'failed')
        self.assertEqual(backend.calls,['prepare','connect','form','restore','close'])
        self.assertNotIn('PRIVATE',''.join(messages))

    def test_restore_failure_is_reported_and_handle_closed(self):
        backend=FakeSetup(failure='restore')
        result,messages=self.run_setup(backend)
        self.assertEqual(result,'failed')
        self.assertEqual(backend.calls[-1],'close')
        self.assertTrue(any('戻せません' in m for m in messages))

    def test_changed_hotspot_after_successful_restore_keeps_redetection_instruction(self):
        backend=FakeSetup()
        def verify(host):
            backend.step('verify');raise ProbeError('HOTSPOT_CHANGED')
        backend.verify_after=verify
        result,messages=self.run_setup(backend)
        self.assertEqual(result,'failed')
        self.assertTrue(any('再検出' in m for m in messages))
        self.assertFalse(any('戻せません' in m for m in messages))
        self.assertEqual(backend.calls[-1],'close')

    def test_cancel_during_last_credentials_read_prevents_post(self):
        setup=WindowsSetup.__new__(WindowsSetup)
        setup.credentials={'ssid':'fixture','password':'x'*12}
        setup.interface=Mock();setup.wlan=Mock()
        current=setup.wlan.connection.return_value
        current.state=1;current.association.ssid.value.return_value=b'Stackchan-Setup'
        cancel=threading.Event()
        def credentials():cancel.set();return setup.credentials
        with patch('provisioning.hotspot_credentials',side_effect=credentials),patch('provisioning.send_configuration') as send:
            with self.assertRaises(Cancelled):setup.submit('fixture-source',cancel)
        send.assert_not_called()

    def test_hotspot_selection_auto_updates_single_address_and_requires_ambiguity_resolution(self):
        self.assertEqual(select_hotspot('192.168.1.1',{'192.168.137.1':'192.168.137.0/24'}),'192.168.137.1')
        with self.assertRaises(ProbeError):select_hotspot('',{})
        with self.assertRaises(ProbeError):select_hotspot('',{'192.168.1.1':'a','192.168.2.1':'b'})
        self.assertEqual(select_hotspot('192.168.2.1',{'192.168.1.1':'a','192.168.2.1':'b'}),'192.168.2.1')


class ConnectionLifecycleTests(unittest.TestCase):
    def test_host_is_ready_before_provisioning_and_shuts_down_on_stop(self):
        cancel=threading.Event();service_ready=threading.Event();service_closed=threading.Event();calls=[]
        def service(runtime,node,host,env,stop,status,*args):
            service_ready.set();status('WIRELESS ready')
            stop.wait(3);service_closed.set();return 0
        def provision(host,stop,online,status):
            self.assertTrue(service_ready.is_set());calls.append('provision')
            cancel.set();return 'cancelled'
        self.assertEqual(run_connection('.','node','192.168.137.1',{},cancel,lambda _:None,
                         service=service,provision=provision,existing_wait=0),0)
        self.assertEqual(calls,['provision']);self.assertTrue(service_closed.is_set())

    def test_peer_online_skips_all_setup(self):
        cancel=threading.Event();called=[]
        def service(runtime,node,host,env,stop,status,*args):
            status('WIRELESS ready');status('WIRELESS device-online')
            cancel.set();stop.wait(3);return 0
        run_connection('.','node','192.168.137.1',{},cancel,lambda _:None,
                       service=service,provision=lambda *a:called.append(True),existing_wait=.01)
        self.assertEqual(called,[])

    def test_failed_listener_does_not_configure_device(self):
        called=[]
        self.assertEqual(run_connection('.','node','192.168.137.1',{},threading.Event(),lambda _:None,
                         service=lambda *a:1,provision=lambda *a:called.append(True)),1)
        self.assertEqual(called,[])

    def test_listener_failure_during_setup_cancels_radio_transaction(self):
        setup_started=threading.Event();called=[]
        def service(runtime,node,host,env,stop,status,*args):
            status('WIRELESS ready');setup_started.wait(2);return 1
        def provision(host,cancel,online,status):
            setup_started.set();called.append(cancel.wait(2));return 'cancelled'
        result=run_connection('.','node','192.168.137.1',{},threading.Event(),lambda _:None,
                              service=service,provision=provision,existing_wait=0)
        self.assertEqual(result,1);self.assertEqual(called,[True])


if __name__=='__main__':unittest.main()
