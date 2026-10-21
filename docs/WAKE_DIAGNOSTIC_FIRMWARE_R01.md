# ウェイク診断・録音整合版 R01

2026-10-21。目的: 第一段階のUSB制御・監視なし/Wi-Fi運用で、呼びかけの見逃しと会議話者の誤検出を切り分ける。対象: AtomS3R + Atomic Voice Base A149、Windowsホスト0.5.0-preview.1。実機で識別精度を確認した完成版ではなく、録音条件の整合と可視化を行った検証版。

## 変更と依存

- 待受も登録と同じWakeEnrollmentCaptureを使用。400msの過渡入力除去＋600msの環境測定、基準max(750,環境RMS90パーセンタイル×3)、100msの前置き、400msの無音終端、末尾300msの除外、最小400msの特徴窓、200ms以上の有声、最大3秒、クリップ拒否を共通化。無発話/過大環境音の後は再測定。
- マイク完了callbackで後続320サンプルを予約。32フレーム/20,480 bytesのリングで描画・特徴計算の間も入力取得。満杯時は未読音声を上書きせず停止・再試行。1秒以上完了が来ない場合も停止する。
- 開始/録音失敗で登録済み状態を無効にせず2秒後に再試行。Sleep/ボタン/質問/回答中は停止。Mic.endでproducer終了後にcallbackとリングを消去。
- Readyで理由とV/T/Dを表示。V=現在の20ms入力のDC除去RMS、T=発話基準、D=最後の照合距離。D--は未取得/計算不能、999超はD>999。距離が小さいほど似ており、既存設定値未満で一致する。既定250は緩めていない。直近の非一致等の理由を約2秒残す。
- Wi-Fi/NVS/登録/ホストは変更せず、質問/登録の準備→ビープ→700ms受付を維持。

Espressif32 6.11.0 / Arduino-ESP32 2.0.17、M5Unified 0.2.26、M5GFX 0.2.33、SimpleVox既存ソース。その他はplatformio.ini。依存追加なし。

## 検証

`C:\Python314\python.exe tools/run_host_tests.py --suite <suite>`で9スイート成功: rules27、gemini-ui6、face-ui29、wake-storage29、wake-enrollment13、wake-enrollment-ui28、wifi-ui4、wireless-link11、wake-detection16、合計163項目。

新規wake-detectionは実際のGeminiWake/AudioDevice/LocalWake/切出しコードを使用。FS/マイク/MFCC/DTWは代替。開始・初回要求・後続要求の失敗と復帰、連続予約、リング満杯、入力停止、過大雑音、RMS600でT1800の待受、距離表示、一致時停止、保存物不変を確認。実機識別精度の保証とはしない。

`platformio.exe run -e atoms3r-gemini-device -e atoms3r -e atoms3r-face-demo`の3構成成功。無線版の静的RAM78,724 bytes、ELF flash1,601,969 bytes。初回は制限環境のplatforms.lock権限で失敗し、限定した権限要求後に成功。MSVCで発見した引数名の警告は修正しUI系を再検証。

実描画コードで生成した`.pio/host-tests/wake-diagnostic-preview.svg`をPNGへ変換し表示確認。Ready/理由/数値/操作案内/PC onlineの配置を確認。実LCDの読みやすさは未確認。

## アプリ限定導入

利用者の「ok,フラッシュして」と「USB接続済み」に基づきCOM4のESP32-S3-PICO-1 rev0.2へ導入。USBは書込みとその前提となるパーティション表の読取りだけに使用。通常のUSB制御・監視は開いていない。

- 候補: `.pio/firmware-snapshots/gemini-wake-diagnostic-20261021-r01.bin`、1,602,336 bytes。
- SHA256: `B432DB868E55F11B6CD8784D310D0C674CCFE457D4DD8D2A8DD710DC54DDE91A`。
- 復旧元: `.pio/firmware-snapshots/gemini-hotspot-trust-20261021-r01.bin`。既存SHA256 `BF936E8863AE7A11587EE1ABD0E6FAC94B64B9D2F196715AE097E6B97CCE3B98`の不変を確認。
- 実機表: `.pio/device-check/partitions-wake-diagnostic-20261021-r01.bin`。app0=0x10000/容量0x300000、NVS=0x9000、SPIFFS=0x610000。
- `tools/flash_application_only.py`へ候補・全SHA・実機表を明示。消去0x10000～0x197FFF、1,602,336 bytesを書込み、Hash of data verified、終了コード0、RTSリセット完了。NVS/SPIFFS/ブートローダー/表は書換え範囲外。
- 書込み後のシリアル監視・GEM INFO・実API呼出しなし。起動/接続/登録の利用可否は画面と実会話で確認待ち。

## USBなしでの確認方法

1. PCのUSBから外して通常の給電へ戻す。PCホスト/ホットスポットは起動継続。
2. PC Online / Sleepingを確認し、画面短押しで起こす。
3. Quiet: measuring中は呼びかけず、Say your callになったら登録済みの呼びかけを1回話し、その後400ms以上黙る。
4. ビープが鳴ったら、終了から約0.7秒後に質問。回答音声/口パク/Ready復帰を確認。
5. 反応しなければ理由とV/T/Dを確認。会議音声だけでListeningへ移るかも別に確認。会議本文・話者情報・音声は保存しない。

Room too loud=環境測定が過大、Hearing voice=音声区間取得中、Voice too long=3秒上限、Call no match=照合不一致、Mic … retry/Wake buffer retry=取得失敗の再試行。Dは最後の距離であり、現在のVと同時刻の音声とは限らない。

利用者による導入後確認: 呼びかけ試験後に「応答あり」と回答。ただし回答音声の冒頭が約0.2秒欠けるとの報告。口パク・Ready復帰・反復検出率について今回の回答からは追加確認しない。

冒頭欠けの静的調査: Node側は受信音声を連結して先頭offset0から送信し、端末は回答の受信完了後にSpeaker.begin直後からPCMを再生する。再生開始前の無音区間は明示的に付けていない。M5UnifiedのAtomic Voice Base用callbackではES8311のリセット/アナログ回路/DAC出力と外部出力を有効化する。再生回路の立上り中に先頭が聞こえなくなる可能性はあるが、クラウド音声自体の欠けとの切分けは未実施。無音PCMでの先行駆動が修正候補であり、現時点で追加修正・書込みは行っていない。

声の設定: ホストのモデル欄はAIモデルIDで、既定gemini-3.8-live。現行server/gemini-turn.tsはspeechConfigを指定しておらず、声はAPI既定。声の選択UIは未実装。Google Live APIはspeechConfig.voiceConfig.prebuiltVoiceConfig.voiceNameによる選択を提供する（例Kore）。一次情報: https://ai.google.dev/gemini-api/docs/live-api/capabilities#change-voice-and-language 。声の指定はホスト側で完結するため端末通信の変更は不要。

未確認: 会議音声下の検出率/誤検出率、リングの実機余裕、実故障からの復帰、長期運用、冒頭欠けの原因と修正。背景話者を除外する専用音源分離や学習済みキーワードモデルは未追加で、単一テンプレート照合の限界は残る。
