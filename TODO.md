# ToDo

2026-10-21 / RC1。現在地は[RESUME](docs/RESUME.md)、詳細な旧作業行は[整理前ToDo](docs/archive/2026-10-21/rc1-before-compaction/TODO.md)に保持しています。

## 現行版

| 状態 | 項目 | 日付・成果物・検証 |
|---|---|---|
| 完了 | Wi-Fiホットスポット経由の呼びかけ・Gemini音声対話 | 2026-10-21。USB不要、基本接続/会話/リセット再接続を確認。[通信仕様](docs/HOTSPOT_TRUST.md) |
| 完了 | 声の選択・PC試聴・採用・保存 | 2026-10-21。desktop/host_app.pyほか。PC試聴だけで採用し即時一覧反映。[実装記録](docs/STACKCHANHOST_VOICE_PC_ADOPTION_2026-10-21.md) |
| 完了 | 回答前200ms待機 | 2026-10-21。src/LocalGemini.cpp。アプリ限定導入後、利用者が冒頭欠け無しを確認。[記録](docs/REPLY_START_DELAY_2026-10-21.md) |
| 完了（候補） | RC1の表示整理・ホストWi-Fi固定 | 2026-10-21。releases/StackchanHost-0.6.0-rc.1-win-x64/StackchanHost.exe、releases/Stackchan-AtomS3R-0.6.0-rc.1/firmware.bin。Python49/Node37/C++169/3構成/EXE15項目成功。利用者が導入後確認にokと回答。[記録](docs/RELEASE_CANDIDATE_0_6_0_2026-10-21.md) |
| 完了 | README・構成スライド・開発記録の整理、GitHub push/merge、EXE公開 | 2026-10-21。[PR #7](https://github.com/thomasleftwite/stackchan-chatgpt/pull/7) main統合済み、[RC1 Release](https://github.com/thomasleftwite/stackchan-chatgpt/releases/tag/v0.6.0-rc.1)へEXE等7件公開、全SHA/サイズ一致。旧全文保管・秘密設定5パス保持。[公開記録](docs/GITHUB_RC1_SYNC_2026-10-21.md) |

## 継続検証

- [ ] ウェイクワードの見逃し・会議話者による誤検出を、環境条件と反復回数を定めて評価する。
- [ ] 長時間運用、物理的電源断、通信断・API制限、別Windows PCでの復帰と会話を検証する。
- [ ] 採用する各声の日本語品質を利用者がPCサンプルで評価し、不適合声を除外する。全候補の品質保証は未完了。
- [ ] RC1の継続使用結果を踏まえ、正式版への移行を判断する。

## 次段階: ホストからのAP設定（2026-10-25）

- [x] 仕様・実現性・問題点の整理（2026-10-25）。[HOST_AP_PROVISIONING_R01.md](docs/HOST_AP_PROVISIONING_R01.md)。端末WiFiManagerの設定受付と既存接続経路をソースで照合。端末変更なしでの実現は見込みであり、Windows API・無線切替の実証は未実施。
- [x] 有線PCでWindows API読取り、AP一時接続・復帰、設定送信とホットスポット参加、既存ホスト接続を基本実証（2026-10-25）。[実証R01](docs/HOST_AP_PROVISIONING_WINDOWS_PROBE_R01.md)。診断ツール2件、オフライン12件成功。20秒維持試行で参加0→1、利用者が既存ホスト開始後のPC Online / Sleepingを確認。直後離脱試行の失敗原因は未確定。
- [ ] 初期対応構成を確定する。現診断は有線上流・Wi-Fi1件が未接続のPC専用。上流Wi-Fi復帰と他構成は未実装・未検証。
- [x] 「スタックチャン接続」のホスト実装・オフライン試験（2026-10-25）。[実装R01](docs/HOST_AP_PROVISIONING_IMPLEMENTATION_R01.md)。有線PCを初期対象に、待受先行・AP設定・20秒維持・取消/復帰を追加。Python全77件、Node全38件成功。製品候補の実機確認とは区別する。
- [x] ホスト0.7.0-rc.1候補EXEのビルド・自己診断・レビュー（2026-10-25）。`releases/StackchanHost-0.7.0-rc.1-win-x64/StackchanHost.exe`。EXE自己診断16項目成功、ソース/ペイロード/EXE整合、既存秘密設定5パス同一、レビュー指摘修正済み。旧版保持、未公開。
- [ ] 旧ホストを停止・終了した後、候補EXEの新ボタンで初回設定・保存済み接続・停止/再実行を実機確認する。
- [ ] 製品ホスト組込み後のWi-Fiのみの実機接続・会話・再起動・復帰試験。今回の診断経由の接続基本確認と区別する。端末ファームウェア/AP設定/呼びかけ登録は保持し、必要な接続先Wi-Fi情報だけを従来と同じ経路で設定する。

## 将来の開発（未着手）

- [ ] **Raspberry Pi OS版ホストアプリ**：Wi-Fi/AP構成、常駐起動、声選択・試聴、音声出力、秘密情報の保存と配布方法を検討し、現行端末との会話を検証する。
- [ ] **macOS版ホストアプリ**：ネットワーク共有、起動・音声再生、秘密情報の保存、アプリ署名・配布を検討する。
- [ ] **ホストアプリ不要のESP32-S3直接Gemini対話システム**：端末からGeminiへ直接接続する構成を将来テーマとして検討する。認証とキー保護、TLS/WSS、メモリ、録音/再生、再接続、遅延を評価する。現行のPC中継版や旧OpenAI併用単体版とは別構成として設計する。
- [ ] 連続ストリーミング、会話履歴・複数ターンを検討する。
