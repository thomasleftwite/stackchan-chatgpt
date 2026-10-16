# TODO

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

詳細な証拠、絶対パス、未確認事項は [検証記録](docs/validation.md) を参照する。

## 今回の要約・同期（2026-10-16）

| 状態 | 項目 | 完了日 | 成果物・検証 |
|---|---|---|---|
| 完了 | 会話の短い再開要約 | 2026-10-16 | docs/RESUME.md R01、docs/HANDOFF_2026-10-16.md R9。現在状態/起動/制限/残作業/参照資料を整理 |
| 完了 | Gemini中継・実機成功までのGitHub push/main統合 | 2026-10-16 | [PR #2](https://github.com/thomasleftwite/stackchan-chatgpt/pull/2) MERGED、merge 9e2b0c0。main同期、秘密設定のバイト一致/除外/未追跡を確認。PC25/25、C++19/19、ビルド2/2成功 |
