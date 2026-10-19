// Real LocalWake candidate/save/load code; only hardware, FS, and MFCC are replaced.
#include "../src/LocalWake.cpp"
#include <iostream>
#include <array>
unsigned checks=0,failures=0;
void check(bool value,const char* name) { ++checks;if(!value)++failures;std::cout<<(value?"PASS ":"FAIL ")<<name<<'\n'; }
std::array<int16_t,9600> a{},b{};
void fresh() { disk.clear();renameFailures.clear();failWrite=corruptWrite=corruptPublish=failRemoveBackup=failRemovePrimary=false; }
void seed(const char* name,int16_t value) { simplevox::MfccFeature f;f.coefficients.assign(120,value);simplevox::MfccEngine e;e.saveFile(name,f); }
bool completeTriple(LocalWake& w,const int16_t* data,size_t count) {
  return w.collectCandidate(data,count) && w.collectCandidate(data,count) && w.validateCandidates();
}
int main() {
  a.fill(1000);b.fill(2000);Config cfg;
  fresh();seed("/wakeword0.bin",1000);
  { LocalWake w;w.begin(cfg,false);const auto old=disk[cfg.templatePath];
    check(w.prepareCandidate(b.data(),b.size()) && !w.validateCandidates() && !w.commitCandidate(),
      "one sample cannot validate or replace the old call");
    check(w.collectCandidate(b.data(),b.size()) && !w.validateCandidates() && !w.commitCandidate() && disk[cfg.templatePath]==old,
      "two samples cannot validate or replace the old call");
    check(w.collectCandidate(b.data(),b.size()) && w.validateCandidates() && w.commitCandidate(),
      "three consistent samples permit one automatic commit"); }
  fresh();seed("/wakeword0.bin",1000);
  { LocalWake w;w.begin(cfg,false);w.prepareCandidate(b.data(),b.size());
    b.fill(1800);w.collectCandidate(b.data(),b.size());b.fill(2200);w.collectCandidate(b.data(),b.size());
    check(!w.validateCandidates() && !w.commitCandidate(),"second and third must also match each other even when both match the first"); }
  fresh();seed("/wakeword0.bin",1000);
  { LocalWake w;w.begin(cfg,false);b.fill(2000);w.prepareCandidate(b.data(),b.size());
    b.fill(2100);w.collectCandidate(b.data(),b.size());b.fill(2200);w.collectCandidate(b.data(),b.size());
    const bool ready=w.validateCandidates();const bool saved=w.commitCandidate();
    int16_t value=0;std::memcpy(&value,disk[cfg.templatePath].data()+9,sizeof(value));
    check(ready && saved && value==2100,"consistent triple stores the central representative in the existing single-template format"); }
  fresh();
  { LocalWake w;w.begin(cfg,false);b.fill(2000);w.prepareCandidate(b.data(),b.size());w.collectCandidate(b.data(),b.size());
    b.fill(2250);w.collectCandidate(b.data(),b.size());
    check(!w.validateCandidates() && !w.commitCandidate(),"distance exactly at the configured threshold is rejected"); }
  fresh();
  { LocalWake w;w.begin(cfg,false);b.fill(2000);w.prepareCandidate(b.data(),b.size());w.collectCandidate(b.data(),b.size());
    check(!w.collectCandidate(b.data(),320) && !w.validateCandidates() && !w.commitCandidate(),
      "invalid third sample discards the partial candidate");
    w.prepareCandidate(b.data(),b.size());const bool ready=completeTriple(w,b.data(),b.size());
    check(ready && !w.collectCandidate(b.data(),b.size()) && !w.commitCandidate(),"a fourth sample invalidates the bounded triple instead of reusing old validation"); }
  b.fill(2000);
  fresh();seed("/wakeword0.bin",1000);const auto original=disk[cfg.templatePath];
  {
    LocalWake w;check(w.begin(cfg,false)&&w.registered(),"existing template loads");
    check(w.prepareCandidate(b.data(),b.size()) && !w.commitCandidate() && disk[cfg.templatePath]==original,
      "candidate cannot replace old registration before three-call validation");
    check(w.collectCandidate(a.data(),a.size()) && w.collectCandidate(b.data(),b.size()) && !w.validateCandidates() && !w.commitCandidate() && disk[cfg.templatePath]==original,
      "different second call keeps old registration");
    w.cancelCandidate();check(!completeTriple(w,b.data(),b.size())&&!w.commitCandidate()&&disk[cfg.templatePath]==original,
      "cancel discards candidate and preserves old file");
    check(!w.prepareCandidate(b.data(),320)&&disk[cfg.templatePath]==original,"short call cannot become candidate");
  }
  for(int failure=0;failure<6;++failure) {
    fresh();seed("/wakeword0.bin",1000);LocalWake w;w.begin(cfg,false);
    w.prepareCandidate(b.data(),b.size());completeTriple(w,b.data(),b.size());
    if(failure==0)failWrite=true;
    if(failure==1)corruptWrite=true;
    if(failure==2)renameFailures["/wakeword0.bin->/wakeword0.bin.bak"]=1;
    if(failure==3)renameFailures["/wakeword0.bin.tmp->/wakeword0.bin"]=1;
    if(failure==4)corruptPublish=true;
    if(failure==5)failRemoveBackup=true;
    check(!w.commitCandidate()&&disk[cfg.templatePath]==original&&w.registered(),
      "write/readback/rename/cleanup fault leaves original registration usable");
  }
  fresh();seed("/wakeword0.bin",1000);
  { LocalWake w;w.begin(cfg,false);w.prepareCandidate(b.data(),b.size());completeTriple(w,b.data(),b.size());
    check(w.commitCandidate()&&disk[cfg.templatePath]!=original&&!w.commitCandidate(),"validated triple commit replaces once"); }
  { LocalWake reloaded;check(reloaded.begin(cfg,false)&&reloaded.registered(),"committed template survives a new loader"); }
  fresh();seed("/wakeword0.bin",1000);
  { LocalWake w;w.begin(cfg,false);w.prepareCandidate(b.data(),b.size());completeTriple(w,b.data(),b.size());
    renameFailures["/wakeword0.bin.tmp->/wakeword0.bin"]=1;
    renameFailures["/wakeword0.bin.bak->/wakeword0.bin"]=1;
    check(!w.commitCandidate()&&disk["/wakeword0.bin.bak"]==original,"failed rollback retains last valid backup"); }
  { LocalWake reloaded;const auto before=disk;check(reloaded.begin(cfg,false)&&reloaded.registered()&&disk==before,
      "interrupted replacement loads valid backup without boot-time writes");
    reloaded.prepareCandidate(b.data(),b.size());completeTriple(reloaded,b.data(),b.size());
    check(reloaded.commitCandidate()&&disk[cfg.templatePath]!=original,"new verified save can recover backup-only state"); }
  fresh();seed("/wakeword0.bin",1000);
  { LocalWake w;w.begin(cfg,false);w.prepareCandidate(b.data(),b.size());completeTriple(w,b.data(),b.size());
    corruptPublish=true;failRemovePrimary=true;
    check(!w.commitCandidate()&&disk["/wakeword0.bin.bak"]==original&&disk[cfg.templatePath]!=original,
      "corrupt publication and failed target removal retain authoritative old backup"); }
  { LocalWake reloaded;const auto before=disk;check(reloaded.begin(cfg,false)&&reloaded.registered()&&disk==before,
      "valid-header corrupt primary cannot override rollback backup on reboot");
    String message; bool detected=false;
    std::array<int16_t,320> voice{};
    for(size_t i=0;i<voice.size();++i)voice[i]=i%2 ? -1000 : 1000;
    for(int i=0;i<30;++i)detected=reloaded.push(voice.data(),false,message)||detected;
    std::array<int16_t,320> silence{};
    for(int i=0;i<20;++i)detected=reloaded.push(silence.data(),false,message)||detected;
    check(detected,"rebooted detector actually uses the previous call features");
    corruptPublish=failRemovePrimary=false;
    reloaded.prepareCandidate(b.data(),b.size());completeTriple(reloaded,b.data(),b.size());
    check(reloaded.commitCandidate()&&!disk.count("/wakeword0.bin.bak"),"verified retry retires interrupted publication backup"); }
  fresh();LocalWake first;first.begin(cfg,false);first.prepareCandidate(b.data(),b.size());completeTriple(first,b.data(),b.size());
  check(first.commitCandidate()&&first.registered(),"first registration also requires validated triple commit");
  std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
