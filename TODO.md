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
| 未実施 | 実APIの各段階と統合試験 | — | 明示承認後に実施。キーや音声本文をログへ残さない |
| 未実施 | ウェイク精度、遅延、異常系、長時間試験 | — | 実測結果を記録。リリース前に要確認 |
| 完了 | 開始音・呼びかけ再登録の調整と通常版回帰確認 | 2026-10-16 | src/AudioDevice.cpp、.pio/build/atoms3r/firmware.bin。開始音0.5秒・マイク増幅4倍の診断版2回と通常版1回で登録成功。通常版の聴取/画面をユーザー確認、監視もPASS。COM4アプリのみ書込み・ハッシュ検証成功 |
| 完了 | リセット後の登録保持とローカル呼びかけ検出の基本確認 | 2026-10-16 | docs/validation.md。通常版でユーザーが短押しリセット後、ボタンなし発話で「接続・API設定を確認してください」への遷移を確認。未登録状態からの初回保存の再現、電源断後保持、精度/誤検出の定量評価は別途未実施 |
| 完了 | ローカルAPI設定のGit除外と公開前確認 | 2026-10-16 | .gitignore、README.md。secrets.jsonとバックアップを除外、実設定の未追跡・空サンプル・公開対象52ファイルに実キー一致なしを確認。Geminiのみ設定済み、端末への導入・実API試験は未実施 |
| 完了 | 検証済み開発版のGitHubへのpush・main統合 | 2026-10-16 | [PR #1](https://github.com/thomasleftwite/stackchan-chatgpt/pull/1)、merge 67a984c。ファームウェア再ビルド・ホストテスト14/14成功。実キーと生成物を除外して公開、main同期後もローカル設定のバイト一致・未追跡を確認 |

詳細な証拠、絶対パス、未確認事項は [検証記録](docs/validation.md) を参照する。
