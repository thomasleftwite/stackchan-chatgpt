#include "LocalWake.h"
#include <SPIFFS.h>

bool LocalWake::validFile(const String& path) {
  File f = SPIFFS.open(path, "r");
  uint8_t tag = 0; int32_t frames = 0, dims = 0;
  if (!f || f.size() < 9) return false;
  if (f.read(&tag, 1) != 1 || f.read(reinterpret_cast<uint8_t*>(&frames), 4) != 4 ||
      f.read(reinterpret_cast<uint8_t*>(&dims), 4) != 4) return false;
  return validTemplateHeader(tag, frames, dims, f.size());
}
bool LocalWake::begin(const Config& config, bool recoverBackup) {
  config_ = &config;
  raw_ = static_cast<int16_t*>(ps_malloc(48000 * sizeof(int16_t)));
  if (!raw_ || !engine_.init(simplevox::MfccConfig{})) return false;
  const String backup = config.templatePath + ".bak";
  if (recoverBackup && validFile(backup)) {
    SPIFFS.remove(config.templatePath);
    SPIFFS.rename(backup, config.templatePath);
  }
  // A remaining backup means publication did not finish, even if primary has a valid header.
  const String active = validFile(backup) ? backup : config.templatePath;
  if (validFile(active)) reference_.reset(engine_.loadFile(("/spiffs" + active).c_str()));
  reset(); return true;
}
void LocalWake::reset() {
  used_ = 0;
  if (raw_) memset(raw_, 0, 48000 * sizeof(int16_t));
  gate_.reset(16000, 400, 200, 3000, 3000, config_->energyThreshold);
}
void LocalWake::resetDetection() {
  detection_.reset(); inputRms_=detectionThreshold_=0;
  detectionStatus_="Wake preparing"; resultFrames_=0;
}
bool LocalWake::detect(const int16_t* samples) {
  if(!raw_ || !reference_ || !samples) return false;
  if(!detection_) detection_.reset(new WakeEnrollmentCapture(raw_,48000));
  int64_t mean=0; for(size_t i=0;i<320;++i) mean+=samples[i]; mean/=320;
  uint64_t energy=0; for(size_t i=0;i<320;++i) { const int64_t v=samples[i]-mean; energy+=v*v; }
  inputRms_=static_cast<uint32_t>(std::sqrt(static_cast<double>(energy/320)));
  const auto result=detection_->push(samples,320);
  detectionThreshold_=detection_->voiceThreshold();
  if(resultFrames_) --resultFrames_;
  using R=WakeEnrollmentCapture::Result;
  if(!resultFrames_) detectionStatus_=result==R::Calibration ? "Quiet: measuring" :
      result==R::Recording ? "Hearing voice" : "Say your call";
  if(!detection_->terminal()) return false;
  bool matched=false;
  if(result==R::Ready) {
    std::unique_ptr<simplevox::MfccFeature> candidate(engine_.create(raw_,static_cast<int>(detection_->samples())));
    lastDistance_=candidate ? simplevox::calcDTW(*reference_,*candidate) : UINT32_MAX;
    matched=lastDistance_<config_->wakeThreshold;
    detectionStatus_=matched ? "Call matched" : "Call no match";
  } else {
    detectionStatus_=result==R::Noisy ? "Room too loud" : result==R::Short ? "Call too short" :
        result==R::Long ? "Voice too long" : result==R::Clipped ? "Sound clipped" : "Say your call";
  }
  resultFrames_=100; // Keep the most recent reason visible for about two seconds.
  const auto ambient=detection_->ambient();
  // Re-measure after idle/noisy conditions. Otherwise preserve the same ambient
  // gate and exact 100ms preroll/400ms end/trim rules used for enrollment.
  if(result==R::Empty || result==R::Noisy) detection_.reset();
  else detection_.reset(new WakeEnrollmentCapture(raw_,48000,ambient));
  memset(raw_,0,48000*sizeof(int16_t));
  return matched;
}
bool LocalWake::sameFile(const String& path, const simplevox::MfccFeature& feature) {
  if (!validFile(path)) return false;
  std::unique_ptr<simplevox::MfccFeature> loaded(engine_.loadFile(("/spiffs" + path).c_str()));
  if (!loaded || loaded->size()!=feature.size() || loaded->dimension()!=feature.dimension()) return false;
  for(int i=0;i<feature.size();++i)
    if(memcmp(loaded->feature(i),feature.feature(i),feature.dimension()*sizeof(int16_t))) return false;
  return true;
}
bool LocalWake::save(const simplevox::MfccFeature& feature) {
  const String path=config_->templatePath, temp=path+".tmp", backup=path+".bak";
  if (!engine_.saveFile(("/spiffs"+temp).c_str(),feature) || !sameFile(temp,feature)) {
    SPIFFS.remove(temp); return false;
  }
  File old=SPIFFS.open(path,"r"); const bool hadOld=old && !old.isDirectory(); old.close();
  const bool recoverable=validFile(backup);
  if(recoverable) {
    // Keep the last committed backup while removing an interrupted target.
    if(hadOld && !SPIFFS.remove(path)) return false;
  } else {
    File prior=SPIFFS.open(backup,"r"); const bool hadBackup=prior && !prior.isDirectory(); prior.close();
    if(hadBackup && !SPIFFS.remove(backup)) return false;
    if(hadOld && !SPIFFS.rename(path,backup)) { SPIFFS.remove(temp); return false; }
  }
  if(!SPIFFS.rename(temp,path)) {
    if(hadOld || recoverable) SPIFFS.rename(backup,path);
    SPIFFS.remove(temp); return false;
  }
  if(!sameFile(path,feature)) {
    SPIFFS.remove(path);
    if(hadOld || recoverable) SPIFFS.rename(backup,path);
    return false;
  }
  if((hadOld || recoverable) && !SPIFFS.remove(backup)) {
    // A successful commit must retire the rollback record. Otherwise retain the old call.
    SPIFFS.remove(path); SPIFFS.rename(backup,path); return false;
  }
  return true;
}
void LocalWake::cancelCandidate() {
  pending_.reset(); for(auto& repeat:repeats_) repeat.reset();
  candidateCount_=0; verified_=false;
}
std::unique_ptr<simplevox::MfccFeature> LocalWake::createCandidate(const int16_t* samples,size_t count) {
  if(!raw_ || !samples || count<6400 || count>48000) return nullptr;
  std::unique_ptr<simplevox::MfccFeature> feature(engine_.create(samples,static_cast<int>(count)));
  if(!feature || feature->size()<2 || feature->size()>186 || feature->dimension()!=12) return nullptr;
  return feature;
}
bool LocalWake::prepareCandidate(const int16_t* samples,size_t count) {
  cancelCandidate(); pending_=createCandidate(samples,count);
  candidateCount_=pending_ ? 1 : 0; return bool(pending_);
}
bool LocalWake::collectCandidate(const int16_t* samples,size_t count) {
  verified_=false;
  if(!pending_ || candidateCount_<1 || candidateCount_>=3) { cancelCandidate(); return false; }
  auto feature=createCandidate(samples,count);
  if(!feature) { cancelCandidate(); return false; }
  repeats_[candidateCount_-1]=std::move(feature); ++candidateCount_; return true;
}
bool LocalWake::validateCandidates() {
  verified_=false;
  if(candidateCount_!=3 || !pending_ || !repeats_[0] || !repeats_[1]) return false;
  const simplevox::MfccFeature* features[]={pending_.get(),repeats_[0].get(),repeats_[1].get()};
  uint64_t totals[3]{};
  for(unsigned i=0;i<3;++i) for(unsigned j=i+1;j<3;++j) {
    const uint32_t distance=simplevox::calcDTW(*features[i],*features[j]);
    if(distance>=config_->wakeThreshold) return false;
    totals[i]+=distance; totals[j]+=distance;
  }
  unsigned best=0;
  for(unsigned i=1;i<3;++i) if(totals[i]<totals[best]) best=i;
  if(best) pending_.swap(repeats_[best-1]);
  verified_=true; return true;
}
bool LocalWake::commitCandidate() {
  if(!pending_ || !verified_ || !save(*pending_)) return false;
  reference_=std::move(pending_); cancelCandidate(); reset(); return true;
}
bool LocalWake::push(const int16_t* data, bool registration, String& message) {
  if (!raw_) return false;
  const auto state = gate_.push(data, 320);
  // Keep 100 ms of pre-roll locally so consonants are not lost at VAD onset.
  if (state == SpeechGate::Result::Waiting) {
    if (used_ >= 1600) { memmove(raw_, raw_ + 320, (used_ - 320) * 2); used_ -= 320; }
  }
  if (used_ + 320 <= 48000) { memcpy(raw_ + used_, data, 640); used_ += 320; }
  if (state == SpeechGate::Result::Empty) { reset(); return false; }
  if (state != SpeechGate::Result::Complete) return false;
  // Exclude most trailing silence, consistently for registration and matching.
  const size_t count = used_ > 4800 ? used_ - 4800 : used_;
  std::unique_ptr<simplevox::MfccFeature> candidate(engine_.create(raw_, static_cast<int>(count)));
  bool result = false;
  if (candidate && candidate->size() >= 2) {
    if (registration) {
      if (save(*candidate)) { reference_ = std::move(candidate); message = "登録しました"; result = true; }
      else message = "登録保存に失敗";
    } else if (reference_) {
      const uint32_t distance = simplevox::calcDTW(*reference_, *candidate);
      Serial.printf("Wake distance: %lu\n", static_cast<unsigned long>(distance));
      result = distance < config_->wakeThreshold;
    }
  } else if (registration) message = "音声が短すぎます";
  reset(); return result;
}
