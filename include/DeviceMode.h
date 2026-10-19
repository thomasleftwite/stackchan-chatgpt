#pragma once
#include <cstdint>

namespace deviceui {
enum class Mode { Boot, Connecting, Sleep, Waking, WakeWait, Armed, Preparing,
                  Listening, Sending, Thinking, Receiving, Speaking, Cooldown,
                  Notice, Error, Enrolling, Verify, Setup };
enum class Link { Unconfigured, WifiOffline, PcOffline, Ready };

// Runtime also checks links before enabling wake input in WakeWait.
class State {
 public:
  static constexpr uint32_t sleepAfterMs = 180000, wakingMs = 350;
  Mode mode = Mode::Boot;
  uint32_t since = 0;
  bool asleep = true;
  void set(Mode value, uint32_t now) { mode = value; since = now; }
  void wake(uint32_t now) { asleep = false; activity_ = now; set(Mode::Waking, now); }
  void sleep(uint32_t now) { asleep = true; set(Mode::Sleep, now); }
  void rest(uint32_t now) { activity_ = now; set(asleep ? Mode::Sleep : Mode::WakeWait, now); }
  bool mayStart() const { return !asleep && (mode == Mode::WakeWait || mode == Mode::Armed); }
  bool microphoneAllowed(bool wakeEnabled = false) const {
    return !asleep && (mode == Mode::Preparing || mode == Mode::Listening ||
                      (wakeEnabled && mode == Mode::WakeWait));
  }
  void tick(uint32_t now) {
    if (mode == Mode::Waking && now - since >= wakingMs) set(Mode::WakeWait, now);
    if (mode == Mode::WakeWait && now - activity_ >= sleepAfterMs) sleep(now);
  }
 private:
  uint32_t activity_ = 0;
};

inline const char* title(Mode mode) {
  switch (mode) {
    case Mode::Boot: return "Starting";
    case Mode::Connecting: return "Connecting";
    case Mode::Sleep: return "Sleeping";
    case Mode::Waking: return "Hello!";
    case Mode::WakeWait: return "Ready";
    case Mode::Armed: return "Release to ask";
    case Mode::Preparing: return "Preparing";
    case Mode::Listening: return "Listening";
    case Mode::Sending: return "Sending";
    case Mode::Thinking: return "Thinking";
    case Mode::Receiving: return "Receiving";
    case Mode::Speaking: return "Speaking";
    case Mode::Cooldown: return "One moment";
    case Mode::Notice: return "Try again";
    case Mode::Error: return "Not ready";
    case Mode::Enrolling: return "Learning call";
    case Mode::Verify: return "Try your call";
    case Mode::Setup: return "Settings";
  }
  return "";
}
} // namespace deviceui
