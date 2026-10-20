#pragma once
#include <string>
#include <map>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <algorithm>
class String:public std::string {
public:
 using std::string::string;
 String(const std::string& x):std::string(x){}
 String(unsigned x):std::string(std::to_string(x)){}
};
inline uint32_t fakeNow=1000;inline uint32_t millis(){return fakeNow;}
inline size_t strlcpy(char* out,const char* in,size_t n){auto size=strlen(in);if(n){auto copied=std::min(size,n-1);memcpy(out,in,copied);out[copied]=0;}return size;}
inline int entropy=0;inline void esp_fill_random(void* out,size_t n){auto* p=static_cast<uint8_t*>(out);for(size_t i=0;i<n;++i)p[i]=static_cast<uint8_t>(++entropy);}
inline std::map<std::string,std::string> storage;
struct Preferences {
 bool begin(const char*,bool){return true;} void end(){}
 String getString(const char* key,const char* fallback){return storage.count(key)?storage[key]:fallback;}
 bool getBool(const char* key,bool fallback){return storage.count(key)?storage[key]=="1":fallback;}
 size_t putString(const char* key,const char* value){storage[key]=value;return strlen(value);}
 size_t putBool(const char* key,bool value){storage[key]=value?"1":"0";return 1;}
 bool remove(const char* key){return storage.erase(key)==1;}
};
constexpr int WL_CONNECTED=3,TFT_BLACK=0;
struct Gateway{String toString(){return "192.168.137.1";}};
struct FakeWiFi {int state=3;int status(){return state;}Gateway gatewayIP(){return {};}};
inline FakeWiFi WiFi;
inline std::string tlsKey,tlsIdentity,lastUrl;inline int httpStatus=200,httpCalls=0;
struct WiFiClientSecure{void setPreSharedKey(const char* identity,const char* key){tlsIdentity=identity;tlsKey=key;}void setHandshakeTimeout(int){}};
struct HTTPClient{
 void setConnectTimeout(int){}void setTimeout(int){}
 bool begin(WiFiClientSecure&,const String& url){lastUrl=url;return true;}
 void addHeader(const char*,const String&){}int GET(){++httpCalls;return httpStatus;}void end(){}
};
struct FakeDisplay{std::string contents;void fillScreen(int){contents.clear();}void setTextSize(int){}void setCursor(int,int){}void print(const char* text){contents+=text;}};
struct FakeM5{FakeDisplay Display;};inline FakeM5 M5;
struct FakeSerial{std::vector<std::string> messages;void println(const char* text){messages.emplace_back(text);}};inline FakeSerial Serial;
