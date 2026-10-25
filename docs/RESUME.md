# 現在地と再開手順

## ホスト新ボタンへの組込み（2026-10-25）

利用者の続行指示を受け、[実装R01](HOST_AP_PROVISIONING_IMPLEMENTATION_R01.md)でホスト0.7.0-rc.1候補を追加。新ボタンから待受先行→必要ならAP設定→20秒維持→PC復帰へ進む。有線上流・未接続Wi-Fi1台を初回自動設定の対象とした。Python全74件成功。EXE自己診断・レビュー・新UIの実機接続は結果追記待ち。旧公開0.6.0-rc.1と端末ファームウェアは保持する。

## 次段階の仕様整理（2026-10-25）

`codex/host-ap-provisioning`で[ホストからのAP設定R01](HOST_AP_PROVISIONING_R01.md)を整理後、利用者の続行・設定送信許可を受けて[Windows実証R01](HOST_AP_PROVISIONING_WINDOWS_PROBE_R01.md)を実施。有線PCでAP一時接続・設定送信・PC復帰・ホットスポット参加0→1、既存ホスト開始後のPC Online / Sleepingを基本確認。受付後20秒維持の試行で成功、直後離脱試行の失敗原因は未確定。

診断ツール2件・オフライン12件成功。ホスト製品コード・配布EXE・端末ファームウェアは未変更。端末の接続先Wi-Fi情報だけを既存WiFiManagerで設定した。新ボタンへの組込み、上流Wi-Fi復帰、通常再起動、会話/音声/口パクは未検証。下記RC1の動作・成果物は引き続き基準とする。

## 現行RC1の基準

2026-10-21 / R16。現行はAtomS3R + Atomic Voice Base A149、Windowsホスト **0.6.0-rc.1**、Gemini Liveの構成。

## 現在の動作

- 通常利用はWi-Fiのみ。PCの2.4GHzホットスポットへ参加し、端末はDHCPゲートウェイのTCP48443へ自動接続する。
- スリープ起動、短押しでReady、登録済みの呼びかけで質問開始。準備→ビープ終了→約700ms後に発話。質問と回答は半二重。
- PCで声のサンプルを最後まで聴いて採用すると通常一覧へ即時追加・選択・保存する。試聴時は利用者が端末をSleepingにする。
- 上段はモード名のみ、診断数値・音量バーなし。回答前200ms待機、口パク、待受復帰を維持する。
- APIキーはホスト限定。通常USB監視を行わず、端末更新はapp0のみで設定・登録を保持する。

## 確認済みと限界

RC1導入の書込みハッシュ検証成功。導入後の表示/会話/待受確認依頼に利用者がokと回答。直前の待機版では冒頭欠け無しを明示確認。リセット約5秒後の再接続、60秒後接続1台と会話復帰も基本確認済み。

最終コード検証: Python49、Node37、C++169、端末3構成、EXE自己診断15項目が成功。個別結果は[検証一覧](validation.md)。長期精度、会議音声の誤検出、物理電源断、別PC/他OS、全声の日本語品質は未検証または継続評価。

## 成果物と依存

- ホスト: releases/StackchanHost-0.6.0-rc.1-win-x64/StackchanHost.exe
- 端末: releases/Stackchan-AtomS3R-0.6.0-rc.1/firmware.bin。導入用保持先は.pio/firmware-snapshots/gemini-release-20261021-rc01.bin。
- [成果物の全SHA・導入証拠](RELEASE_CANDIDATE_0_6_0_2026-10-21.md)
- 端末: PlatformIO espressif32 6.11.0 / Arduino-ESP32 2.0.17。ホスト配布: Python3.14.4 / Node24.15.0。固定依存はplatformio.ini / server/package-lock.json。

GitHubは[PR #7](https://github.com/thomasleftwite/stackchan-chatgpt/pull/7)でmainへ統合済み。検証済みEXE等は[RC1 Release](https://github.com/thomasleftwite/stackchan-chatgpt/releases/tag/v0.6.0-rc.1)へ公開。個人設定・キーは含めない。

## 次に読むもの

[利用手順](../desktop/README.ja.md) → [通信仕様](HOTSPOT_TRUST.md) → [ToDo](../TODO.md)。Raspberry Pi OS版、macOS版、ESP32-S3直接Gemini構成は将来の未着手項目。今回は実装しない。

履歴は[短い開発記録](DEVELOPMENT_HISTORY.md)、改訂前全文は[保管資料](archive/2026-10-21/rc1-before-compaction/README.md)。公開状況は[GITHUB_RC1_SYNC_2026-10-21.md](GITHUB_RC1_SYNC_2026-10-21.md)でpush・PR・merge・Releaseを区別して確認する。
