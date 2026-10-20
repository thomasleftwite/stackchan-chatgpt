# TODO

現在地・未検証項目は [開発経緯と再開地点](docs/RESUME.md) を優先。以下の過去の日付付き行は当時の状態として保持する。

## Windowsホスト0.5・資料のGitHub統合（2026-10-21）

| 状態 | 項目 | 完了日 | 成果物・検証 |
|---|---|---|---|
| 完了 | コード不要接続・失敗分類・再開資料・構成スライドのpush/main統合 | 2026-10-21 | [PR #6](https://github.com/thomasleftwite/stackchan-chatgpt/pull/6) MERGED、merge e106a54。docs/GITHUB_SYNC_2026-10-21.md。Python39/Node32/C++145、3構成ビルド成功。182公開ファイル/72履歴blobの秘密値一致0。main同期・検証ツリー一致・秘密設定4パス保持を確認。端末操作/実API試験は追加していない |

## 開発経緯の整理と構成スライド更新（2026-10-21）

| 状態 | 項目 | 完了日 | 成果物・検証 |
|---|---|---|---|
| 完了 | 現行Windows版への再開資料一本化 | 2026-10-21 | docs/RESUME.md R10、README.md、docs/HOTSPOT_TRUST.md R04。旧資料全文をdocs/archive/2026-10-21へ保持し、現行方式と確認済み/未検証を区別。相対リンク・Git基点/リモート参照を確認 |
| 完了 | 構成スライドR02更新・元版保管 | 2026-10-21 | docs/architecture-slide.pptx、docs/ARCHITECTURE_SLIDE_PRESERVATION.md R02。1枚/58編集可能図形/元寸法・フォントを維持。構造・配置検査0件、最終ファイル再読込・画像表示確認。PowerPoint実アプリ確認は未実施 |

## GitHub同期（2026-10-20）

| 状態 | 項目 | 完了日 | 成果物・検証 |
|---|---|---|---|
| 完了 | 顔表示・呼びかけ登録R07・Windowsホストのpush/main統合 | 2026-10-20 | PR #4 MERGED、merge 93a9071。docs/GITHUB_SYNC_2026-10-20.md。Python28/サーバー26/C++130項目、3構成ビルド、ローカルPCM自己診断成功。ローカルmainへ反映、検証済みツリー一致、秘密設定2件の保持/除外/未追跡を確認。配布EXE再生成・別PC実会話は未実施 |

対象: AtomS3R + Atomic Voice Base（旧称Atomic Echo Base、A149）/ 開発版0.1.0-dev。更新日: 2026-10-16（日本時間）。
目的: 既存の独立プロジェクト実装を引き継ぎ、オフライン検証から実機・実API検証へ進める。成果物はこのファイルがある案件フォルダ内に保存する。

| 状態 | 項目 | 完了日 | 成果物・検証 |
|---|---|---|---|
| 完了 | 設計・セットアップ | 2026-10-15 | docs/design-research.md、docs/implementation-plan.md。GitHub main acc1d58 |
| 完了 | 手元実装とGitHub履歴の接続、既存ファイル保持 | 2026-10-15 | main由来のcodex/resume-implementationへ接続。ローカル変更を保持 |
| 完了 | PlatformIOのビルド不備修正 | 2026-10-15 | src、include、platformio.ini、tools/prepare_websockets.py。ファームウェアビルド成功 |
| 完了 | 発話判定の不具合修正とホストテスト | 2026-10-15 | tests/conversation_rules_test.cpp、tools/run_host_tests.py。14/14成功 |
| 完了 | SPIFFS生成と内容確認 | 2026-10-15 | .pio/build/atoms3r/spiffs.bin。設定・公開CA・空キー例の3件のみ |
| 完了 | 再現手順・検証範囲の更新 | 2026-10-15 | README.md、docs/dependencies.md、docs/validation.md |
| 未実施 | 独立レビュー | — | ビルド/ホストテストで扱わない通信・音声・保存の確認を含む |
| 完了 | 実機接続先・チップ確認 | 2026-10-15 | COM4、ESP32-S3-PICO-1 rev0.2、Flash/PSRAM各8 MB。詳細はdocs/validation.md |
| 完了 | アプリ本体のみの書込み | 2026-10-15 | 0x10000へ1,910,576 bytes、esptoolハッシュ検証成功。既存のブートローダー/パーティション表/設定領域は保持 |
| 完了 | 新アプリの起動と画面表示 | 2026-10-15 | ユーザーが「設定領域なし」「Upload Filesystem」を確認。psramFoundチェック通過。PSRAM全領域の読書き・録音再生は未検証 |
| 完了 | プロジェクト用パーティション・SPIFFSの書込み | 2026-10-15 | 領域初期化・移行を承認後、COM4へ5イメージ書込み、5/5ハッシュ検証成功 |
| 完了 | キーなし初回起動の不具合修正と実機回帰確認 | 2026-10-15 | src/Config.cpp、tools/check_boot_config.py。不在ファイルのディレクトリ判定を修正しFAIL→PASS、修正版アプリ書込みのハッシュ検証成功 |
| 完了 | WiFiManager設定APの起動・検出 | 2026-10-15 | ユーザーがスマートフォンでStackchan-Setupの表示を確認。AP接続・家庭内Wi-Fi設定は未確認 |
| 一部確認 | Wi-Fi設定操作後の待機移行 | — | 2026-10-16にユーザーが「待機中」「APIキー未設定」を確認。Wi-Fi未接続/音声検出停止の追加表示を確認中。接続保存・再起動後の再接続は未検証 |
| 一部確認 | 表示、PSRAM、録音再生、Wi-Fi、登録保存 | — | 表示とPSRAM起動時チェック、開始音、通常版での呼びかけ保存・リセット後の保持/検出を確認。PSRAM全領域・音声録音再生品質・Wi-Fiは未検証 |
| 基本確認済み | Gemini実APIと実機統合試験 | 2026-10-16 | 実マイク16kHz→PC→Gemini→端末24kHz回答再生1回成功、ユーザー聴取確認。従来のOpenAI段階は未実施。キーや音声本文をログへ残さない |
| 未実施 | ウェイク精度、遅延、異常系、長時間試験 | — | 実測結果を記録。リリース前に要確認 |
| 完了 | 開始音・呼びかけ再登録の調整と通常版回帰確認 | 2026-10-16 | src/AudioDevice.cpp、.pio/build/atoms3r/firmware.bin。開始音0.5秒・マイク増幅4倍の診断版2回と通常版1回で登録成功。通常版の聴取/画面をユーザー確認、監視もPASS。COM4アプリのみ書込み・ハッシュ検証成功 |
| 完了 | リセット後の登録保持とローカル呼びかけ検出の基本確認 | 2026-10-16 | docs/validation.md。通常版でユーザーが短押しリセット後、ボタンなし発話で「接続・API設定を確認してください」への遷移を確認。未登録状態からの初回保存の再現、電源断後保持、精度/誤検出の定量評価は別途未実施 |
| 完了 | ローカルAPI設定のGit除外と公開前確認 | 2026-10-16 | .gitignore、README.md。secrets.jsonとバックアップを除外、実設定の未追跡・空サンプル・公開対象52ファイルに実キー一致なしを確認。Geminiのみ設定済み、端末への導入・実API試験は未実施 |
| 完了 | 検証済み開発版のGitHubへのpush・main統合 | 2026-10-16 | [PR #1](https://github.com/thomasleftwite/stackchan-chatgpt/pull/1)、merge 67a984c。ファームウェア再ビルド・ホストテスト14/14成功。実キーと生成物を除外して公開、main同期後もローカル設定のバイト一致・未追跡を確認 |
| 完了 | GitHub同期時点の会話要約・再開用記録 | 2026-10-16 | docs/HANDOFF_2026-10-16.md R2。main/origin/main=d06ecd5を基準に成果・未検証・秘密設定・再開順を整理。再開後のクラウド稼働先指定を追記 |
| 完了 | Node.js中継 + Gemini Liveの次期構成の一次検討 | 2026-10-16 | docs/GEMINI_LIVE_ARCHITECTURE_REVIEW.md R01。公式モデル/音声仕様/SDKを確認し、16 kHz入力/24 kHz出力、キーのサーバ管理、初期半二重、通信/終端/有界キュー/検証計画を記録。設計案であり未実装・実API未検証 |
| 一部実装 | 次期構成のサーバ・端末実装と段階検証 | — | PC/端末v2で1質問1回答を実装・実機成功。連続録音・会話履歴・ウェイク統合は未実装。クラウド保留 |
| 完了 | AI Studio / Cloud Run方式の資料による実現可能性確認 | 2026-10-16 | docs/AI_STUDIO_CLOUD_FEASIBILITY_2026-10-16.md R01。Node.js/Secrets、Cloud Run公開/WSS、接続期限・認証・利用枠を公式資料で照合。実デプロイの独自upgrade・実機・対象キーの会話は未検証 |
| 完了 | 中継実装の初期準備とPCM形式モジュール | 2026-10-16 | server/README.md、protocol.ts、package.json/lock。依存固定、TypeScriptビルド、単体2/2成功。Node.js24.15.0。クラウド通信や会話状態管理は未実装 |
| 保留 | AI Studio公開先での認証付きWSS疎通 | — | docs/STARTER_ECHO_DEPLOYMENT.md R02。公開設定でStarter対象外を確認後、ユーザーが公開保留・ローカル検証を指定。生成版にも形式/制限差分とプレビュー404が残る。課金/Secret/公開は未実施 |
| 完了 | Gemini未使用の認証付きechoと公開準備 | 2026-10-16 | server/server.ts、echo.ts、smoke.ts。型検査・ローカル11/11成功。Dockerfileと秘密情報を除外したソースZIPを用意。コンテナビルドと外部WSS/実機確認は未実施 |
| 保留 | クラウド公開 | — | ユーザーが公開保留・ローカル検証へ切替を指定。課金/Secret/公開は変更しない |
| 完了 | PC内の連続PCM通信 | 2026-10-16 | server/test/pcm-stream.test.ts。16kHzの100フレーム/32,000サンプルがbyte一致、サーバ全12/12成功。実機LANは未検証 |
| 基本確認済み | 端末単体の16kHz録音・24kHz再生 | 2026-10-16 | docs/LOCAL_AUDIO_VALIDATION_2026-10-16.md R01。専用診断版をCOM4のアプリのみへ書込み、ハッシュ検証・起動・確認音・録音した自声の再生を確認。ホスト17/17成功。録音時間/音量統計・音質定量評価は未実施。現在端末は診断版 |
| 基本確認済み | 実機とPCの認証付きWebSocket・PCM通信 | 2026-10-16 | docs/LAN_PCM_VALIDATION_2026-10-16.md R01。合成音/自声の16kHz上り48,000・24kHz下り72,000サンプル、150フレーム全一致とclose1000。録音3,054ms、clip0、ユーザーが自然な再生を確認。サーバ15/15成功。現在LAN診断版R04、PC一時サーバ停止済み |
| 完了 | 保存済みGeminiキーの実API確認 | 2026-10-16 | server/private-config.ts、gemini-check.ts。gemini-3.8-live HTTP200と24kHz音声22,081サンプルを確認。既存設定を読込み、OpenAIキー不要 |
| 完了 | ローカルPCのGemini Live中継と起動環境 | 2026-10-16 | Start-Gemini-PC.cmd、server/gemini-pc.ts、gemini-server.ts、gemini-turn.ts、gemini-isolated.ts、gemini-worker.ts。型検査/25件成功、単独起動healthz200、認証/上限/期限/異常系、非公開設定作成と保持を確認 |
| 完了 | PCの16kHz音声入力からGeminiの24kHz回答取得 | 2026-10-16 | docs/GEMINI_PC_VALIDATION_2026-10-16.md R01、server/gemini-local-check.ts。生成試験音声13,440入力→78,000出力サンプル/163フレーム、正常終了。端末再生完了は模擬通知で実聴取なし |
| 基本確認済み | 実機のGemini中継v2対応・回答再生 | 2026-10-16 | docs/GEMINI_DEVICE_VALIDATION_2026-10-16.md R01、src/LocalGemini.cpp、include/LivePcm.h。アプリ限定書込み・ハッシュ検証成功、C++19/19。3秒入力48,000→回答47,040サンプル/98フレーム、実再生終了まで14,454ms、ユーザーが日本語回答を確認 |
| 完了 | 実機向けPC起動・USBのRAM設定 | 2026-10-16 | Start-Gemini-Device.cmd、server/local-gemini.ts、tools/run_gemini_device.py。LANの空きポート/ランダムトークンをUSB転送し保存しない。認証拒否・合成可変長12,001サンプル再生を確認。試験後サービス停止、最長約10分の一時検証用 |
| 未実装 | 連続送受信・会話履歴・長時間評価 | — | 現PC版は最大5秒入力/10秒回答の1質問1回答。連続DMA取得、低遅延送信、複数ターン、再接続/異常系の実機確認、TLS/クラウドは未検証 |
| 基本確認済み | 環境音に合わせた発話終了判定・無発話では送信しない | 2026-10-16 | include/UtteranceWindow.h、src/LocalGemini.cpp、docs/VARIABLE_UTTERANCE_VALIDATION_2026-10-16.md。R02誤検出を修正しR03の無発話2回でEMPTY。環境音RMS219/しきい値750、約6.06秒で終了。R04へ終了時競合の修正を含めてアプリ限定導入済み。C++27/27、PC26/26、ビルド/書込みハッシュ成功 |
| 基本確認済み | 可変長の実質問からGemini回答再生 | 2026-10-16 | docs/RECORD_START_FIX_2026-10-16.md。R05で発話末尾無音により37,120サンプル/2.32秒を送信→24kHz回答56,400/118フレーム→再生完了13,743ms。ユーザーがSpeak nowへの移行と回答を確認 |
| 完了 | Release表示に残る録音開始前の不具合修正 | 2026-10-16 | src/LocalGemini.cpp、tests/gemini_ui_test.cpp、tools/run_host_tests.py --suite gemini-ui。実ソースで未設定/切断の症状をFAIL再現→修正後6項目成功。PCの稼働通知/停止通知/期限、ボタン離し待ち上限を追加。R05アプリ限定書込みハッシュ・実機再試験成功、サービス停止済み |

詳細な証拠、絶対パス、未確認事項は [検証記録](docs/validation.md) を参照する。

## 今回の要約・同期（2026-10-16）

| 状態 | 項目 | 完了日 | 成果物・検証 |
|---|---|---|---|
| 完了 | 会話の短い再開要約 | 2026-10-16 | docs/RESUME.md R01、docs/HANDOFF_2026-10-16.md R9。現在状態/起動/制限/残作業/参照資料を整理 |
| 完了 | Gemini中継・実機成功までのGitHub push/main統合 | 2026-10-16 | [PR #2](https://github.com/thomasleftwite/stackchan-chatgpt/pull/2) MERGED、merge 9e2b0c0。main同期、秘密設定のバイト一致/除外/未追跡を確認。PC25/25、C++19/19、ビルド2/2成功 |

## 会話成立時点の記録・同期（2026-10-16）

| 状態 | 項目 | 完了日 | 成果物・検証 |
|---|---|---|---|
| 完了 | 会話成立段階の開発進捗整理 | 2026-10-16 | docs/DEVELOPMENT_PROGRESS_2026-10-16.md R01、RESUME R04、HANDOFF R12。PC26/26、C++27/27、録音UI6項目、端末ビルド成功。今回の端末/API追加操作なし |
| 未解明・継続観察 | Start-Gemini-Device.cmdの一度の終了 | — | ユーザー報告で再起動後の会話は正常。終了コード/経過時間未取得、約10分の期限終了との区別も未確認。再発時に秘密情報を除いた段階/コードを確認 |
| 完了 | 可変長録音・録音開始修正・進捗資料のpush/main統合 | 2026-10-16 | [PR #3](https://github.com/thomasleftwite/stackchan-chatgpt/pull/3) MERGED、merge a60d165。未公開3コミット/125 blobと公開ファイル114件で秘密値一致0。ローカルmain同期、設定2件のバイト一致/除外/未追跡、検証済みツリー一致を確認 |


## 顔表示・呼びかけの段階開発（2026-10-16）

| 状態 | 項目 | 完了日 | 成果物・検証 |
|---|---|---|---|
| 完了 | 最終状態遷移・音声入力停止区間・段階計画の合意記録 | 2026-10-16 | docs/FACE_MODES_STAGE1.md R01。スリープ→ボタン→呼びかけ待ち→呼びかけ→合図→質問、回答再生終了と残響待ちまで入力停止 |
| 完了 | 第1段階の顔/状態/ボタン実装とオフライン検証 | 2026-10-16 | include/DeviceMode.h、FaceUI.h、src/FaceUI.cpp、LocalGemini.cpp、FaceDemo.cpp。新規15項目/既存UI6項目/C++27/PC26成功、3構成ビルド成功 |
| 完了 | 顔付き端末版のアプリ限定導入とスリープ起動確認 | 2026-10-16 | .pio/firmware-snapshots/gemini-face-stage1-r01.bin。COM4書込みハッシュ成功、GEM UI stage=1 mode=2 asleep=1 mic=0。設定領域は保持、質問録音/API追加呼出し0 |
| 完了 | 第1段階の顔表示と短押し切替の実機確認 | 2026-10-16 | docs/FACE_MODES_STAGE1.md R02。ユーザーが顔とSleeping→短押しReady→短押しSleepingの切替を確認 |
| 完了 | 第1段階の合成PCM往復と再生完了 | 2026-10-16 | .pio/device-check/face-stage1-fixture-r02.json。入力24,000→出力12,001/26フレーム、2,610ms/played=1。マイク/API未使用、Sleep/mic=0、正常停止確認。初回起動失敗は原因未解明 |
| 基本確認済み | 第1段階の実機API音声回帰・負荷と異常系 | 2026-10-19（基本回帰） | docs/FACE_MODES_STAGE1.md R03。再試験でPC API PASS、ユーザーが日本語聴取/口パク/Ready復帰の3点を確認。描画負荷/音切れ定量評価/中止/自動スリープ/異常系は未検証 |
| 基本確認済み | 第2段階: 登録済み1パターンとGemini端末版の統合 | 2026-10-19（基本動作） | docs/WAKE_STAGE2.md R02。実機導入・登録読込あり、PC実API2回PASS、ユーザーが回答後の再反応を確認。精度/再生中停止/残響/異常系は未検証 |
| R07基本動作完了 | 第3段階: 専用登録・開始合図 | 2026-10-19（完了） | docs/WAKE_STAGE3_R07.md。登録/質問を準備→ビープ→700ms後の発話へ統一。149項目/2構成ビルド/2軸レビュー成功。実機で登録3回→自動保存、通常質問2回→回答/Ready復帰を利用者とログで確認。5回とも受付700〜701ms。検証用PCサービス正常停止。精度長期評価/電源断耐性は別途 |
| 未実施 | 第3段階の実機登録・複数サンプル・品質/精度評価 | — | まず取消後の旧呼びかけ、登録/照合/保存、リセット後保持、会話回帰を確認。複数サンプル、距離/雑音/似た語の検出率評価は後段 |

| 状態 | 項目 | 完了日 | 成果物・検証 |
|---|---|---|---|
| 完了 | 第2段階R01のアプリ限定導入と起動 | 2026-10-19 | docs/WAKE_STAGE2.md。COM4の0x10000へ1,465,824 bytes、ハッシュ検証成功。手動リセット後stage=2/Sleep/mic=0とPC/Wi-Fi接続正常を確認。登録読込み/実呼びかけは別途確認待ち |

| 保留（利用者指示） | Stage2実機監視ツールの終了コード1の原因究明 | — | 2026-10-19、docs/USB_MONITOR_DIAGNOSTICS_2026-10-19.md R02。利用者観察でUSB接触不良による切断/再接続の可能性。USB通信エラーと停止応答未確認を区別。原因確定/追加再現は保留 |


## 安定性確認・診断記録（2026-10-19）

| 状態 | 項目 | 完了日 | 成果物・検証 |
|---|---|---|---|
| 完了 | 監視終了理由の安全な分類とUSB行読込み改善 | 2026-10-19 | tools/run_gemini_device.py、tests/test_gemini_coordinator.py、docs/USB_MONITOR_DIAGNOSTICS_2026-10-19.md R02。診断14/14成功、例外文を出さず段階/経過/理由を記録。分割行/複数行/一括読込みを検証。物理切断の修復ではない |
| 完了 | 呼びかけ検出有効時の通信/再生/700ms停止とPC停止/復帰のホスト回帰 | 2026-10-19 | tests/face_ui_test.cpp、27/27成功（追加4項目）。機器/検出器模擬。実機全境界の証明ではない |
| 基本確認済み | Ready無操作約3分の自動スリープ | 2026-10-19 | 利用者がSleeping復帰/勝手な開始なしを確認。端末状態遷移ログは不足。短時間の環境観察であり誤検出率保証ではない |


## Stage2追加実機確認（2026-10-19）

| 状態 | 項目 | 完了日 | 成果物・検証 |
|---|---|---|---|
| 基本確認済み | 再生中の誤反応・中止・回答後受付 | 2026-10-19 | docs/WAKE_STAGE2.md。利用者が提案1をOKと回答。個別の計測値/実施回数は未取得 |
| 基本確認済み | 呼びかけ精度の簡易評価 | 2026-10-19 | docs/WAKE_STAGE2.md。利用者が提案3をOKと回答。検出率/誤検出率の定量値は未取得 |
| 仕様許容・追加対応終了 | PCサービス停止後もOnline表示が残る | 2026-10-19（判断） | docs/PC_SERVICE_STATUS_2026-10-19.md R02。利用者がStart-Gemini-PC.cmdで評価、20秒以上残留と回答。追加監視通信は不要、次の問いかけ時の接続失敗で表示を更新する現状を許容。原因解明/停止復帰試験合格ではない。実機用はStart-Gemini-Device.cmd |


## Windows配布ホスト（2026-10-19）

| 状態 | 項目 | 完了日 | 成果物・検証 |
|---|---|---|---|
| 完了 | 開発環境不要の実機用ホストEXE作成 | 2026-10-19 | desktop/README.md、releases/StackchanHost-0.3.0-preview.1-win-x64/StackchanHost.exe、同版ZIP。Node/Python/Tk/pyserial/実行依存同梱、APIキーなし。単独コピー/PATH制限で自己診断8項目PASS、設定7/監視17/サーバ26件成功。配布入力の実キー一致0、アーカイブ機密名0 |
| 未確認 | 移行先Windows PCでの起動・USB認識・実会話 | — | 同版VALIDATION.md。現PCの模擬/単独コピー試験とは区別。移行先でGeminiキー入力とCOM/LAN選択、必要なWindows接続許可を確認 |


## Wi-Fi設定機能の維持・追加（2026-10-20）

Gemini端末版R07にWiFiManagerを統合しCOM4アプリ限定書込み/ハッシュ検証成功。Wi-Fi接続失敗時のAP、画面ボタンを押した起動での強制AP、180秒上限、保存済み設定非消去、USBへのパスワード出力なし。Python29件/端末ビルド成功。C++追加回帰はSDK不足で未実行、実機AP/設定保存/会話は確認待ち。詳細docs/WIFI_PROVISIONING.md。
確認済み機能を通常版に維持し、機能削除が必要な場合は理由を説明する。通常通信はWi-Fi、補足診断はUSB。USB診断経路は残す。PC通知/稼働通知のUSB依存の解消は後段。

2026-10-20追記: 利用者指示で設定APの180秒制限を廃止し標準WiFiManagerの無期限設定へ変更。接続失敗後も再入力可能。USBのAP_JOIN/AP_LEAVE/AP_STOP診断を追加。Python30件成功、Android接続の切断原因は未確定。


## コード入力不要のホットスポット信頼方式（2026-10-21）

| 状態 | 項目 | 完了日 | 成果物・検証 |
|---|---|---|---|
| 完了（実装・PC検証） | ホットスポット参加を信頼しコード入力を廃止 | 2026-10-21 | docs/HOTSPOT_TRUST.md R01、releases/StackchanHost-0.5.0-preview.1-win-x64/StackchanHost.exe、.pio/firmware-snapshots/gemini-hotspot-trust-20261021-r01.bin。Python39/Node32/C++145/3構成ビルド/EXE自己診断11成功、自己診断API0回 |
| 完了（導入・基本接続） | 新方式のアプリ限定導入・コードなし自動接続 | 2026-10-21 | docs/HOTSPOT_TRUST.md R02。利用者明示承認、COM4の実機表確認/0x10000への書込み/ハッシュ検証成功。利用者がコードなし接続を確認、端末HOTSPOT/wifi=1/pc=1/WAKE available=1。Windows受信規則変更なし |
| 完了（基本実機確認） | 新方式での実会話・呼びかけ再登録・リセット後自動復帰 | 2026-10-21 | docs/HOTSPOT_TRUST.md R03。登録SAVED、照合188/MATCH、gen3 PASS/played=1、利用者が音声/口パク/待受復帰を確認。端末リセット後はコードなし自動接続/照合187/MATCH/gen1 PASS/played=1、利用者も再会話を確認。質問受付gap700/701ms |
| 未検証 | 長期検出精度・電源断耐性・別PC移行 | — | 基本実機確認・端末リセット試験と区別する。追加のストレス試験や実施予定の自動監視は今回設定しない |

## 無線版の開発再開（2026-10-21）

| 状態 | 項目 | 完了日 | 成果物・検証 |
|---|---|---|---|
| 完了 | GitHub整合性確認とPC上の再検証 | 2026-10-21 | docs/WIRELESS_RESUME_2026-10-21.md R02。リモートmain3283ad2と開発基点一致、ローカルにスライド保存1件から再開。Python34/Node28/C++144/EXE自己診断10項目成功、API0回。テスト用文字列コピーをMSVC対応 |
| 完了（導入・基本起動） | 無線版のアプリ限定導入と起動 | 2026-10-21 | .pio/firmware-snapshots/gemini-wireless-resume-20261021-r01.bin、SHA256先頭16799F01、1,606,672 bytes。利用者明示承認、COM4の表確認/0x10000への書込みハッシュ検証成功。リセット後にREADY/TLS/wifi=1/WAKE available=1/asleep=1/mic=0を確認。pc=0、実会話は未実施 |
| 完了（実機接続） | PCホットスポット接続・TLS認証・登録保存 | 2026-10-21 | docs/WIRELESS_RESUME_2026-10-21.md R06。接続先再設定後に利用者が1台を確認。GEM PAIR SAVED/LINK TLS/wifi=1/pc=1。Windows受信規則は変更なし |
| 完了（手動質問の基本確認） | API認証復旧・TLS経由の実質問と回答再生 | 2026-10-21 | docs/WIRELESS_RESUME_2026-10-21.md R08。キー再入力後に利用者が回答音声を確認。端末PASS/played=1、input24000/output64560/135frames/18149ms。口パク・長期安定・再起動保持は別途 |
| 完了（後続0.5で基本確認） | 呼びかけ検出・3回再登録 | 2026-10-21 | 過程はdocs/WIRELESS_RESUME_2026-10-21.md R08、解決結果はdocs/HOTSPOT_TRUST.md R03。PCコード更新操作を廃止した0.5で3回再登録SAVED、距離188/187でMATCHと回答再生成功。リセット後の登録保持・再会話も確認 |
| 完了（実装・分類の実機確認） | ホストの安全な失敗分類表示と配布版番号整合 | 2026-10-21 | releases/StackchanHost-0.4.0-preview.2-win-x64/StackchanHost.exe。固定分類/取得できたHTTP番号だけを表示。Python37/Node29/EXE自己診断10成功、自己診断API0回、旧EXEハッシュ維持。実機質問でAUTH分類を利用者画面で確認。回答成功は未確認 |

## 構成説明スライドの保管（2026-10-20）

| 状態 | 項目 | 完了日 | 成果物・検証 |
|---|---|---|---|
| 完了 | 復元PPTXのGit履歴保存と所在・復旧手順の整備 | 2026-10-20 | docs/architecture-slide.pptx、docs/ARCHITECTURE_SLIDE_PRESERVATION.md R01、READMEリンク、.gitignore除外解除。ZIP/XML・SHA256一致・Git保存を確認。表示確認・GitHubへのpushは未実施 |

## USB不要のホットスポット構成（2026-10-20）

利用者選択: PCは共有Wi-Fi、端末はPCのモバイルホットスポット。固定IPや毎起動のUSB転送を廃止し、初回画面コード登録/端末固有鍵の永続化/TLS1.2 DHE-PSK/PCゲートウェイTCP48443の構成を実装。USB診断と既存機能を維持。Python34/サーバー28/C++144項目/3構成ビルド/最終EXE自己診断10項目成功。配布版0.4.0-preview.1を生成。
新ファームウェアの実機書込みは自動承認審査で拒否され、明示的な導入承認待ち。Windows設定は未変更。実機ペアリング・会話・再起動後自動接続は未確認。詳細docs/WIRELESS_HOST.md。
