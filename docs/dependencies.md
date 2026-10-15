# 依存関係

PlatformIOの依存バージョンはplatformio.iniに固定する。Arduino-ESP32は内部にESP-IDFを含むが、開発フレームワークはArduinoのみ。

2026-10-15のビルド環境: espressif32 6.11.0、Arduino-ESP32 2.0.17（framework-arduinoespressif32 3.20017.241212+sha.dcc1105b）、Xtensa GCC 8.4.0。M5Unified 0.2.26、M5GFX 0.2.33、WiFiManager 2.0.17、WebSockets 2.7.3、ArduinoJson 7.4.2、YAMLDuino 1.4.2+sha.bc40a8e。

- M5Unified / M5GFX: https://github.com/m5stack/M5Unified / https://github.com/m5stack/M5GFX
- WiFiManager: https://github.com/tzapu/WiFiManager
- arduinoWebSockets: https://github.com/Links2004/arduinoWebSockets
- ArduinoJson: https://arduinojson.org/
- YAMLDuino: https://github.com/tobozo/YAMLDuino
- SimpleVox: https://github.com/MechaUma/SimpleVox commit `f2c589f6193d9743548eb76e98ee31c934e20d86`。lib/SimpleVoxへソースとライセンスを同梱。
- ca.pem: Python certifiに同梱されたMozilla CA集合から生成。https://github.com/certifi/python-certifi （MPL-2.0）。公開CAであり秘密鍵は含まない。

参考にした既存AI_StackChan_Exのライセンスは `AI_StackChan_Ex-LICENSE` に保持する。設定・音声切替・SimpleVoxの利用方法を参照した。

API仕様:
- https://developers.openai.com/api/docs/guides/realtime-transcription
- https://developers.openai.com/api/docs/guides/realtime-conversations
- https://ai.google.dev/api/generate-content

## この案件での適合修正

- WebSockets 2.7.3はESP32向けに `WEBSOCKETS_MAX_DATA_SIZE` を15 KiBへ無条件で再定義する。`tools/prepare_websockets.py` が生成済み依存ヘッダーの当該箇所だけに `#ifndef` ガードを追加し、platformio.iniの128 KiBを有効にする。アプリ側にもstatic_assertを置く。依存ライブラリ更新時はパッチを再確認する。元のライセンス表示は保持する。
- PlatformIOの依存探索は `deep` を使用する。`deep+` ではWebSockets自身の条件付きincludeからWiFi.hを解決できず、実際にコンパイルが失敗した。
- SimpleVoxはMFCC/DTWを使用し、VADはアプリのSpeechGateを使用する。同梱library.jsonで未使用のsimplevox_vad.cppをビルドから除外する。MFCC用のesp-dspは今回のArduino-ESP32に同梱されたものを使用する。
- ArduinoJson 7.4.2ではMemberProxyをauto変数へ直接コピーできないため、オブジェクト読出しにas、生成にtoを明示する。[公式説明](https://arduinojson.org/v7/proxy-non-copyable/)
- 同梱CAは既存の公開CA集合（121証明書、240,216 bytes）を保持した。PC上でのPEM読込みと秘密鍵マーカーなしを確認。tools/update_ca.pyによる小さいCA集合への再生成は今回実施していない。実機のTLSメモリ消費と接続可否は別途検証する。
