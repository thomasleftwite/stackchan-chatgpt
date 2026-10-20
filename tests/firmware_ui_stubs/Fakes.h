#pragma once
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <limits>

using String = std::string;
using std::max;
constexpr int TFT_BLACK = 0, WIFI_STA = 1, WL_CONNECTED = 3;
constexpr unsigned MALLOC_CAP_SPIRAM = 1, MALLOC_CAP_8BIT = 2;
constexpr double PI = 3.141592653589793;
inline uint32_t fakeTime = 0;
inline uint32_t fakeGestureStart = 0;
inline bool fakeHeldForever = false;
inline uint32_t fakeDelayBudget = std::numeric_limits<uint32_t>::max();
inline unsigned fakeBeeps = 0, fakeRecordRequests = 0;
inline unsigned fakeMicBegins = 0, fakeFrames = 0;
inline unsigned fakeCallbackQueues = 0, fakeCaptureGaps = 0;
inline std::function<bool(int16_t*,size_t,uint32_t)> fakeRecordAction;
inline bool fakeMicActive = false, fakeCanvasFailure = false;
inline std::function<bool(uint32_t)> fakeButtonSchedule;
inline std::function<void()> fakeWsLoop;
inline uint32_t fakePlaybackMs = 0, fakePlaybackUntil = 0;
inline uint32_t fakeCueDurationMs = 0, fakeCueEndedAt = 0, fakeMicBeganAt = 0;
inline uint32_t millis() { return fakeTime; }
inline void delay(unsigned ms) { fakeTime += ms; if (fakeTime > fakeDelayBudget) throw std::runtime_error("button release wait exceeded budget"); }
inline size_t strlcpy(char* out, const char* in, size_t capacity) {
  const auto size = std::strlen(in); if (capacity) { std::strncpy(out, in, capacity - 1); out[capacity - 1] = 0; } return size;
}
inline void* heap_caps_calloc(size_t n, size_t bytes, unsigned) { return std::calloc(n, bytes); }
struct FakeSerial {
  std::vector<std::string> messages;
  void println(const char* text) { messages.emplace_back(text); }
  template<class... Args> void printf(const char* format, Args... args) { char text[256]; std::snprintf(text, sizeof(text), format, args...); messages.emplace_back(text); }
  int available() { return 0; }
  char read() { return 0; }
};
inline FakeSerial Serial;
struct FakeButton {
  bool down = false, press = false, release = false;
  void update() {
    const uint32_t now = fakeTime - fakeGestureStart;
    const bool next = fakeButtonSchedule ? fakeButtonSchedule(fakeTime) : fakeHeldForever || now < 2100 || (now >= 2300 && now < 2360);
    press = next && !down; release = !next && down; down = next;
  }
  bool wasPressed() const { return press; }
  bool wasReleased() const { return release; }
  bool isPressed() const { return down; }
};
struct FakeDisplay {
  std::string text;
  std::vector<std::string> history;
  void fillScreen(int) {}
  void setCursor(int, int) {}
  void setTextSize(int) {}
  void print(const char* value) { text = value; history.emplace_back(value); }
  int width() const { return 128; }
  int height() const { return 128; }
  std::string svg;
};
struct FakeMic {
  struct Request { int16_t* data; size_t count; uint32_t rate, due; };
  std::vector<Request> pending;
  void* releaseArgs = nullptr;
  void (*onRelease)(void*,void*,size_t) = nullptr;
  struct Config { uint32_t sample_rate = 16000; unsigned magnification = 4; } settings;
  Config config() { return settings; }
  void config(Config value) { settings = value; }
  bool begin() { ++fakeMicBegins; fakeMicActive = true; fakeMicBeganAt = fakeTime; return true; }
  void end() { fakeMicActive = false; pending.clear(); }
  void setBufferReleaseCallback(void* args, void (*fn)(void*, void*, size_t)) { releaseArgs=args; onRelease=fn; }
  bool record(int16_t* data, size_t n, uint32_t rate, bool = false) {
    ++fakeRecordRequests;
    if(!fakeRecordAction) return false;
    if(onRelease) {
      if(pending.size()>=2) return false;
      pending.push_back({data,n,rate,fakeTime+static_cast<uint32_t>(uint64_t(n)*1000/rate)});
      ++fakeCallbackQueues; return true;
    }
    return fakeRecordAction(data,n,rate);
  }
  void tick() {
    if(pending.empty() || fakeTime<pending.front().due) return;
    const auto request=pending.front();pending.erase(pending.begin());
    if(!fakeRecordAction(request.data,request.count,request.rate)) return;
    if(onRelease) onRelease(releaseArgs,request.data,request.count);
    if(fakeMicActive && pending.empty()) ++fakeCaptureGaps;
  }
  unsigned isRecording() { return static_cast<unsigned>(pending.size()); }
  bool isRunning() const { return fakeMicActive; }
};
struct FakeSpeaker {
  bool begin() { return true; }
  void end() { fakePlaybackUntil = 0; }
  void stop() { fakePlaybackUntil = 0; }
  void setVolume(int) {}
  bool tone(int, int) { ++fakeBeeps; fakeCueEndedAt = fakeTime + fakeCueDurationMs;
    if(fakeCueDurationMs) fakePlaybackUntil = fakeCueEndedAt; return true; }
  bool isPlaying() const { return fakePlaybackUntil && static_cast<int32_t>(fakePlaybackUntil - fakeTime) > 0; }
  bool playRaw(const int16_t*, size_t, unsigned, bool, int, int) { fakePlaybackUntil = fakeTime + fakePlaybackMs; return true; }
};
struct FakeM5 {
  struct Config { bool internal_spk = false, internal_mic = false; struct { bool atomic_echo = false; } external_speaker; unsigned serial_baudrate = 0; };
  FakeButton BtnA;
  FakeDisplay Display;
  FakeMic Mic;
  FakeSpeaker Speaker;
  Config config() { return {}; }
  void begin(Config) {}
  void update() { BtnA.update(); Mic.tick(); }
};
inline FakeM5 M5;
// Record the actual renderer's drawing commands as SVG for offline inspection.
class M5Canvas {
 public:
  explicit M5Canvas(FakeDisplay*) {}
  void setColorDepth(int) {}
  void setPsram(bool) {}
  void* createSprite(int, int) { return fakeCanvasFailure ? nullptr : this; }
  int width() const { return 128; }
  int height() const { return 128; }
  void setTextSize(int) {}
  void setTextColor(uint16_t color) { textColor = color; }
  void fillScreen(int) { body = "<rect width='128' height='128' fill='black'/>"; labels.clear(); }
  void drawCenterString(const char* text, int x, int y) {
    labels += std::string(text) + "\n";
    body += "<text x='" + std::to_string(x) + "' y='" + std::to_string(y + 8) +
      "' text-anchor='middle' font-family='monospace' font-size='10' fill='" + color(textColor) + "'>" + text + "</text>";
  }
  void fillRoundRect(int x, int y, int w, int h, int radius, uint16_t c) {
    body += "<rect x='" + std::to_string(x) + "' y='" + std::to_string(y) + "' width='" + std::to_string(w) +
      "' height='" + std::to_string(h) + "' rx='" + std::to_string(radius) + "' fill='" + color(c) + "'/>";
  }
  void fillRect(int x, int y, int w, int h, uint16_t c) { fillRoundRect(x,y,w,h,0,c); }
  void fillCircle(int x, int y, int r, uint16_t c) {
    body += "<circle cx='" + std::to_string(x) + "' cy='" + std::to_string(y) + "' r='" + std::to_string(r) + "' fill='" + color(c) + "'/>";
  }
  void drawLine(int x, int y, int xx, int yy, uint16_t c) {
    body += "<path d='M " + std::to_string(x) + " " + std::to_string(y) + " L " + std::to_string(xx) + " " + std::to_string(yy) + "' stroke='" + color(c) + "'/>";
  }
  void pushSprite(int, int) { ++fakeFrames; M5.Display.text = labels; M5.Display.history.push_back(labels);
    M5.Display.svg = "<svg xmlns='http://www.w3.org/2000/svg' width='128' height='128' viewBox='0 0 128 128'>" + body + "</svg>"; }
 private:
  std::string body, labels;
  uint16_t textColor = 0xFFFF;
  static std::string color(uint16_t c) { char out[8]; std::snprintf(out,sizeof(out),"#%02x%02x%02x", ((c>>11)&31)*255/31, ((c>>5)&63)*255/63, (c&31)*255/31); return out; }
};
enum WiFiEvent_t { ARDUINO_EVENT_WIFI_AP_STACONNECTED, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED, ARDUINO_EVENT_WIFI_AP_STOP };
struct FakeWifi {
  void onEvent(void (*)(WiFiEvent_t)) {}
  int state = 0;
  int status() const { return state; }
  void setAutoReconnect(bool) {}
  void persistent(bool) {}
  void mode(int) {}
  void begin() {}
};
inline FakeWifi WiFi;
struct IPAddress { bool fromString(const char*) { return true; } unsigned operator[](int) const { return 10; } };
enum WStype_t { WStype_DISCONNECTED, WStype_ERROR, WStype_BIN, WStype_TEXT, WStype_FRAGMENT_TEXT_START, WStype_FRAGMENT_BIN_START, WStype_FRAGMENT, WStype_FRAGMENT_FIN };
class WebSocketsClient {
 public:
  void onEvent(std::function<void(WStype_t, uint8_t*, size_t)>) {}
  void begin(const char*, unsigned, const char*, const char*) {}
  void setReconnectInterval(unsigned) {}
  void setExtraHeaders(const char*) {}
  bool sendTXT(const char*) { return true; }
  bool sendBIN(uint8_t*, size_t) { return true; }
  void loop() { if (fakeWsLoop) fakeWsLoop(); }
  void disconnect() {}
};
struct JsonVariant {
  template<class T> T as() const { return {}; }
  template<class T> bool is() const { return false; }
};
struct JsonDocument { JsonVariant operator[](const char*) const { return {}; } };
inline bool deserializeJson(JsonDocument&, uint8_t*, size_t) { return true; }
