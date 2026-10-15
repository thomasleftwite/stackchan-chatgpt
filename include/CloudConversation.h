#pragma once
#include <Arduino.h>
#include <functional>
#include <vector>
#include "AudioDevice.h"
#include "Config.h"

class CloudConversation {
 public:
  using Status = std::function<void(const String&)>;
  bool begin(const Config& config, String& error);
  // Empty transcript is the normal inactivity exit, not an API request to Gemini.
  bool transcribe(AudioDevice& audio, String& transcript, String& error, Status status);
  bool answer(const String& transcript, String& response, String& error);
  bool speak(AudioDevice& audio, const String& text, String& error, Status status);
  void clearHistory() { history_.clear(); }
 private:
  const Config* config_ = nullptr;
  char* ca_ = nullptr;
  struct Turn { String user; String assistant; };
  std::vector<Turn> history_;
};
