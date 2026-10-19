# 登録済み呼びかけの統合

版: R01 / 2026-10-19（日本時間）。対象: M5AtomS3R + Atomic Voice Base A149、Arduino-ESP32 2.0.17 / PlatformIO。目的: 顔表示付きPC中継版で、既存の登録済み1パターンから質問を開始する。実装済み・実機未導入。

## 動作

起動はSleepingでマイク停止。短押しでWakeWaitへ移行し、PC/Wi-Fi正常かつ登録ありなら16kHz/320サンプルずつ端末内で照合する。表示はSay call / hold to ask。登録なし・無効・マイク初期化失敗ではHold: manual askの手動経路を残す。長押し/離す操作は従来通り。呼びかけが一致すると検出マイクと前の音声を破棄し、開始音→約1秒の環境音測定→Speak now→質問録音へ進む。呼びかけと質問は続けて話さず案内を待つ。

送信/回答待ち/受信/再生/再生後700msの間は呼びかけ検出を停止。成功・無発話・取消後にWakeWaitへ戻り、次の呼びかけを待つ。残響待ち700msは暫定で実機未評価。WakeWaitの3分自動スリープ、PC期限切れ、Wi-Fi切断、ボタン操作時も検出マイクを停止する。登録処理や会話履歴は追加しない。

## データと依存

include/GeminiWake.hが既存LocalWake/SimpleVoxを使用。SPIFFS.begin(false)で自動フォーマットせず、Config.load(error, false)で秘密情報ファイルを読まない。LocalWake.begin(config, false)でバックアップ復旧書込みを抑止する。登録・ファイルシステム更新なし。既存通常版は既定引数で従来動作を維持。依存追加なし、固定版はplatformio.ini。実機用はatoms3r-gemini-device。

## 再現・検証

- tools/run_host_tests.py --suite face-ui: 23/23成功。追加8項目で待受限定マイク、Sleep、PC/Wi-Fi切断と復帰、呼びかけ後の合図と質問経路、非待受モード、ポーリングで延長しない自動スリープを確認。
- tools/run_host_tests.py --suite gemini-ui: 既存6項目成功。
- tools/run_host_tests.py: 27/27成功。
- serverでnpm test: 26/26成功。実API呼出しなし。
- 規約・仕様の2軸で別エージェントによる読取りレビュー実施。修正必須の指摘0。検証不足: UI試験はGeminiWakeをスタブ化するため、実SPIFFS読込・実マイク320サンプル処理・MFCC/DTWは実機で要確認。

## 実機試験の手順（未実施）

1. 合格済みStage1イメージを保持し、第2段階候補のハッシュとアプリ領域のみの変更を確認する。
2. tools/flash_application_only.pyでCOM4の0x10000のみ書込み。既存の確認済みパーティション表を指定し、SPIFFS/NVS/設定/登録は保持。書込み後ハッシュ検証。
3. GEM INFOでstage=2、起動Sleeping/mic=0を確認。GEM WAKE available=1なら既存登録の読込あり。0なら再登録せず原因を調査。
4. Start-Gemini-Device.cmdを起動、短押し、登録済み呼びかけ、合図とSpeak now後に質問。回答、口パク、Ready復帰、2回目の呼びかけを確認。
5. 再生中に呼びかけても録音しないこと、Sleep/切断時mic=0、中止、自動スリープ、再生後の誤反応を確認。音声・本文・キーを保存せず数値/状態だけ記録する。

今回のコード変更を実機成功・精度保証とは扱わない。実機書込みと統合試験は次工程。クラウド公開・課金変更なし。

## ビルド済み候補

3構成（atoms3r / atoms3r-gemini-device / atoms3r-face-demo）の最終ビルド成功。候補: `C:\Users\thoma\SynologyDrive\H\Doc\Src\Codex\projects\ai-stackchan\AI_StackChan_Ex\Stackchan-chatgpt\.pio\firmware-snapshots\gemini-face-stage2-r01.bin`、1,465,824 bytes、SHA256 `C43024F6272FF7D3FB12180517E8B7653A13577F3AEA10E08469717746F3ECE1`。Stage1復旧用イメージのハッシュ一致を確認。実機への導入は未実施。
