// Compile the actual firmware loop/run/display paths; only hardware is replaced.
#define STACKCHAN_GEMINI_DEVICE 1
#include "firmware_ui_stubs/Fakes.h"
#ifdef _MSC_VER
// ESP32 size_t is 32-bit; this host is x64. Ignore only the known ABI warnings.
#pragma warning(push)
#pragma warning(disable: 4244 4267)
#endif
#include "../src/AudioDevice.cpp"
#include "../src/LocalGemini.cpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <iostream>

int main() {
  unsigned failedTests = 0;
  struct Case { const char* name; bool provisioned; int wifi; bool expired; unsigned expectedRequests; };
  const Case cases[] = {{"not configured",false,0,false,0}, {"WiFi lost",true,0,false,0},
                        {"PC heartbeat expired",true,WL_CONNECTED,true,0}, {"ready capture path",true,WL_CONNECTED,false,1}};
  for (const auto& item : cases) {
    fakeTime = item.expired ? 30000 : 0; fakeGestureStart = fakeTime;
    fakeBeeps = fakeRecordRequests = 0; M5.BtnA = {}; M5.Display = {};
    configured = item.provisioned; pcOnline = configured; pcSeen = 0;
    WiFi.state = item.wifi; consumed = true; pressedAt = 0;
    setup();
    // A real 2.1 second hold then release, as in the user's report.
    while (fakeTime - fakeGestureStart < 2200) loop();
    const bool stuck = M5.Display.text == "Release to record";
    const bool safe = fakeRecordRequests == item.expectedRequests;
    // Then a short click, which must not be required to leave the hold prompt.
    while (fakeTime - fakeGestureStart < 2400) loop();
    const bool matchesReport = stuck && fakeBeeps == 1 && M5.Display.text != "Release to record";
    std::cout << (stuck || !safe ? "FAIL " : "PASS ")
              << item.name
              << ": hold/release stuck=" << stuck << ", micRequests=" << fakeRecordRequests
              << ", nextClickOnlyBeepsAndRestoresIdle=" << matchesReport << '\n';
    if (stuck || !safe) ++failedTests;
    if (item.expectedRequests) {
      // A stale link must show offline without requiring another button click.
      fakeTime += 21000; loop();
      bool lifecycle = !pcOnline && M5.Display.text.find("PC service stopped") != std::string::npos;
      strlcpy(command, "GEM PING", sizeof(command)); dispatch();
      lifecycle = lifecycle && pcReady();
      strlcpy(command, "GEM STOP", sizeof(command)); dispatch();
      strlcpy(command, "GEM PING", sizeof(command)); dispatch();
      lifecycle = lifecycle && !pcReady() && !configured && !token[0];
      if (!lifecycle) ++failedTests;
      std::cout << (lifecycle ? "PASS " : "FAIL ") << "heartbeat expiry/recovery and explicit stop\n";
    }
    std::free(input); std::free(output); input = output = nullptr;
  }
  fakeTime = fakeGestureStart = 0; fakeHeldForever = true; fakeDelayBudget = 16000;
  fakeRecordRequests = 0; M5.BtnA = {}; M5.Display = {};
  configured = pcOnline = true; pcSeen = 0; WiFi.state = WL_CONNECTED; consumed = true;
  setup();
  bool bounded = true;
  try { while (fakeTime < 2200) loop(); } catch (const std::runtime_error&) { bounded = false; }
  const bool noStalePrompt = M5.Display.text != "Release to record" && fakeRecordRequests == 0;
  std::cout << (bounded && noStalePrompt ? "PASS " : "FAIL ") << "unreleased button exits with an instruction\n";
  if (!bounded || !noStalePrompt) ++failedTests;
  std::free(input); std::free(output); input = output = nullptr;
  return failedTests ? 1 : 0;
}
