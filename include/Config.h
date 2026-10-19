#pragma once
#include <Arduino.h>

struct Config {
  String phrase = "スタックちゃん", templatePath = "/wakeword0.bin";
  bool wakeEnabled = true;
  uint32_t wakeThreshold = 250, energyThreshold = 500, silenceMs = 800;
  uint32_t idleMs = 15000, maxSpeechMs = 15000;
  uint8_t volume = 100;
  String transcriptionModel = "gpt-live-transcribe";
  String realtimeModel = "gpt-realtime", voice = "marin";
  String geminiModel = "gemini-2.5-flash";
  String systemPrompt = "あなたはスタックちゃんです。日本語で親しみやすく、短く2文程度で回答してください。";
  String openaiKey, geminiKey;
  bool load(String& error, bool includeSecrets = true);
  bool hasKeys() const { return !openaiKey.isEmpty() && !geminiKey.isEmpty(); }
};
