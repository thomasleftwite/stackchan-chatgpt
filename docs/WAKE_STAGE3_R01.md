# 第3段階R01: 1パターンの安全な呼びかけ再登録

版: R01 / 2026-10-19（日本時間）。対象: M5AtomS3R + Atomic Voice Base A149、Arduino-ESP32 2.0.17 / PlatformIO、atoms3r-gemini-device。目的: 通常の質問操作と分けた登録画面で、既存登録を守りながら1パターンを再登録する。複数サンプル登録は後段。現時点は実装/オフライン検証、実機未導入。

## 操作と仕様

1. Sleeping中に画面ボタンを5秒長押し。2秒時点でHold 5s: learn call、5秒でRelease: learn callとなるので離す。2〜5秒未満で離した場合は従来どおりウェイクだけ行う。
2. Click: record callで短押し。開始音→約1秒静かに待つ→Say call表示後に登録したい呼びかけを話す。
3. 品質確認が通るとClick: try call。短押しし、開始音/準備を待ち、Repeat same call表示後に同じ呼びかけをもう一度話す。
4. 一致するとClick: save / hold: no。短押ししたときだけ保存する。成功表示後はSleepingへ戻る。

選択画面では2秒長押し、録音中は押下で取消。選択の無操作30秒、長押し後の離し待ち10秒、各録音10秒まで。取消/不一致/無音/品質不足/保存失敗では旧登録を保持。PC/Wi-Fi接続なしでも登録でき、呼びかけ登録音声や特徴をクラウドへ送らない。画面は小型表示のため短い英語案内を使う。

## 録音品質と照合

新しいWakeEnrollmentCaptureが16kHz/320サンプル単位のローカル音声を扱う。先頭1秒は既存AmbientGateで準備/環境音を測定。発話は最大48,000サンプルの固定領域内で、最低400msの有声部分を要求する。待ち無音約5秒、末尾無音400ms、100msの前後余白で呼びかけ部分を残す。長すぎる音声・無音・短音・大きすぎる環境音・過大なクリップを拒否する。閾値は初期値であり実機品質の保証ではない。

候補特徴はLocalWake.pending_としてRAMに保持。別発話の特徴が設定wakeThreshold（既存値）未満で一致し、利用者が保存を選択するまで既存reference_と登録ファイルを置換しない。照合はSimpleVox MFCC/DTWで話者認証ではない。

## 保存と復旧

保存先は既存config.templatePath（通常/wakeword0.bin）。新特徴を.tmpへ書き、ヘッダ検証に加えて読み戻した全特徴量を候補と照合する。旧primaryを.bakへ退避し、新primaryの改名後も読み戻し照合する。最後に.bakを削除できて初めて保存成功とする。失敗した場合は旧primaryへ戻す。ロールバックの改名自体が失敗しても.bakを残す。有効な.bakの残存は保存未完了を意味し、Stage3の起動時はprimaryの有効性に関係なく優先して読取りだけで使用する。ヘッダ正常・内容不一致のprimaryを削除できない場合でも旧登録を選択できる。旧版で正常保存後にbackup削除だけ失敗した残存ファイルも、安全側として旧登録が優先される。次の検証済み登録で未完了状態から復旧できる。SPIFFS自動フォーマットは行わない。

この処理はホスト上で故障を注入した論理検証済み。実SPIFFSの電源断耐性・Flash故障・実際の瞬断試験は未実施で、完全なアトミック保存を保証しない。既存通常版のLocalWake.saveにも読戻し照合と保全を適用する。

## 検証

- wake-storage: 実LocalWake.cppを使用し、FSとMFCCだけ置換。21/21成功。検証前保存拒否/不一致/取消/初回登録/書込み失敗/一時・本保存読戻し不一致/改名失敗/ロールバック失敗/バックアップ読込を確認。内容不一致とprimary削除失敗の同時発生後、再起動で旧特徴を使った検出と再保存も確認。実DSPの精度証明ではない。
- wake-enrollment: 9/9成功。無音/短音/雑音/過大入力/長時間/DC/無効フレームと正常録音窓を確認。
- wake-enrollment-ui: 実LocalGemini/FaceUI/AudioDeviceソースを機器置換で実行し11/11成功。5秒入口から録音/検証/保存、各画面取消、I/O/照合/保存失敗、無操作期限、USB probeの通信入口拒否、PC停止時のローカル状態保持を確認。
- 既存回帰: 共通ルール27、録音UI6、FaceUI27、PC監視17、サーバ26がすべて成功。機器/APIを置換したオフライン試験。
- 仕様・品質の2軸で独立レビューを実施。仕様レビューで保存読戻し不一致とprimary削除失敗の組合せを指摘し、backup優先読込と故障試験を追加。再レビューで指摘解消を確認。実機精度と電源断耐性の証明は含まない。

再現: PlatformIO用Pythonでtools/run_host_tests.pyのrules、gemini-ui、face-ui、wake-storage、wake-enrollment、wake-enrollment-uiを各実行。PC監視はpython -m unittest discover -s tests -p test_gemini_coordinator.py -q、サーバはserver内でnpm test。ビルドはpio run -e atoms3r -e atoms3r-gemini-device -e atoms3r-face-demo。依存版はplatformio.iniで固定（espressif32 6.11.0、Arduino-ESP32 2.0.17、M5Unified 0.2.26、M5GFX 0.2.33、SimpleVoxローカルライブラリ）。

登録中はUSB INFO/PING/STOPを処理し、CONFIG/PROBEはGEM LOCAL BUSYで拒否。SPIFFS/NVS全体の書込みやWi-Fi再設定、ファームウェア自動書込みは追加しない。PC監視追加を行わない合意、USB接触不良の原因究明保留も維持する。

## 導入と未確認範囲

実機はStage2 R01のまま。Stage3はアプリ領域0x10000限定の候補を準備し、設定/既存登録を保持して導入する。登録置換は利用者の登録画面の操作と最終短押しでだけ行う。SPIFFSイメージ書込みは使わない。Stage2復旧イメージを保持する。

最終ビルドは通常版atoms3r、端末版atoms3r-gemini-device、表示デモatoms3r-face-demoの3/3成功。候補は.pio/firmware-snapshots/gemini-face-stage3-r01.bin（1,469,328 bytes）、SHA256: 6D0746268D50C55394151730D39EE589C9337F0ADCD2C899E86382DAA14F34A4。端末版のRAM使用量57,624 bytes。直前のStage2復旧イメージ.pio/firmware-snapshots/gemini-face-stage2-r01.binは保持し、SHA256 C43024F6272FF7D3FB12180517E8B7653A13577F3AEA10E08469717746F3ECE1の一致を再確認した。

導入手順は接続COMを再確認→監視/PC起動ツール停止→esptool --chip esp32s3 --port COM4 --baud 460800 write_flash 0x10000 .pio/firmware-snapshots/gemini-face-stage3-r01.bin→ハッシュ検証→必要時は手動リセット。復旧時は同じアプリ領域へStage2イメージだけを書き戻す。登録保存後に旧ファームウェアへ戻しても登録内容は自動で旧呼びかけへ戻らない。erase_flash/uploadfsは使用しない。今回の実機書込み・API呼出しは0回。

実機導入後はまず登録画面を開いて取消し、元の呼びかけが使えることを確認。次に新登録/別発話の検証/保存を行い、リセット後の保持と質問回答への回帰を確認する。実機の音量/録音品質/誤検出/実保存/電源断耐性は未検証。配布ホストEXEはprotocol v2のまま使えるが、0.3.0-preview.1の状態ログ一覧はStage2までなので、Stage3固有の操作は端末画面で行う。今回EXEは再生成しない。
