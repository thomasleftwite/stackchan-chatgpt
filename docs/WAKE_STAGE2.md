# 登録済み呼びかけの統合

版: R02 / 2026-10-19（日本時間）。対象: M5AtomS3R + Atomic Voice Base A149、Arduino-ESP32 2.0.17 / PlatformIO。目的: 顔表示付きPC中継版で、既存の登録済み1パターンから質問を開始する。実装・導入済み。呼びかけからの会話と回答後の再検出は基本確認済み。

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

## 実機試験の手順（下記に実施結果）

1. 合格済みStage1イメージを保持し、第2段階候補のハッシュとアプリ領域のみの変更を確認する。
2. tools/flash_application_only.pyでCOM4の0x10000のみ書込み。既存の確認済みパーティション表を指定し、SPIFFS/NVS/設定/登録は保持。書込み後ハッシュ検証。
3. GEM INFOでstage=2、起動Sleeping/mic=0を確認。GEM WAKE available=1なら既存登録の読込あり。0なら再登録せず原因を調査。
4. Start-Gemini-Device.cmdを起動、短押し、登録済み呼びかけ、合図とSpeak now後に質問。回答、口パク、Ready復帰、2回目の呼びかけを確認。
5. 再生中に呼びかけても録音しないこと、Sleep/切断時mic=0、中止、自動スリープ、再生後の誤反応を確認。音声・本文・キーを保存せず数値/状態だけ記録する。

今回のコード変更を実機成功・精度保証とは扱わない。実機書込みと統合試験は次工程。クラウド公開・課金変更なし。

## ビルド済み候補

3構成（atoms3r / atoms3r-gemini-device / atoms3r-face-demo）の最終ビルド成功。候補: `C:\Users\thoma\SynologyDrive\H\Doc\Src\Codex\projects\ai-stackchan\AI_StackChan_Ex\Stackchan-chatgpt\.pio\firmware-snapshots\gemini-face-stage2-r01.bin`、1,465,824 bytes、SHA256 `C43024F6272FF7D3FB12180517E8B7653A13577F3AEA10E08469717746F3ECE1`。Stage1復旧用イメージのハッシュ一致を確認。実機への導入は未実施。

## 導入試行（2026-10-19）

ユーザー承認後、COM4へ上記SHA256のアプリ限定書込みを2回試行。どちらも接続段階で「Failed to connect to ESP32-S3: No serial data received」で終了コード1。消去・書込み開始前であり、Stage2導入未完了。COM4の列挙は確認済み。次は利用者がAtomS3R本体のリセットボタン長押しでダウンロードモードにした後、ポートを再確認して同じ候補を再試行する。アプリ書込みの許可は取得済みで、再承認は不要。

## アプリ書込み成功・起動確認待ち（2026-10-19）

利用者によるダウンロードモード移行後、COM4へ1,465,824 bytesを書込み、Hash of data verified、終了コード0。消去範囲0x10000〜0x175fff。SPIFFS/NVS/パーティション表は書換え対象外。自動リセット後のGEM INFO応答なし。制御線解放後の8秒確認では受信152 bytes、ROM起動表示あり、panic/Stage2 READYは未検出。新アプリの起動・登録読込は未確認で、利用者の短押しリセット後に再照会する。音声/API試験はまだ実施していない。状態照会証跡: .pio/device-check/face-stage2-boot-20261019-r01.json（状態行0件）。

## 通常起動と接続確認（2026-10-19）

利用者のリセット短押し後、GEM UI stage=2 mode=2 asleep=1 mic=0、READY v2を確認。証跡 .pio/device-check/face-stage2-boot-20261019-r02.json。続いて一時Geminiサービス起動、configured=1 wifi=1 pc=1、起動Sleep/mic=0を確認。API呼出し・録音はまだ0件。起動時のみ出るWAKE availableのログは未取得のため、登録読込みは待受表示/実検出で確認する。ユーザー物理操作による呼びかけ試験待ち。

## 呼びかけ会話と回答後再検出の基本確認（2026-10-19）

利用者が「回答後も再度反応した」と報告。既存登録による呼びかけ→質問→回答→再度呼びかけの基本動作を確認済みとする。Stage1で確認した口パクの第2段階での個別再評価や精度保証までは含めない。

PCの実API処理は2回ともPASS。1回目は入力47,680サンプル（16kHz・2.98秒）、出力74,881サンプル（24kHz・約3.12秒）、API処理8,975ms。2回目は入力52,160サンプル（3.26秒）、出力137,520サンプル（5.73秒）、API処理12,921ms。証跡: `.pio/device-check/face-stage2-live-20261019-r01.json`。今回の監視記録にはGEM WAKE MATCHや端末GEM RESULTが収録されていないため、端末の再生・再検出は利用者報告を根拠とし、PC処理時間を端末応答時間とは扱わない。

監視ツールは「Temporary Gemini device server stopped」に続き、詳細抑止の失敗表示・終了コード1で停止していた。正常停止とは扱わず、時点/直接原因と過去の終了事象との同一性は未解明。再照会時に起動時のGEM WAKE available=1とREADYを受信し、GEM STOPに対するPC OFFLINE、configured=0 wifi=0 pc=0、stage=2/Sleep/mic=0を確認。CAPTURE/RESULTはNONEだった。再起動の発生時点や要因（シリアル再接続によるものか等）は未確定。証跡: `.pio/device-check/face-stage2-after-live-20261019-r01.json`。追加録音・API呼出しなし。

残項目: 再生中の検出停止と700ms残響待ちの実機境界、誤検出/未検出の定量評価、ボタン中止・無操作3分・切断復帰、長時間安定性、監視ツール停止原因。これらを完了扱いにしない。次は第3段階へ拡張する前に、必要な安定性/異常系を確認する。

## 安定性確認の更新（2026-10-19）

ユーザーがReady無操作約3分でSleeping復帰、勝手な開始なしを確認。ホストUIは27/27へ拡充。PC監視ツールの診断14件成功、USB例外と停止応答未確認を区別する情報を追加。利用者観察でUSB接触不良の切断/再接続の可能性が示され、原因究明は利用者指示で保留。最終サービスはstop/PC OFFLINE/終了コード0。端末ファームウェアはStage2 R01のまま。詳細は[USB診断記録](USB_MONITOR_DIAGNOSTICS_2026-10-19.md) R02。実機の再生中停止・残響・切断復帰・定量評価は未検証。

## 利用者による追加確認（2026-10-19）

提案した3項目について、利用者が1（再生中の誤反応/中止/回答後受付）と3（呼びかけ精度の簡易評価）をOKと報告。各試行回数や個別の数値、700msの実測は未取得のため、基本的な利用者確認とし定量評価完了にはしない。

2（PC停止/復帰）では「PCサービスを停止してもOnlineのまま、質問回答時に接続失敗して表情が切り替わり、その後Start PC Service表示」と報告。停止手段と停止後の待ち時間は確認中。現版はUSB稼働通知の期限20秒で、サーバへの質問接続成功とは別の生存表示。UI/受付停止の課題として継続し、USB接触不良の原因究明保留とは区別する。

## PC停止表示の許容仕様（2026-10-19）

利用者はStart-Gemini-PC.cmdで評価し、20秒以上Onlineのままだったと回答。追加の常時監視通信は不要で、次の問いかけ時の接続失敗でStart PC Serviceへ変わる現状でよいと合意。根本原因解明や停止復帰の合格とは扱わず、追加監視の実装/追跡は行わない。実機起動はUSB接続設定を行うStart-Gemini-Device.cmd。PC単独用との区別は[PC停止表示記録](PC_SERVICE_STATUS_2026-10-19.md) R02に整理。
