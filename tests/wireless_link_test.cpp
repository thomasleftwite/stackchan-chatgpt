#define STACKCHAN_WIRELESS 1
#include "wireless_stubs/Fakes.h"
#include "../src/WirelessLink.cpp"
#include <iostream>
int main(){
 unsigned failures=0;
 auto check=[&](bool ok,const char* name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';if(!ok)++failures;};
 storage["wake-fixture"]="preserved";storage["key"]="old-device-key";const auto oldStorage=storage;
 WirelessLink first;check(first.begin()&&first.ready(),"fresh device is ready without an app credential");
 check(storage==oldStorage&&M5.Display.contents.empty(),"startup preserves NVS and wake state and displays no pairing code");
 httpStatus=403;check(!first.online(),"host rejection cannot become online");
 const auto rejectedCalls=httpCalls;first.online();check(httpCalls==rejectedCalls,"failed checks use bounded retry interval");
 fakeNow+=6000;httpStatus=200;check(first.online(),"hotspot host health admits the connected device automatically");
 check(lastUrl=="http://192.168.137.1:48443/healthz"&&tlsKey.empty(),"DHCP gateway requires neither fixed Windows IP nor PSK");
 auto before=httpCalls;fakeNow+=6000;first.online();check(httpCalls==before,"idle does not interrupt microphone with health polling");
 WiFi.state=0;check(!first.online(),"Wi-Fi loss clears online state");WiFi.state=3;fakeNow+=6000;
 check(first.online(),"Wi-Fi recovery reconnects without a code");
 WirelessLink reboot;check(reboot.begin()&&reboot.online(),"reboot needs no registration step");
 check(storage==oldStorage&&Serial.messages.empty(),"network checks do not overwrite data or print credentials");
 return failures?1:0;
}
