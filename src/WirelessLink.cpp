#ifdef STACKCHAN_WIRELESS
#include "WirelessLink.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

bool WirelessLink::begin() {
  // Leave old app-pairing NVS, Wi-Fi credentials and wake templates untouched.
  initialized_=true;online_=false;checked_=0;return true;
}
bool WirelessLink::online() {
  if(!ready() || WiFi.status()!=WL_CONNECTED) return online_=false;
  if(online_) return true;
  if(checked_ && millis()-checked_<5000) return false;
  checked_=millis();
  WiFiClient client;
  HTTPClient request;request.setConnectTimeout(2000);request.setTimeout(2000);
  const String url=String("http://")+WiFi.gatewayIP().toString()+":"+String(port)+"/healthz";
  if(!request.begin(client,url)) return online_=false;
  online_=request.GET()==200;request.end();return online_;
}
#endif
