#pragma once
#include "SPIFFS.h"
#include <vector>
namespace simplevox {
struct MfccConfig {};
struct MfccFeature {
  std::vector<int16_t> coefficients;
  int size() const { return 10; } int dimension() const { return 12; }
  const int16_t* feature(int i) const { return coefficients.data()+i*12; }
};
struct MfccEngine {
  bool init(MfccConfig) { return true; }
  MfccFeature* create(const int16_t* data,int) { auto* f=new MfccFeature;f->coefficients.assign(120,data[0]);return f; }
  static std::string path(const char* v) { std::string p=v; return p.rfind("/spiffs",0)==0 ? p.substr(7) : p; }
  bool saveFile(const char* p,const MfccFeature& f) {
    if(failWrite) return false;
    auto& data=disk[path(p)];data.assign(249,0);data[0]=1;int32_t n=10,d=12;
    std::memcpy(data.data()+1,&n,4);std::memcpy(data.data()+5,&d,4);std::memcpy(data.data()+9,f.coefficients.data(),240);
    if(corruptWrite) data.back()^=1;
    return true;
  }
  MfccFeature* loadFile(const char* p) {
    const auto name=path(p);if(!disk.count(name) || disk[name].size()!=249) return nullptr;
    auto* f=new MfccFeature; f->coefficients.resize(120);std::memcpy(f->coefficients.data(),disk[name].data()+9,240);return f;
  }
};
inline uint32_t calcDTW(const MfccFeature& a,const MfccFeature& b) { return a.coefficients==b.coefficients ? 0 : 1000; }
}
