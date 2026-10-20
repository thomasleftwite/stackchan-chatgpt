"""Allow the pinned WebSockets 2.7.3 ESP32 frame limit to be configured.

Only the generated project-local dependency is patched. Unexpected source fails
the build so upgrading the library requires a deliberate review.
"""
from pathlib import Path

Import("env")

header = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "WebSockets/src/WebSockets.h"
old = "#if defined(ESP8266) || defined(ESP32)\n\n#define WEBSOCKETS_MAX_DATA_SIZE (15 * 1024)"
new = "#if defined(ESP8266) || defined(ESP32)\n\n#ifndef WEBSOCKETS_MAX_DATA_SIZE\n#define WEBSOCKETS_MAX_DATA_SIZE (15 * 1024)\n#endif"
source = header.read_text(encoding="utf-8")
if old in source:
    if source.count(old) != 1:
        raise RuntimeError("Unexpected WebSockets header: review the frame-limit patch")
    header.write_text(source.replace(old, new, 1), encoding="utf-8")
elif new not in source:
    raise RuntimeError("WebSockets source changed: review the frame-limit patch")


# WebSockets 2.7.3 does not expose WiFiClientSecure PSK configuration. Add the
# pinned ESP32 API call only in the wireless firmware; never use setInsecure there.
client = header.with_name("WebSocketsClient.cpp")
old_psk = """            } else if(!SSL_FINGERPRINT_IS_SET) {
                _client.ssl->setInsecure();"""
new_psk = """            } else if(!SSL_FINGERPRINT_IS_SET) {
#ifdef STACKCHAN_WIRELESS
                extern const char* stackchanTlsPsk();
                _client.ssl->setPreSharedKey("stackchan-v1", stackchanTlsPsk());
#else
                _client.ssl->setInsecure();
#endif"""
source = client.read_text(encoding="utf-8")
if new_psk not in source:
    if source.count(old_psk) != 1:
        raise RuntimeError("WebSockets PSK extension changed: review pinned source")
    client.write_text(source.replace(old_psk,new_psk,1),encoding="utf-8")
