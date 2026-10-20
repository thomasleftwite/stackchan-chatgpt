// Compile the actual firmware loop/run/display paths; only hardware is replaced.
#define STACKCHAN_GEMINI_DEVICE 1
#include "firmware_ui_stubs/Fakes.h"
#ifdef _MSC_VER
// ESP32 size_t is 32-bit; this host is x64. Ignore only the known ABI warnings.
#pragma warning(push)
#pragma warning(disable: 4244 4267)
#endif
#include "../src/AudioDevice.cpp"
#include "../src/FaceUI.cpp"
#include "../src/LocalGemini.cpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <iostream>

int main() {
  unsigned failures=0;
  for(int scenario=0;scenario<3;++scenario) {
    WiFi.state=scenario==0 ? WL_CONNECTED : 0;
    fakeWifiForced=fakeWifiPortal=fakeWifiStopped=false;
    fakeWifiPortalTimeout=-1; fakeWifiBreakAfterConfig=true;
    Serial.messages.clear(); M5.Display.history.clear();
    connectWifi(scenario==2);
    bool ok=fakeWifiStopped && fakeWifiConnectTimeout==15 && fakeWifiPortalTimeout==0 && !fakeWifiBreakAfterConfig;
    ok=ok && fakeWifiForced==(scenario==2) && fakeWifiPortal==(scenario!=0);
    ok=ok && !fakeMicActive && fakeWifiApPassword=="12345678";
    if(scenario!=0) {
      bool unlimited=false;
      for(const auto& text:M5.Display.history) if(text.find("No timeout")!=std::string::npos) unlimited=true;
      ok=ok && unlimited;
    }
    for(const auto& line:Serial.messages) ok=ok && line.find("12345678")==std::string::npos;
    std::cout<<(ok ? "PASS " : "FAIL ")<<"WiFi saved/missing/forced scenario "<<scenario<<'\n';
    if(!ok) ++failures;
  }
  Serial.messages.clear();
  wifiDiagnostic(ARDUINO_EVENT_WIFI_AP_STACONNECTED);
  wifiDiagnostic(ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
  wifiDiagnostic(ARDUINO_EVENT_WIFI_AP_STOP);
  bool events=Serial.messages==std::vector<std::string>{"GEM WIFI AP_JOIN","GEM WIFI AP_LEAVE","GEM WIFI AP_STOP"};
  std::cout<<(events ? "PASS " : "FAIL ")<<"bounded USB AP diagnostics\n";
  if(!events) ++failures;
  return failures ? 1 : 0;
}
