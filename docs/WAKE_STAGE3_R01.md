# 第3段階R01: 1パターンの安全な呼びかけ再登録

最新の診断は[第3段階R03](WAKE_STAGE3_R03.md)。R01の登録準備NOISYを受けてR02でビープ後700ms待ちを追加し、R03でも維持する。最後の判定値を再照会して発生源を切り分け中。本書は基本設計・R01の実機確認履歴。

版: R01 / 2026-10-19（日本時間）。対象: M5AtomS3R + Atomic Voice Base A149、Arduino-ESP32 2.0.17 / PlatformIO、atoms3r-gemini-device。目的: 通常の質問操作と分けた登録画面で、既存登録を守りながら1パターンを再登録する。複数サンプル登録は後段。実装/オフライン検証済み。アプリ限定導入・通常起動・既存登録読込・Sleeping/mic停止を確認。実登録の操作は確認中。

## 操作と仕様

1. Sleeping中に画面ボタンを5秒以上長押し（実機案内は約7秒を目安）。2秒時点でHold 5s: learn call、5秒でRelease: learn callとなるので離す。2〜5秒未満で離した場合は従来どおりウェイクだけ行う。Readyからの長押しは手動質問用なので、登録する場合は先に短押しでSleepingへ戻す。
2. 登録画面の上段タイトルはSettings。画面下部の補助文がClick: record callになったら短押し。開始音→約1秒静かに待つ→Say call表示後に登録したい呼びかけを話す。上段タイトルと下部の補助文を区別して案内する。
3. 品質確認が通るとClick: try call。短押しし、開始音/準備を待ち、Repeat same call表示後に同じ呼びかけをもう一度話す。
4. 一致するとClick: save / hold: no。短押ししたときだけ保存する。成功表示後はSleepingへ戻る。

選択画面では2秒以上長押し（実機案内は約3秒を目安）、録音中は押下で取消。選択の無操作30秒、長押し後の離し待ち10秒、各録音10秒まで。取消/不一致/無音/品質不足/保存失敗では旧登録を保持。PC/Wi-Fi接続なしでも登録でき、呼びかけ登録音声や特徴をクラウドへ送らない。画面は小型表示のため短い英語案内を使う。

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

Stage3はアプリ領域0x10000限定で導入する。設定/既存登録の領域を保持し、登録置換は利用者の登録画面の操作と最終短押しでだけ行う。SPIFFSイメージ書込みは使わない。Stage2復旧イメージを保持する。実機導入の最新結果は下記。

最終ビルドは通常版atoms3r、端末版atoms3r-gemini-device、表示デモatoms3r-face-demoの3/3成功。候補は.pio/firmware-snapshots/gemini-face-stage3-r01.bin（1,469,328 bytes）、SHA256: 6D0746268D50C55394151730D39EE589C9337F0ADCD2C899E86382DAA14F34A4。端末版のRAM使用量57,624 bytes。直前のStage2復旧イメージ.pio/firmware-snapshots/gemini-face-stage2-r01.binは保持し、SHA256 C43024F6272FF7D3FB12180517E8B7653A13577F3AEA10E08469717746F3ECE1の一致を再確認した。

導入手順は接続COMを再確認→監視/PC起動ツール停止→esptool --chip esp32s3 --port COM4 --baud 460800 write_flash 0x10000 .pio/firmware-snapshots/gemini-face-stage3-r01.bin→ハッシュ検証→必要時は手動リセット。復旧時は同じアプリ領域へStage2イメージだけを書き戻す。登録保存後に旧ファームウェアへ戻しても登録内容は自動で旧呼びかけへ戻らない。erase_flash/uploadfsは使用しない。候補準備時点では実機書込み・API呼出し0回。導入結果は下記。

実機導入後はまず登録画面を開いて取消し、元の呼びかけが使えることを確認。次に新登録/別発話の検証/保存を行い、リセット後の保持と質問回答への回帰を確認する。実機の音量/録音品質/誤検出/実保存/電源断耐性は未検証。配布ホストEXEはprotocol v2のまま使えるが、0.3.0-preview.1の状態ログ一覧はStage2までなので、Stage3固有の操作は端末画面で行う。今回EXEは再生成しない。

## 導入作業の委譲と接続試行（2026-10-19）

利用者から、今回開発中の一連のコード生成・デバッグに必要なプロジェクト内の追加/削除/編集とデバイスフラッシュの権限を委譲された。対象案件内で進め、通常の再書込みごとに承認を求め直さない。実機で必要な物理ボタン操作と聴取/表示の確認は利用者に依頼する。

COM4のUSB VID:PID=303A:1001、候補SHA256一致、監視/PC起動プロセスなしを確認し、flash_application_only.pyで0x10000への導入を試行。自動接続はNo serial data receivedで終了コード1。消去/書込み開始前で、第3段階の導入は未完了。前回同様、利用者による本体リセットボタン長押しでのダウンロードモード移行後に再試行する。接触不良そのものの原因究明は保留を維持する。

## アプリ書込み成功・通常起動の確認待ち（2026-10-19）

利用者のダウンロードモード移行後、COM4を再確認し、同じ候補1,469,328 bytesを0x10000へ書込み。消去範囲0x10000〜0x176fff、Hash of data verified、終了コード0。設定・Wi-Fi・登録を含むSPIFFS/NVS/パーティション表は書換え対象外。自動リセット後、制御線を解放した8秒間のGEM INFO照会で状態応答0件、panic/brownout検出なし。無応答を通常起動成功とは扱わず、利用者へ本体リセット短押しを依頼した。証跡は.pio/device-check/face-stage3-flash-20261019-r02.jsonとface-stage3-boot-20261019-r01.json。実マイク/API呼出しは0回、登録取消/再登録/リセット後保持/会話回帰は未確認。

## 通常起動と既存登録の読込確認（2026-10-19）

本体リセット短押し後、利用者が画面表示を確認。USB状態照会でGEM WAKE available=1、READY v2、UI stage=3 mode=2 asleep=1 mic=0を受信。既存登録が読め、Sleepingでマイク停止していることを確認した。configured=0 wifi=0 pc=0、CAPTURE/RESULTはNONE。証跡.pio/device-check/face-stage3-boot-20261019-r02.json。次は録音を伴わない登録画面の取消確認。質問音声/API送信はまだ行っていない。

登録取消の案内後の状態専用監視はusb_errorで終了し、取消結果を示す状態行は未取得。証跡.pio/device-check/face-stage3-cancel-20261019-r01.json。実機の異常終了とは断定しない。利用者による画面上の取消結果は回答待ちで、取消成功は未確認。USB接触不良の原因究明保留を維持し、今回追加調査は行わない。監視用の診断スクリプトは.pio/device-check/observe_stage3.py（既存の許可リストを使い、状態照会のみ、音声/秘密情報は保存しない）。

## 登録入口と無操作取消の再確認（2026-10-19）

利用者が最初の登録操作後にReadyと報告。状態照会でもstage=3 mode=4 asleep=0 mic=0を確認。開始状態をSleepingにそろえ、表示が変わるまで保持してから離す手順で再確認した。利用者の表示報告はSettingsで、これは登録画面の正しい上段タイトル。従来の案内は下部のClick: record callだけを指定していたため、上段タイトルと補助文を区別する説明へ修正した。最初の操作がReadyになった直接原因（開始状態/保持時間/接点など）は未確定で、コード不具合とは断定しない。

端末記録ではSleeping（22,797ms）→登録画面Setup/mic=0（37,610ms）→ENROLL result=CANCEL（67,641ms）→Sleeping/mic=0（69,469ms）を確認。登録画面から約30秒で無操作取消され、質問録音/回答はNONEのまま。登録入口と無操作取消は実機で確認できた。証跡.pio/device-check/face-stage3-entry-repro-20261019-r01.json、直前の状態はface-stage3-entry-state-20261019-r01.json。既存のホスト登録UI11項目も再実行成功。ファームウェアの変更/追加フラッシュはなし。次は長押し取消を実機確認し、その後に登録/照合/保存を確認する。

## 長押し取消の実機確認（2026-10-19）

Sleepingから約7秒長押しして離す→上段Settingsで約3秒長押しして離す手順を案内し、利用者がSleepingへ戻ったと回答。記録でもSetup/mic=0（50,797ms）→ENROLL result=CANCEL（56,391ms）→Sleeping/mic=0（58,219ms）を確認した。Setupから約5.6秒の取消で、30秒無操作とは区別できる。録音・質問・回答は始まっておらず、保存操作には進んでいない。証跡.pio/device-check/face-stage3-manual-cancel-20261019-r01.json。登録入口/無操作取消/長押し取消の基本動作を実機確認済み。次は今までの呼びかけを録音→別発話照合→明示短押し保存する実機確認で、Googleへの送信は行わない。
