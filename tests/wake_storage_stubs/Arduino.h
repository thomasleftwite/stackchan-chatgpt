#pragma once
#include <string>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdio>
class String : public std::string {
 public: using std::string::string; String(const std::string& s):std::string(s){} bool isEmpty() const { return empty(); }
};
inline String operator+(const String& a,const char* b) { return String(std::string(a)+b); }
inline String operator+(const char* a,const String& b) { return String(a+std::string(b)); }
inline String operator+(const String& a,const String& b) { return String(std::string(a)+std::string(b)); }
inline void* ps_malloc(size_t n) { return std::malloc(n); }
struct SerialFake { template<class... T> void printf(const char*,T...) {} };
inline SerialFake Serial;
