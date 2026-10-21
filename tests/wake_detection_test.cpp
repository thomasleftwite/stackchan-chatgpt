// Real stream, segmentation and lifecycle; synthetic audio and substituted MFCC/FS.
#include "../include/GeminiWake.h"
#include "../src/AudioDevice.cpp"
#include "../src/LocalWake.cpp"
#include <array>
#include <iostream>
bool Config::load(String&,bool) { return true; }
unsigned checks=0,failures=0;
void check(bool value,const char* name) { ++checks;if(!value)++failures;
  std::cout<<(value?"PASS ":"FAIL ")<<name<<'\n'; }
std::array<int16_t,320> signal(int level) { std::array<int16_t,320> data{};
  for(size_t i=0;i<data.size();++i)data[i]=static_cast<int16_t>((i&1)?-level:level);return data; }
bool frame(GeminiWake& wake,AudioDevice& audio,int level) {
  const auto data=signal(level);fakeWakeTime+=20;M5.Mic.finish(data.data());return wake.poll(audio);
}
int main() {
  simplevox::MfccEngine engine;simplevox::MfccFeature feature;feature.coefficients.assign(120,100);
  engine.saveFile("/wakeword0.bin",feature);const auto saved=disk;
  AudioDevice audio;GeminiWake wake;wake.begin();check(wake.available(),"saved template loads without modification");
  failMicBegin=true;wake.poll(audio);failMicBegin=false;const auto failedBegins=micBegins;
  wake.poll(audio);check(wake.available()&&micBegins==failedBegins,"failed microphone start keeps registration and backs off");
  fakeWakeTime+=2000;wake.poll(audio);check(M5.Mic.running&&M5.Mic.pending,"microphone start retries after two seconds");
  const auto data=signal(100);M5.Mic.finish(data.data());
  check(M5.Mic.pending!=nullptr,"producer queues successor before foreground polling");wake.poll(audio);
  for(unsigned i=0;i<49;++i)frame(wake,audio,100);
  char state[32]{},metrics[32]{};wake.diagnostics(state,sizeof(state),metrics,sizeof(metrics));
  check(std::string(state)=="Say your call"&&std::string(metrics).find("T750")!=std::string::npos,"warmup uses enrollment ambient gate and reports threshold");
  for(unsigned i=0;i<5;++i)frame(wake,audio,100);
  for(unsigned i=0;i<30;++i)frame(wake,audio,2400);
  bool matched=false;for(unsigned i=0;i<20;++i)matched=frame(wake,audio,100)||matched;
  check(matched&&!M5.Mic.running&&!M5.Mic.callback,"matching call stops stream and removes callback before question capture");
  wake.poll(audio);for(unsigned i=0;i<55;++i)frame(wake,audio,600);
  wake.diagnostics(state,sizeof(state),metrics,sizeof(metrics));
  check(std::string(state)=="Say your call"&&std::string(metrics).find("T1800")!=std::string::npos,"background RMS600 remains waiting at calibrated threshold1800");
  for(unsigned i=0;i<30;++i)frame(wake,audio,2400);
  bool rejectedMatch=false;for(unsigned i=0;i<20;++i)rejectedMatch=frame(wake,audio,600)||rejectedMatch;
  wake.diagnostics(state,sizeof(state),metrics,sizeof(metrics));
  check(!rejectedMatch&&std::string(state)=="Call no match"&&std::string(metrics).find("D500")!=std::string::npos,"nonmatching synthetic features report rejection and exact distance");
  failMicRecord=true;frame(wake,audio,600);wake.poll(audio);failMicRecord=false;
  check(wake.available()&&!M5.Mic.running&&!M5.Mic.callback,"successor queue failure stops safely without disabling saved call");
  fakeWakeTime+=2000;wake.poll(audio);check(M5.Mic.pending!=nullptr,"record failure recovers after backoff");
  // Foreground is paused while the producer continues to fill the bounded ring.
  for(unsigned i=0;i<32;++i)M5.Mic.finish(data.data());wake.poll(audio);
  check(!M5.Mic.running&&!M5.Mic.callback,"full ring fails closed instead of overwriting unread frames");
  fakeWakeTime+=2000;wake.poll(audio);fakeWakeTime+=1001;wake.poll(audio);
  wake.diagnostics(state,sizeof(state),metrics,sizeof(metrics));
  check(std::string(state)=="Mic stalled retry"&&!M5.Mic.running,"stalled producer has bounded failure and retry");
  fakeWakeTime+=2000;wake.poll(audio);wake.stop(audio);
  check(!M5.Mic.running&&!M5.Mic.callback&&disk==saved,"sleep stop joins capture and preserves all stored data");
  fakeWakeTime+=2000;failMicRecord=true;wake.poll(audio);failMicRecord=false;
  check(wake.available()&&!M5.Mic.running&&!M5.Mic.callback,"initial queue failure cleans up and retains the saved call");
  fakeWakeTime+=2000;wake.poll(audio);
  for(unsigned i=0;i<50;++i)frame(wake,audio,1600);
  wake.diagnostics(state,sizeof(state),metrics,sizeof(metrics));
  check(std::string(state)=="Room too loud","excessive calibration noise is reported and cannot match");
  wake.stop(audio);check(disk==saved,"all failure and detection paths leave stored settings unchanged");
  std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
