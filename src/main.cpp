#include <Arduino.h>
#include <M5Unified.h>
#include <SPIFFS.h>
#include <WiFiManager.h>
#include <time.h>
#include "AudioDevice.h"
#include "CloudConversation.h"
#include "Config.h"
#include "LocalWake.h"

namespace {
Config config;
AudioDevice audio;
LocalWake wake;
CloudConversation cloud;
bool configurationOk = false, wakeOk = false, cloudOk = false;
bool registering = false, holdConsumed = false;
uint32_t registrationStart = 0, reconnectAt = 0;
String lastStatus;

void show(const String& text, bool logStatus = true) {
  if (text == lastStatus) return;
  lastStatus = text;
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(2, 2);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.print(text);
  // Status only: transcripts, secrets and answer text are never logged.
  if (logStatus) Serial.println(text);
}
void showIdle() {
  String text = "待機中\n押す: 会話\n長押し: 登録\n";
  if (!config.hasKeys()) text += "APIキー未設定\n";
  if (WiFi.status() != WL_CONNECTED) text += "Wi-Fi未接続\n";
  if (!wakeOk) text += "音声検出停止";
  else if (!wake.registered()) text += "呼びかけ未登録";
  else text += config.phrase;
  show(text);
}
void enterIdle() {
  registering = false;
  audio.stop();
  if (wakeOk) wake.reset();
  if (wakeOk && config.wakeEnabled && wake.registered()) {
    if (!audio.listen(16000)) { wakeOk = false; show("マイク初期化失敗"); delay(1500); }
  }
  showIdle();
}
void connectWifi(bool forcePortal) {
  audio.stop();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFiManager manager;
  manager.setDebugOutput(false);
  manager.setConnectTimeout(15);
  manager.setConfigPortalTimeout(0);
  manager.setBreakAfterConfig(false);
  const char password[] = "12345678";
  manager.setAPCallback([&](WiFiManager*) {
    show("Wi-Fi設定\nStackchan-Setup\nPW: " + String(password) + "\n192.168.4.1", false);
  });
  show("Wi-Fi接続中");
  if (forcePortal) {
    // Keep saved credentials until the user supplies replacements.
    show("Wi-Fi設定\nStackchan-Setup\nPW: " + String(password) + "\n192.168.4.1", false);
    manager.startConfigPortal("Stackchan-Setup", password);
  } else manager.autoConnect("Stackchan-Setup", password);
  manager.stopConfigPortal();
  WiFi.mode(WIFI_STA);
  configTime(0, 0, "pool.ntp.org", "time.google.com");
}
bool clockReady() {
  const uint32_t start = millis();
  show("時刻を確認中");
  while (time(nullptr) < 1735689600 && millis() - start < 15000) {
    M5.update(); delay(50);
  }
  return time(nullptr) >= 1735689600;
}
void conversation() {
  audio.stop();
  if (wakeOk) wake.reset(); // Discard every pre-trigger sample before cloud capture.
  if (!cloudOk || !config.hasKeys() || WiFi.status() != WL_CONNECTED) {
    show("接続・API設定を\n確認してください"); delay(2000); enterIdle(); return;
  }
  if (!clockReady()) { show("時刻同期に失敗"); delay(2000); enterIdle(); return; }
  cloud.clearHistory();
  const uint32_t start = millis();
  // One conversation has a bounded lifetime even if the room never becomes quiet.
  while (millis() - start < 10UL * 60 * 1000) {
    String text, answer, error;
    if (!cloud.transcribe(audio, text, error, [](const String& s) { show(s); })) {
      show("聞き取り失敗\n" + error); delay(2500); break;
    }
    if (text.isEmpty()) break;
    show("考え中");
    if (!cloud.answer(text, answer, error)) {
      show("回答取得失敗\n" + error); delay(2500); break;
    }
    if (!cloud.speak(audio, answer, error, [](const String& s) { show(s); })) {
      show("読み上げ失敗\n" + error); delay(2500); break;
    }
  }
  cloud.clearHistory();
  // Consume button events accumulated during network operations.
  M5.update(); holdConsumed = true;
  enterIdle();
}
void startRegistration() {
  if (!wakeOk) { show("音声検出初期化失敗"); return; }
  audio.stop(); wake.reset();
  show("登録します\n離してから\n" + config.phrase);
  while (M5.BtnA.isPressed()) { M5.update(); delay(10); }
  if (!audio.cue() || !audio.listen(16000)) { show("マイク初期化失敗"); return; }
  registering = true; registrationStart = millis();
  show("呼びかけを\n話してください\n" + config.phrase);
}
}

void setup() {
  auto m5 = M5.config();
  m5.internal_spk = false; m5.internal_mic = false;
  m5.external_speaker.atomic_echo = true;
  m5.serial_baudrate = 115200;
  M5.begin(m5);
  M5.Display.setFont(&fonts::efontJA_12);
  M5.Display.setTextWrap(true);
  M5.update();
  const bool forcePortal = M5.BtnA.isPressed();
  if (!psramFound()) { show("PSRAMが必要です"); return; }
  if (!SPIFFS.begin(false)) { show("設定領域なし\nUpload Filesystem\nImageを実行"); return; }
  String error;
  if (!config.load(error)) { show("設定エラー\n" + error); return; }
  configurationOk = true;
  M5.Speaker.setVolume(config.volume);
  wakeOk = wake.begin(config);
  cloudOk = cloud.begin(config, error);
  if (!cloudOk) { show(error); delay(2000); }
  connectWifi(forcePortal);
  holdConsumed = true;
  enterIdle();
}

void loop() {
  M5.update();
  if (!configurationOk) { delay(50); return; }
  if (M5.BtnA.isReleased() && !M5.BtnA.wasReleased()) holdConsumed = false;
  if (!holdConsumed && M5.BtnA.pressedFor(2000)) {
    holdConsumed = true;
    if (!registering) startRegistration();
  }
  if (M5.BtnA.wasReleased() && !holdConsumed) {
    if (registering) enterIdle();
    else conversation();
    return;
  }
  if (WiFi.status() != WL_CONNECTED && millis() - reconnectAt >= 10000) {
    reconnectAt = millis(); WiFi.reconnect();
    if (!registering) showIdle();
  }
  if (registering && millis() - registrationStart > 10000) {
    show("登録タイムアウト"); delay(1500); enterIdle(); return;
  }
  if (wakeOk && (registering || (config.wakeEnabled && wake.registered()))) {
    int16_t frame[320];
    if (!audio.read(frame, 320)) { show("マイク読み取り失敗"); delay(1000); enterIdle(); return; }
    String message;
    const bool matched = wake.push(frame, registering, message);
    memset(frame, 0, sizeof(frame));
    if (registering && (!message.isEmpty() || matched)) {
      show(message); delay(1500); enterIdle();
    } else if (matched) conversation();
  } else delay(10);
}
