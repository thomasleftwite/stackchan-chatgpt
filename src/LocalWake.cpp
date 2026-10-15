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
bool LocalWake::begin(const Config& config) {
  config_ = &config;
  raw_ = static_cast<int16_t*>(ps_malloc(48000 * sizeof(int16_t)));
  if (!raw_ || !engine_.init(simplevox::MfccConfig{})) return false;
  const String backup = config.templatePath + ".bak";
  if (!validFile(config.templatePath) && validFile(backup)) {
    SPIFFS.remove(config.templatePath);
    SPIFFS.rename(backup, config.templatePath);
  }
  if (validFile(config.templatePath))
    reference_.reset(engine_.loadFile(("/spiffs" + config.templatePath).c_str()));
  reset(); return true;
}
void LocalWake::reset() {
  used_ = 0;
  if (raw_) memset(raw_, 0, 48000 * sizeof(int16_t));
  gate_.reset(16000, 400, 200, 3000, 3000, config_->energyThreshold);
}
bool LocalWake::save(const simplevox::MfccFeature& feature) {
  const String path = config_->templatePath, temp = path + ".tmp", backup = path + ".bak";
  if (!engine_.saveFile(("/spiffs" + temp).c_str(), feature) || !validFile(temp)) {
    SPIFFS.remove(temp); return false;
  }
  SPIFFS.remove(backup);
  File old = SPIFFS.open(path, "r");
  const bool hadOld = old && !old.isDirectory();
  old.close();
  if (hadOld && !SPIFFS.rename(path, backup)) { SPIFFS.remove(temp); return false; }
  if (!SPIFFS.rename(temp, path)) {
    if (hadOld) SPIFFS.rename(backup, path);
    return false;
  }
  SPIFFS.remove(backup); return true;
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
  std::unique_ptr<simplevox::MfccFeature> candidate(engine_.create(raw_, count));
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
