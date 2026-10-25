"""Host-first connection lifecycle. Existing peers bypass AP provisioning."""
import threading
import time
from provisioning import provision_device
from wireless import run_wireless


def run_connection(runtime, node, host, env, stop_event, on_status, commands=None, on_event=None,
                   *, service=run_wireless, provision=provision_device, existing_wait=10, peer_wait=60):
    ready, online, done, service_stop = (threading.Event() for _ in range(4))
    result = [1]

    class Cancellation:
        def is_set(self):
            return stop_event.is_set() or done.is_set()

        def wait(self, seconds):
            deadline = time.monotonic() + seconds
            while not self.is_set() and time.monotonic() < deadline:
                stop_event.wait(min(.05, max(0, deadline-time.monotonic())))
            return self.is_set()

    def status(value):
        if value == 'WIRELESS ready':
            ready.set()
        if value == 'WIRELESS device-online':
            online.set()
        on_status(value)

    def serve():
        try:
            result[0] = service(runtime, node, host, env, service_stop, status, commands, on_event)
        except Exception:
            on_status('WIRELESS failed')
        finally:
            done.set()

    def wait_for(event, seconds):
        deadline = time.monotonic() + seconds
        while not event.is_set() and time.monotonic() < deadline:
            if stop_event.is_set() or done.is_set():
                return False
            event.wait(.05)
        return event.is_set() and not stop_event.is_set() and not done.is_set()

    worker = threading.Thread(target=serve, daemon=False)
    worker.start()
    try:
        if not wait_for(ready, 15):
            return 1
        if not wait_for(online, existing_wait) and not stop_event.is_set() and not done.is_set():
            outcome = provision(host, Cancellation(), online, lambda text: on_status('CONNECT '+text))
            if outcome == 'configured' and not wait_for(online, peer_wait) and not stop_event.is_set():
                on_status('CONNECT 端末からPCへの到達をまだ確認できません。ホットスポットと端末表示を確認してください。自動再送せず待受を続けます。')
        while not stop_event.wait(.1) and not done.is_set():
            pass
        return result[0] if done.is_set() else 0
    finally:
        service_stop.set()
        # run_wireless bounds child shutdown; do not leave a competing listener.
        worker.join()
