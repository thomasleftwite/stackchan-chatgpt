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
