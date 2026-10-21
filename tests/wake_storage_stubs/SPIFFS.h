#pragma once
#include "Arduino.h"
#include <map>
#include <vector>
inline std::map<std::string,std::vector<uint8_t>> disk;
inline std::map<std::string,int> renameFailures;
inline bool failWrite=false,corruptWrite=false,corruptPublish=false,failRemoveBackup=false,failRemovePrimary=false;
struct File {
  std::string path; size_t offset=0; bool opened=false;
  explicit operator bool() const { return opened; }
  bool isDirectory() const { return false; }
  size_t size() const { return opened ? disk[path].size() : 0; }
  size_t read(uint8_t* out,size_t count) { if(!opened) return 0; auto& data=disk[path];
    const auto available=offset<data.size() ? data.size()-offset : 0; const auto n=count<available?count:available;
    std::memcpy(out,data.data()+offset,n); offset+=n; return n; }
  void close() { opened=false; }
};
struct Filesystem {
  bool begin(bool) { return true; }
  File open(const String& p,const char*) { return File{p,0,disk.count(p)!=0}; }
  bool remove(const String& p) {
    if(failRemoveBackup && p.find(".bak")!=std::string::npos) return false;
    if(failRemovePrimary && p=="/wakeword0.bin") return false;
    return disk.erase(p)!=0;
  }
  bool rename(const String& a,const String& b) {
    auto key=std::string(a)+"->"+std::string(b);
    if(renameFailures[key]>0) { --renameFailures[key]; return false; }
    if(!disk.count(a) || disk.count(b)) return false;
    disk[b]=disk[a];disk.erase(a);
    if(corruptPublish && a.find(".tmp")!=std::string::npos && !disk[b].empty()) disk[b].back()^=1;
    return true;
  }
};
inline Filesystem SPIFFS;
