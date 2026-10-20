#pragma once
#include "Fakes.h"
inline bool fakeWifiForced = false, fakeWifiPortal = false, fakeWifiStopped = false;
inline int fakeWifiConnectTimeout = -1, fakeWifiPortalTimeout = -1;
inline bool fakeWifiBreakAfterConfig = true;
inline std::string fakeWifiApPassword;
class WiFiManager {
  std::function<void(WiFiManager*)> callback;
public:
  void setDebugOutput(bool) {}
  void setRestorePersistent(bool) {}
  void setConnectTimeout(int n) { fakeWifiConnectTimeout=n; }
  void setConfigPortalTimeout(int n) { fakeWifiPortalTimeout=n; }
  void setBreakAfterConfig(bool value) { fakeWifiBreakAfterConfig=value; }
  void setAPCallback(std::function<void(WiFiManager*)> fn) { callback=fn; }
  bool startConfigPortal(const char*, const char* password) { fakeWifiApPassword=password; fakeWifiForced=true; fakeWifiPortal=true; if(callback) callback(this); return WiFi.status()==WL_CONNECTED; }
  bool autoConnect(const char*, const char* password) { fakeWifiApPassword=password; if(WiFi.status()!=WL_CONNECTED) { fakeWifiPortal=true; if(callback) callback(this); } return WiFi.status()==WL_CONNECTED; }
  void stopConfigPortal() { fakeWifiStopped=true; }
};
