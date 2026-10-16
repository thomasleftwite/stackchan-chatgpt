#include "FaceUI.h"
#include <cstring>

bool FaceUI::begin() {
  frame_.setColorDepth(16); frame_.setPsram(true);
  available_ = frame_.createSprite(M5.Display.width(), M5.Display.height()) != nullptr;
  dirty_ = true;
  return available_;
}
void FaceUI::set(deviceui::Mode mode, const char* note) {
  state.set(mode, millis()); strlcpy(note_, note, sizeof(note_)); level_ = 0; dirty_ = true;
  tick(millis(), true);
}
void FaceUI::link(deviceui::Link value) { if (value != link_) { link_ = value; dirty_ = true; } }
void FaceUI::tick(uint32_t now, bool force) {
  using namespace deviceui;
  const auto before = state.mode; state.tick(now);
  if (before != state.mode) { note_[0] = 0; dirty_ = true; }
  // All drawing stays in the foreground, never in the microphone callback.
  if (!force && !dirty_ && now - lastFrame_ < 50) return;
  if (!available_) {
    if (force || dirty_ || drawn_ != state.mode) {
      M5.Display.fillScreen(TFT_BLACK); M5.Display.setCursor(2, 2);
      M5.Display.print(title(state.mode)); M5.Display.print("\n"); M5.Display.print(note_);
    }
    dirty_ = false; drawn_ = state.mode; lastFrame_ = now; return;
  }
  const int w = frame_.width(), h = frame_.height();
  const int cx = w / 2;
  const int cy = h * 45 / 100 + (state.mode == Mode::Sleep ? static_cast<int>((now / 1200) % 2) : 0);
  const bool sleeping = state.mode == Mode::Sleep;
  const bool error = state.mode == Mode::Error;
  const bool thinking = state.mode == Mode::Thinking || state.mode == Mode::Sending ||
                        state.mode == Mode::Receiving || state.mode == Mode::Connecting;
  const bool blink = sleeping || (!error && !thinking && now % 4300 < 110);
  const uint16_t white = sleeping ? 0x7BEF : 0xFFFF;
  const uint16_t accent = error ? 0xFBE0 : 0x07FF;
  frame_.fillScreen(TFT_BLACK);
  frame_.setTextSize(1); frame_.setTextColor(white);
  frame_.drawCenterString(title(state.mode), cx, 4);
  const int shift = thinking ? (static_cast<int>((now / 450) % 3) - 1) * 4 : 0;
  const int lift = state.mode == Mode::Waking ? -4 : 0;
  for (int side : {-1, 1}) {
    const int x = cx + side * w / 5 + shift;
    if (blink || state.mode == Mode::Preparing) frame_.fillRoundRect(x - 9, cy + lift, 18, 3, 1, white);
    else if (error) frame_.drawLine(x - 8, cy - side * 4, x + 8, cy + side * 4, white);
    else frame_.fillRoundRect(x - 6, cy - 10 + lift, 12, state.mode == Mode::Listening ? 24 : 20, 5, white);
  }
  int mouth = 2;
  // Stage 1 uses a rhythmic mouth only while actual playback is active.
  // PCM/cursor-synchronised lip motion is a separate later stage.
  if (state.mode == Mode::Speaking) mouth = 3 + static_cast<int>((now / 110) % 4) * 2;
  if (state.mode == Mode::Listening) mouth = 2 + static_cast<int>(level_) / 2;
  frame_.fillRoundRect(cx - (sleeping ? 4 : 9), cy + 25, sleeping ? 8 : 18, mouth, 2, white);
  if (thinking) for (int i = 0; i < 3; ++i) frame_.fillCircle(cx - 12 + i * 12, cy + 39, (now / 250) % 3 == static_cast<unsigned>(i) ? 3 : 1, accent);
  if (state.mode == Mode::Listening) frame_.fillRect(12, h - 31, static_cast<int>(level_) * (w - 24) / 10, 3, accent);
  const char* hint = note_;
  if (!hint[0]) {
    if (sleeping) hint = "Click: wake";
    else if (state.mode == Mode::WakeWait) hint = "Hold: manual ask";
    else if (state.mode == Mode::Preparing) hint = "Please stay quiet";
    else if (state.mode == Mode::Listening) hint = "Speak now";
  }
  frame_.drawCenterString(hint, cx, h - 23);
  const char* status = link_ == Link::Ready ? "PC online" : link_ == Link::WifiOffline ? "WiFi offline" :
                       link_ == Link::PcOffline ? "Start PC service" : "Run PC tool / USB";
  frame_.setTextColor(link_ == Link::Ready ? 0x07E0 : 0xFBE0);
  frame_.drawCenterString(status, cx, h - 11);
  frame_.pushSprite(0, 0);
  dirty_ = false; drawn_ = state.mode; lastFrame_ = now;
}
