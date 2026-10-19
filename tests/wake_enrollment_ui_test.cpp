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
std::vector<uint32_t> speechGaps() {
  std::vector<uint32_t> gaps;
  for(const auto& line:Serial.messages) { unsigned prep=0,gap=0;
    if(std::sscanf(line.c_str(),"GEM CAPTURE TIMING prep=%u gap=%u",&prep,&gap)==2) gaps.push_back(gap);
  }
  return gaps;
}
void check(bool ok,const char* name) { ++checks;if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n'; }
bool shown(const char* text) {
  return std::any_of(M5.Display.history.begin(),M5.Display.history.end(),[text](const std::string& value){return value.find(text)!=std::string::npos;});
}
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
  fakeEnrollAllowed=fakeEnrollMatch=fakeEnrollSave=true;fakeEnrollSamples=fakeEnrollValidations=0;Serial.messages.clear();setup();
}
void interactions(bool entry,int cancelAt=0,bool retryOnce=false) {
  const auto began=fakeTime;
  fakeButtonSchedule=[began,entry,cancelAt,retryOnce,retried=false,last=std::string{},since=uint32_t(0)](uint32_t now) mutable {
    if(entry && now-began<5200) return true;
    std::string phase;
    const auto& text=M5.Display.text;
    if(text.find("1/3 Click: record")!=std::string::npos) phase="record1";
    else if(text.find("2/3 Click: record")!=std::string::npos) phase="record2";
    else if(text.find("3/3 Click: record")!=std::string::npos) phase="record3";
    else if(text.find("Click: retry")!=std::string::npos && retryOnce && !retried) phase="retry";
    if(phase.empty()) return false;
    if(phase!=last) { last=phase;since=now; }
    if(phase=="retry" && now-since>=90) { retried=true;fakeEnrollMatch=true;return false; }
    const int stage=phase=="record1"?1:phase=="record2"?2:3;
    return now-since>=50 && now-since<(cancelAt==stage?2200u:90u);
  };
}
void cleanAudio(int quiet=0,bool transient=false) {
  fakeRecordAction=[quiet,transient](int16_t* data,size_t n,uint32_t rate) {
    if(n!=320 || rate!=16000)return false;
    const uint32_t since=fakeTime-fakeCueEndedAt;
    const int amplitude=fakeBeeps && transient && since<650 ? 30000 : fakeBeeps && since>700 && since<=1300 ? 2400:quiet;
    for(size_t i=0;i<n;++i)data[i]=static_cast<int16_t>(i%2?amplitude:-amplitude);
    return true;
  };
}
int main() {
  reset();face.state.wake(fakeTime);face.set(Mode::CueWait,"Beep then wait 0.7s");
  check(face.state.microphoneAllowed() && !face.state.mayStart() && shown("Get ready") && shown("Beep then wait 0.7s"),
    "cue pause has a ready prompt distinct from preparation and cannot start another question");
  reset();face.state.wake(fakeTime);fakeCueDurationMs=500;cleanAudio(100);
  size_t naturalCount=0;
  check(!strcmp(enrollmentCapture(Mode::Enrolling,naturalCount),"READY") && naturalCount>=9600,
    "enrollment accepts a natural 600ms call starting 700ms after the completed beep");
  reset();face.state.wake(fakeTime);face.state.set(Mode::WakeWait,fakeTime);fakeCueDurationMs=500;cleanAudio(100);
  check(recordQuestion() && inputCount>=9600,
    "normal question accepts speech starting 700ms after the completed beep without a later calibration");
  reset();face.state.wake(fakeTime);fakeCueDurationMs=500;cleanAudio(100,true);
  naturalCount=0;const auto transientResult=enrollmentCapture(Mode::Enrolling,naturalCount);
  check(!strcmp(transientResult,"READY") && naturalCount>=9600 &&
    std::all_of(input,input+naturalCount,[](int16_t value){return value>=-2400 && value<=2400;}),
    "post-beep startup transients are excluded from the enrollment model without dropping the call");
  reset();face.state.wake(fakeTime);face.state.set(Mode::WakeWait,fakeTime);fakeCueDurationMs=500;cleanAudio(100,true);
  check(recordQuestion() && inputCount>=9600 &&
    std::all_of(input,input+inputCount,[](int16_t value){return value>=-2400 && value<=2400;}),
    "post-beep startup transients cannot reach the normal question API input");
  reset();face.state.wake(fakeTime);fakeCueDurationMs=500;cleanAudio();
  fakeButtonSchedule=[](uint32_t now){return fakeBeeps && now-fakeCueEndedAt>=300;};
  naturalCount=0;
  check(!strcmp(enrollmentCapture(Mode::Enrolling,naturalCount),"CANCEL") && fakeBeeps==1 && !naturalCount &&
    !fakeMicActive && !M5.Mic.onRelease && M5.Mic.pending.empty() && !capturing.load(),
    "cancel in the post-beep pause shuts down restarted microphone and pending callbacks");
  reset();face.state.wake(fakeTime);fakeCueDurationMs=500;cleanAudio();
  const auto healthyAudio=fakeRecordAction;
  fakeRecordAction=[healthyAudio](int16_t* data,size_t n,uint32_t rate){return !fakeBeeps && healthyAudio(data,n,rate);};
  naturalCount=0;
  check(!strcmp(enrollmentCapture(Mode::Enrolling,naturalCount),"IO") && fakeBeeps==1 && !naturalCount && !fakeMicActive,
    "a stalled post-beep microphone restart fails at the 700ms gate instead of accepting speech late");
  reset();cleanAudio();face.state.wake(fakeTime);size_t continuousCount=0;
  const auto continuousResult=enrollmentCapture(Mode::Enrolling,continuousCount);
  bool continuousData=true;
  for(size_t i=0;i<11520;++i) {
    const int expected=i>=320 && i<9920 ? (i%2 ? 2400 : -2400) : 0;
    if(input[i]!=expected)continuousData=false;
  }
  check(!strcmp(continuousResult,"READY") && continuousCount==11520 && continuousData && fakeCallbackQueues>50 && !fakeCaptureGaps,
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
  const bool replayedStep=std::find(Serial.messages.begin(),Serial.messages.end(),"GEM ENROLL STEP retry")!=Serial.messages.end();
  const bool replayedAudio=std::any_of(Serial.messages.begin(),Serial.messages.end(),[](const std::string& line){
    return line.find("GEM ENROLL AUDIO samples=0 voiced=0 ")==0;
  });
  check(replayedCal && replayedResult && replayedStep && replayedAudio && !fakeBeeps,
    "preparation noise rejects enrollment before any start beep and retained status can be queried");
  reset();cleanAudio();fakeCueDurationMs=500;
  face.state.wake(fakeTime);size_t settlingCount=0;
  const auto settlingResult=enrollmentCapture(Mode::Enrolling,settlingCount);
  check(!strcmp(settlingResult,"READY") && settlingCount>0 && speechGaps().size()==1 && speechGaps()[0]>=700 && speechGaps()[0]<=702,
    "enrollment opens the speech gate 700ms after the cue with microphone startup inside that pause");
  reset();cleanAudio();fakeCueDurationMs=500;face.state.wake(fakeTime);
  fakeButtonSchedule=[pressed=fakeTime+600](uint32_t now){return now>=pressed;};
  size_t cancelledCount=0;
  check(!strcmp(enrollmentCapture(Mode::Enrolling,cancelledCount),"CANCEL") && !fakeBeeps && !cancelledCount && !fakeMicActive,
    "cancel during preparation never plays the start cue or retains input");
  reset();cleanAudio();face.state.wake(fakeTime);
  fakeButtonSchedule=[pressed=fakeTime+900](uint32_t now){return now>=pressed;};
  check(!strcmp(enrollmentCapture(Mode::Enrolling,cancelledCount),"CANCEL") && !fakeMicActive &&
    M5.Mic.pending.empty() && !M5.Mic.onRelease && !capturing.load(),
    "cancel during continuous capture drains requests and detaches callback before returning");
  reset();cleanAudio();interactions(true);fakeCueDurationMs=500;const auto before=generation;
  for(unsigned i=0;i<2700;++i) { loop();if(fakeEnrollCommits)break; }
  check(fakeEnrollCommits==1 && !enrollmentActive && face.state.asleep && !fakeMicActive && generation==before && fakeMicBegins==6 && fakeEnrollValidations==1,
    "three identical record actions validate and automatically save offline without a fourth click");
  const auto gaps=speechGaps();
  check(gaps.size()==3 && std::all_of(gaps.begin(),gaps.end(),[](uint32_t gap){return gap>=700 && gap<=702;}),
    "all three recordings wait at least 700ms after their complete cue");
  check(shown("1/3 Click: record") && shown("2/3 Click: record") && shown("3/3 Click: record") &&
    shown("1/3 Say same call") && shown("2/3 Say same call") && shown("3/3 Say same call") &&
    !shown("Click: try call") && !shown("Click: save"),"all three numbered stages use identical instructions and omit the old verification and save menus");
  check(!fakeEnrollPending && inputCount==0 && input[0]==0,"completion wipes candidate and raw question buffer");
  for(int stage=1;stage<=3;++stage) {
    reset();cleanAudio();interactions(false,stage);enrollCall();
    check(!fakeEnrollCommits && fakeWakeAvailable && !fakeEnrollPending && face.state.asleep && !fakeMicActive,
      "cancel before any of the three recordings preserves previous registration");
  }
  reset();interactions(false);enrollCall();
  check(!fakeEnrollCommits && fakeWakeAvailable && !fakeMicActive,"microphone failure keeps old call");
  reset();cleanAudio();fakeEnrollMatch=false;interactions(false);enrollCall();
  check(!fakeEnrollCommits && fakeWakeAvailable && !fakeEnrollPending && fakeMicBegins==6 && fakeEnrollValidations==1 && shown("Click: retry"),
    "inconsistent triple cannot reach automatic save and offers retry");
  reset();cleanAudio();fakeEnrollMatch=false;interactions(false,0,true);enrollCall();
  check(fakeEnrollCommits==1 && fakeMicBegins==12 && fakeEnrollValidations==2 && !fakeEnrollPending && face.state.asleep,
    "retry discards the failed triple and collects three fresh calls before saving");
  for(unsigned failedSample: {2u,3u}) {
    reset();cleanAudio();const auto clean=fakeRecordAction;
    fakeRecordAction=[clean,failedSample](int16_t* data,size_t n,uint32_t rate){
      return fakeMicBegins!=failedSample*2 && clean(data,n,rate);
    };
    interactions(false);enrollCall();
    check(!fakeEnrollCommits && fakeWakeAvailable && !fakeEnrollPending && !fakeEnrollSamples && !fakeEnrollValidations && !fakeMicActive,
      "failure in a later recording clears all partial samples without validating or changing the old call");
  }
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
