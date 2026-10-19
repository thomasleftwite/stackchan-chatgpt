#include "WakeEnrollment.h"
#include <array>
#include <iostream>
unsigned checks=0,failures=0;
void check(bool value,const char* name) { ++checks;if(!value)++failures;std::cout<<(value?"PASS ":"FAIL ")<<name<<'\n'; }
std::array<int16_t,48000> buffer{};
std::array<int16_t,320> frame{};
void feed(WakeEnrollmentCapture& w,int amplitude,int frames,bool dc=false) {
  for(size_t i=0;i<frame.size();++i) frame[i]=static_cast<int16_t>(dc || i%2 ? amplitude : -amplitude);
  for(int i=0;i<frames;++i)w.push(frame.data(),frame.size());
}
int main() {
  using R=WakeEnrollmentCapture::Result;
  { WakeEnrollmentCapture w(buffer.data(),buffer.size());feed(w,0,50);feed(w,0,250);
    check(w.result()==R::Empty && !w.samples(),"no speech exits locally without a candidate"); }
  { WakeEnrollmentCapture w(buffer.data(),buffer.size());feed(w,100,50);feed(w,100,10);feed(w,2400,30);feed(w,100,20);
    check(w.result()==R::Ready && w.samples()==12800,"clean call keeps 100ms preroll and trims trailing silence");
    feed(w,32000,100);check(w.samples()==12800,"finished capture cannot absorb following audio"); }
  { WakeEnrollmentCapture w(buffer.data(),buffer.size());feed(w,0,50);feed(w,2400,10);feed(w,0,20);
    check(w.result()==R::Short && !w.samples(),"brief noise or too-short call cannot be registered"); }
  { WakeEnrollmentCapture w(buffer.data(),buffer.size());feed(w,1600,50);
    check(w.result()==R::Noisy,"loud calibration prevents enrollment"); }
  { WakeEnrollmentCapture w(buffer.data(),buffer.size());feed(w,0,50);feed(w,32767,30);feed(w,0,20);
    check(w.result()==R::Clipped && !w.samples(),"clipped microphone audio is rejected"); }
  { WakeEnrollmentCapture w(buffer.data(),buffer.size());feed(w,0,50);feed(w,2400,200);
    check(w.result()==R::Long && !w.samples(),"sustained speech hits the bounded three-second storage limit"); }
  { WakeEnrollmentCapture w(buffer.data(),buffer.size());feed(w,0,50);feed(w,2000,250,true);
    check(w.result()==R::Empty,"DC offset is not accepted as a call"); }
  { WakeEnrollmentCapture w(buffer.data(),buffer.size());w.push(nullptr,320);
    check(w.result()==R::Invalid,"invalid microphone frame fails closed"); }
  std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
