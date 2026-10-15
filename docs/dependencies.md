# 依存関係

PlatformIOの依存バージョンはplatformio.iniに固定する。Arduino-ESP32は内部にESP-IDFを含むが、開発フレームワークはArduinoのみ。

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
