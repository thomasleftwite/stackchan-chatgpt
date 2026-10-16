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
    const bool next = fakeHeldForever || now < 2100 || (now >= 2300 && now < 2360);
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
};
struct FakeMic {
  struct Config { uint32_t sample_rate = 16000; unsigned magnification = 4; } settings;
  Config config() { return settings; }
  void config(Config value) { settings = value; }
  bool begin() { return true; }
  void end() {}
  void setBufferReleaseCallback(void*, void (*)(void*, void*, size_t)) {}
  bool record(int16_t*, size_t, uint32_t, bool = false) { ++fakeRecordRequests; return false; }
  unsigned isRecording() { return 0; }
};
struct FakeSpeaker {
  bool begin() { return true; }
  void end() {}
  void stop() {}
  void setVolume(int) {}
  bool tone(int, int) { ++fakeBeeps; return true; }
  bool isPlaying() const { return false; }
  bool playRaw(const int16_t*, size_t, unsigned, bool, int, int) { return true; }
};
struct FakeM5 {
  struct Config { bool internal_spk = false, internal_mic = false; struct { bool atomic_echo = false; } external_speaker; unsigned serial_baudrate = 0; };
  FakeButton BtnA;
  FakeDisplay Display;
  FakeMic Mic;
  FakeSpeaker Speaker;
  Config config() { return {}; }
  void begin(Config) {}
  void update() { BtnA.update(); }
};
inline FakeM5 M5;
struct FakeWifi {
  int state = 0;
  int status() const { return state; }
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
  void loop() {}
  void disconnect() {}
};
struct JsonVariant {
  template<class T> T as() const { return {}; }
  template<class T> bool is() const { return false; }
};
struct JsonDocument { JsonVariant operator[](const char*) const { return {}; } };
inline bool deserializeJson(JsonDocument&, uint8_t*, size_t) { return true; }
