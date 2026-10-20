#define STACKCHAN_WIRELESS 1
#include "wireless_stubs/Fakes.h"
#include "../src/WirelessLink.cpp"
#include <iostream>
int main(){
 unsigned failures=0;
 auto check=[&](bool ok,const char* name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';if(!ok)++failures;};
 WirelessLink first;check(first.begin()&&strlen(first.key())==32&&!first.paired(),"fresh device generates persistent random credential");
 const std::string original=first.key();first.showCode();check(M5.Display.contents.find(original.substr(0,8))!=std::string::npos&&Serial.messages.empty(),"pairing credential shown only on device display");
 httpStatus=401;check(!first.online()&&!first.paired(),"failed authentication never confirms pairing");
 fakeNow+=6000;httpStatus=200;check(first.online()&&first.paired(),"authenticated TLS check commits pairing");
 check(tlsKey==original&&tlsIdentity=="stackchan-v1"&&lastUrl=="https://192.168.137.1:48443/healthz","DHCP gateway and fixed TLS port require no USB configuration");
 WirelessLink reboot;check(reboot.begin()&&reboot.paired()&&reboot.key()==original,"reboot retains the enrolled credential");
 check(reboot.online(),"reboot reconnects with stored credential");auto before=httpCalls;fakeNow+=6000;reboot.online();check(httpCalls==before,"authenticated idle does not block wake capture with TLS polling");
 storage["wake-fixture"]="preserved";check(reboot.resetPairing()&&!reboot.paired()&&reboot.key()!=original&&storage["wake-fixture"]=="preserved","credential rotation invalidates old key and preserves unrelated state");
 for(const auto& line:Serial.messages)check(line.find(original)==std::string::npos,"USB events never contain the device credential");
 return failures?1:0;
}
