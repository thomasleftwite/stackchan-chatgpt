#include "Config.h"
#include <ArduinoJson.h>
#include <YAMLDuino.h>
#include <SPIFFS.h>

namespace {
bool token(const String& s) {
  if (s.isEmpty() || s.length() > 128) return false;
  for (size_t i = 0; i < s.length(); ++i)
    if (!isalnum(static_cast<unsigned char>(s[i])) && s[i] != '-' && s[i] != '_' && s[i] != '.') return false;
  return true;
}
bool number(JsonVariantConst v, uint32_t& dest, uint32_t lo, uint32_t hi) {
  if (!v.is<uint32_t>()) return false;
  const uint32_t n = v.as<uint32_t>();
  if (n < lo || n > hi) return false;
  dest = n; return true;
}
bool keyOk(const String& s) {
  if (s.length() > 512) return false;
  for (size_t i = 0; i < s.length(); ++i) if (s[i] < 33 || s[i] > 126) return false;
  return true;
}
}

bool Config::load(String& error) {
  File f = SPIFFS.open("/config.yaml", "r");
  if (!f || f.isDirectory() || f.size() > 8192) { error = "config.yaml missing/too large"; return false; }
  JsonDocument d;
  auto e = deserializeYml(d, f);
  f.close();
  if (e || d["schema_version"].as<int>() != 1) { error = "Invalid config.yaml/schema"; return false; }
  auto w = d["wake_word"].as<JsonObjectConst>();
  if (!w["enabled"].is<bool>() || String(w["method"] | "") != "local_template" ||
      !w["phrase"].is<const char*>() || !w["template_file"].is<const char*>()) {
    error = "Invalid wake_word settings"; return false;
  }
  wakeEnabled = w["enabled"].as<bool>();
  phrase = w["phrase"].as<String>(); templatePath = w["template_file"].as<String>();
  if (phrase.isEmpty() || phrase.length() > 96 || !templatePath.startsWith("/wakeword") ||
      !templatePath.endsWith(".bin") || templatePath.length() > 24 || templatePath.indexOf("..") >= 0 ||
      templatePath.substring(1).indexOf('/') >= 0) {
    error = "Invalid phrase/template path"; return false;
  }
  auto a = d["audio"].as<JsonObjectConst>();
  uint32_t vol = volume;
  if (!number(w["distance_threshold"], wakeThreshold, 1, 1000) ||
      !number(a["energy_threshold"], energyThreshold, 50, 10000) ||
      !number(a["silence_ms"], silenceMs, 300, 3000) ||
      !number(a["idle_timeout_ms"], idleMs, 5000, 60000) ||
      !number(a["max_speech_ms"], maxSpeechMs, 3000, 30000) ||
      !number(a["volume"], vol, 0, 255)) { error = "Invalid audio limits"; return false; }
  volume = vol;
  transcriptionModel = d["openai"]["transcription_model"] | "gpt-live-transcribe";
  realtimeModel = d["openai"]["realtime_model"] | "gpt-realtime";
  voice = d["openai"]["voice"] | "marin";
  geminiModel = d["gemini"]["model"] | "gemini-2.5-flash";
  systemPrompt = d["gemini"]["system_prompt"] | systemPrompt.c_str();
  if (!token(transcriptionModel) || !token(realtimeModel) || !token(voice) ||
      !token(geminiModel) || systemPrompt.length() > 2048) { error = "Invalid API settings"; return false; }
  File s = SPIFFS.open("/secrets.json", "r");
  // SPIFFS can return a directory handle for an absent path on this core.
  // A real file with invalid JSON must still fail closed below.
  if (!s || s.isDirectory()) return true; // Local operation without API keys.
  if (s.size() > 2048 || deserializeJson(d, s)) { error = "Invalid secrets.json"; return false; }
  openaiKey = d["openai_api_key"] | "";
  geminiKey = d["gemini_api_key"] | "";
  if (!keyOk(openaiKey) || !keyOk(geminiKey)) { error = "Invalid API key format"; return false; }
  return true;
}
