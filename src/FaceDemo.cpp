#ifdef STACKCHAN_FACE_DEMO
#include "FaceUI.h"

namespace {
FaceUI face;
const deviceui::Mode modes[] = {deviceui::Mode::Boot, deviceui::Mode::Connecting,
  deviceui::Mode::Sleep, deviceui::Mode::Waking, deviceui::Mode::WakeWait,
  deviceui::Mode::Armed, deviceui::Mode::Preparing, deviceui::Mode::Listening,
  deviceui::Mode::Sending, deviceui::Mode::Thinking, deviceui::Mode::Receiving,
  deviceui::Mode::Speaking, deviceui::Mode::Cooldown, deviceui::Mode::Notice,
  deviceui::Mode::Error, deviceui::Mode::Enrolling, deviceui::Mode::Verify, deviceui::Mode::Setup};
size_t modeIndex = 0;
uint32_t shown = 0, pressed = 0;
bool automatic = true, consumed = true;
void showMode() {
  face.state.asleep = modes[modeIndex] == deviceui::Mode::Sleep;
  face.set(modes[modeIndex], modes[modeIndex] == deviceui::Mode::WakeWait ? "Call: next stage" : "Visual demo only");
  shown = millis();
}
}
void setup() {
  auto cfg = M5.config(); cfg.internal_mic = false; cfg.internal_spk = false;
  cfg.serial_baudrate = 115200; M5.begin(cfg);
  M5.Mic.end(); M5.Speaker.end();
  face.begin(); face.link(deviceui::Link::Ready); showMode();
  Serial.println("FACE DEMO: no microphone, network, filesystem or API");
}
void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) { pressed = millis(); consumed = false; }
  if (!consumed && M5.BtnA.isPressed() && millis() - pressed >= 2000) {
    consumed = true; automatic = !automatic; shown = millis();
  }
  if ((M5.BtnA.wasReleased() && !consumed) || (automatic && millis() - shown >= 2000)) {
    consumed = true; modeIndex = (modeIndex + 1) % (sizeof(modes) / sizeof(modes[0])); showMode();
  }
  face.level(static_cast<unsigned>((millis() / 100) % 11));
  face.tick(millis()); delay(2);
}
#endif
