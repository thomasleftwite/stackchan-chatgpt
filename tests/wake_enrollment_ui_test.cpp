#define STACKCHAN_GEMINI_DEVICE 1
#include "firmware_ui_stubs/Fakes.h"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244 4267)
#endif
#include "../src/AudioDevice.cpp"
#include "../src/FaceUI.cpp"
#include "../src/LocalGemini.cpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <iostream>
unsigned checks=0,failures=0;
void check(bool ok,const char* name) { ++checks;if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n'; }
void reset() {
  wakeInput.stop(audio);std::free(input);std::free(output);input=output=nullptr;
  fakeTime=fakeGestureStart=1000;fakeDelayBudget=1000000;fakeHeldForever=false;
  fakeMicBegins=fakeRecordRequests=fakeBeeps=0;fakeMicActive=false;
  fakeCallbackQueues=fakeCaptureGaps=0;M5.Mic.end();M5.Mic.setBufferReleaseCallback(nullptr,nullptr);
  fakePlaybackMs=fakePlaybackUntil=0;fakeRecordAction={};fakeWsLoop={};
  fakeCueDurationMs=fakeCueEndedAt=fakeMicBeganAt=0;
  fakeButtonSchedule=[](uint32_t){return false;};M5.BtnA={};M5.Display={};face.state={};
  configured=pcOnline=false;WiFi.state=0;consumed=true;enrollmentActive=false;
  fakeWakeAvailable=true;fakeEnrollCommits=0;fakeEnrollPending=fakeEnrollVerified=false;
  fakeEnrollAllowed=fakeEnrollMatch=fakeEnrollSave=true;setup();
}
void interactions(bool entry,int cancelAt=0) {
  const auto began=fakeTime;
  fakeButtonSchedule=[began,entry,cancelAt,last=std::string{},since=uint32_t(0)](uint32_t now) mutable {
    if(entry && now-began<5200) return true;
    std::string phase;
    const auto& text=M5.Display.text;
    if(text.find("Click: record call")!=std::string::npos) phase="record";
    else if(text.find("Click: try call")!=std::string::npos) phase="verify";
    else if(text.find("Click: save / hold: no")!=std::string::npos) phase="save";
    if(phase.empty()) return false;
    if(phase!=last) { last=phase;since=now; }
    const int stage=phase=="record"?1:phase=="verify"?2:3;
    return now-since>=50 && now-since<(cancelAt==stage?2200u:90u);
  };
}
void cleanAudio() {
  fakeRecordAction=[frame=unsigned(0)](int16_t* data,size_t n,uint32_t rate) mutable {
    if(n!=320 || rate!=16000) return false;
    const auto slot=frame++%100;const int amplitude=slot>=50 && slot<80 ? 2400:0;
    for(size_t i=0;i<n;++i)data[i]=static_cast<int16_t>(i%2?amplitude:-amplitude);
    return true;
  };
}
int main() {
  reset();cleanAudio();face.state.wake(fakeTime);size_t continuousCount=0;
  const auto continuousResult=enrollmentCapture(Mode::Enrolling,continuousCount);
  bool continuousData=true;
  for(size_t i=0;i<11200;++i) {
    const int expected=i<9600 ? (i%2 ? 2400 : -2400) : 0;
    if(input[i]!=expected)continuousData=false;
  }
  check(!strcmp(continuousResult,"READY") && continuousCount==11200 && continuousData && fakeCallbackQueues>50 && !fakeCaptureGaps,
    "enrollment queues successor buffers at completion and preserves every speech sample");
  reset();interactions(false);Serial.messages.clear();
  fakeRecordAction=[](int16_t* data,size_t n,uint32_t){
    for(size_t i=0;i<n;++i)data[i]=static_cast<int16_t>(i%2 ? 1600 : -1600);return true;
  };
  enrollCall();Serial.messages.clear();strlcpy(command,"GEM INFO",sizeof(command));dispatch();
  const bool replayedCal=std::any_of(Serial.messages.begin(),Serial.messages.end(),[](const std::string& line){
    return line.find("GEM ENROLL CAL rms=1600 threshold=4800 ")==0;
  });
  const bool replayedResult=std::any_of(Serial.messages.begin(),Serial.messages.end(),[](const std::string& line){
    return line.find("GEM ENROLL result=NOISY")==0;
  });
  const bool replayedStep=std::find(Serial.messages.begin(),Serial.messages.end(),"GEM ENROLL STEP record")!=Serial.messages.end();
  const bool replayedAudio=std::any_of(Serial.messages.begin(),Serial.messages.end(),[](const std::string& line){
    return line.find("GEM ENROLL AUDIO samples=0 voiced=0 ")==0;
  });
  check(replayedCal && replayedResult && replayedStep && replayedAudio,"state query replays calibration stage and sample counts after transient logs are missed");
  reset();cleanAudio();fakeCueDurationMs=500;
  face.state.wake(fakeTime);size_t settlingCount=0;
  const auto settlingResult=enrollmentCapture(Mode::Enrolling,settlingCount);
  check(!strcmp(settlingResult,"READY") && settlingCount>0 && fakeMicBeganAt>=fakeCueEndedAt+700,
    "enrollment microphone starts only after cue completes and a 700ms quiet gap");
  reset();cleanAudio();fakeCueDurationMs=500;face.state.wake(fakeTime);
  fakeButtonSchedule=[pressed=fakeTime+600](uint32_t now){return now>=pressed;};
  size_t cancelledCount=0;
  check(!strcmp(enrollmentCapture(Mode::Enrolling,cancelledCount),"CANCEL") && !fakeMicBegins && !cancelledCount,
    "cancel during the speaker settling gap never starts microphone recording");
  reset();cleanAudio();face.state.wake(fakeTime);
  fakeButtonSchedule=[pressed=fakeTime+900](uint32_t now){return now>=pressed;};
  check(!strcmp(enrollmentCapture(Mode::Enrolling,cancelledCount),"CANCEL") && !fakeMicActive &&
    M5.Mic.pending.empty() && !M5.Mic.onRelease && !capturing.load(),
    "cancel during continuous capture drains requests and detaches callback before returning");
  reset();cleanAudio();interactions(true);const auto before=generation;
  for(unsigned i=0;i<2700;++i) { loop();if(fakeEnrollCommits)break; }
  check(fakeEnrollCommits==1 && !enrollmentActive && face.state.asleep && !fakeMicActive && generation==before && fakeMicBegins==2,
    "sleep five-second gesture records verifies and explicitly saves offline without cloud traffic");
  check(!fakeEnrollPending && inputCount==0 && input[0]==0,"completion wipes candidate and raw question buffer");
  for(int stage=1;stage<=3;++stage) {
    reset();cleanAudio();interactions(false,stage);enrollCall();
    check(!fakeEnrollCommits && fakeWakeAvailable && !fakeEnrollPending && face.state.asleep && !fakeMicActive,
      "cancel at record verification or save confirmation preserves previous registration");
  }
  reset();interactions(false);enrollCall();
  check(!fakeEnrollCommits && fakeWakeAvailable && !fakeMicActive,"microphone failure keeps old call");
  reset();cleanAudio();fakeEnrollMatch=false;interactions(false);enrollCall();
  check(!fakeEnrollCommits && fakeWakeAvailable && !fakeEnrollPending,"different verification call cannot reach save");
  reset();cleanAudio();fakeEnrollSave=false;interactions(false);enrollCall();
  check(!fakeEnrollCommits && fakeWakeAvailable && !fakeEnrollPending,"save failure keeps previous registration available");
  reset();enrollCall();
  check(!fakeEnrollCommits && fakeTime<35000 && face.state.asleep,"unanswered registration menu times out safely");
  reset();enrollmentActive=true;face.state.wake(fakeTime);face.set(Mode::Enrolling);
  const auto gen=generation;strlcpy(command,"GEM PROBE",sizeof(command));dispatch();
  check(generation==gen && face.state.mode==Mode::Enrolling,"USB probe cannot enter cloud path while enrolling");
  strlcpy(command,"GEM STOP",sizeof(command));dispatch();
  check(face.state.mode==Mode::Enrolling && !configured,"PC stop does not interrupt local registration state");
  enrollmentActive=false;
  std::free(input);std::free(output);input=output=nullptr;
  std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
