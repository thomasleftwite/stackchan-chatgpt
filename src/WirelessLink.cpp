#ifdef STACKCHAN_WIRELESS
#include "WirelessLink.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <esp_system.h>
#include <M5Unified.h>

bool WirelessLink::begin() {
  Preferences store;
  if(!store.begin("stacklink",false)) return false;
  String saved=store.getString("key","");
  bool valid=saved.length()==32;
  for(unsigned i=0;i<saved.length();++i) valid=valid && ((saved[i]>='0' && saved[i]<='9') || (saved[i]>='a' && saved[i]<='f'));
  if(!valid) {
    uint8_t bytes[16];esp_fill_random(bytes,sizeof(bytes));
    for(unsigned i=0;i<16;++i) snprintf(key_+i*2,3,"%02x",bytes[i]);
    memset(bytes,0,sizeof(bytes));
    // A commit marker prevents a partially written enrollment from being used.
    store.putBool("paired",false);
    if(store.putString("key",key_)!=32) {memset(key_,0,sizeof(key_));store.end();return false;}
  } else strlcpy(key_,saved.c_str(),sizeof(key_));
  paired_=store.getBool("paired",false);store.end();return true;
}
void WirelessLink::showCode() {
  if(!ready() || codeShown_) return;
  codeShown_=true;
  M5.Display.fillScreen(TFT_BLACK);M5.Display.setTextSize(1);M5.Display.setCursor(2,2);
  M5.Display.print("Pair with PC\nEnter code in host:\n");
  for(unsigned i=0;i<4;++i) {char row[10]{};memcpy(row,key_+i*8,8);row[8]='\n';M5.Display.print(row);}
  M5.Display.print("\nWaiting for PC...");
}
bool WirelessLink::online() {
  if(!ready() || WiFi.status()!=WL_CONNECTED) return online_=false;
  // Do not block wake-word capture with periodic TLS handshakes. Verify once,
  // and retry only after Wi-Fi loss or an actual conversation failure.
  if(online_) return true;
  if(checked_ && millis()-checked_<5000) return false;
  checked_=millis();
  WiFiClientSecure client;client.setPreSharedKey("stackchan-v1",key_);client.setHandshakeTimeout(2);
  HTTPClient request;request.setConnectTimeout(2000);request.setTimeout(2000);
  const String url=String("https://")+WiFi.gatewayIP().toString()+":"+String(port)+"/healthz";
  if(!request.begin(client,url)) return online_=false;
  request.addHeader("Authorization",String("Bearer ")+key_);
  online_=request.GET()==200;request.end();
  if(online_ && !paired_) {
    Preferences store;
    if(store.begin("stacklink",false)) {paired_=store.putBool("paired",true)==1;store.end();}
    if(paired_) Serial.println("GEM PAIR SAVED");
  }
  return online_;
}
bool WirelessLink::resetPairing() {
  Preferences store;if(!store.begin("stacklink",false)) return false;
  // Rotate only this device credential. Wi-Fi and wake registrations are separate.
  store.putBool("paired",false);bool ok=store.remove("key");store.end();
  paired_=online_=codeShown_=false;checked_=0;memset(key_,0,sizeof(key_));return ok && begin();
}
#endif
