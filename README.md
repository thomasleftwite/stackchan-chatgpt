# Stackchan 音声対話端末

AtomS3R + Atomic Voice Base A149を、Windowsホスト経由でGemini Liveへ接続する音声対話システムです。Windowsホストは正式版 **0.7.0**、端末は既存の **0.6.0-rc.1**を使用します。端末更新は不要です。端末とホストはWi-Fiで接続し、通常利用にUSB操作・監視は不要です。

[ホスト0.7.0の配布先](https://github.com/thomasleftwite/stackchan-chatgpt/releases/tag/v0.7.0) / [既存の端末ファームウェア（RC1）](https://github.com/thomasleftwite/stackchan-chatgpt/releases/tag/v0.6.0-rc.1)

## 起動と操作

1. PCをインターネットへ接続し、2.4GHzのモバイルホットスポットを有効にします。
2. Windowsホストを起動し、Gemini APIキー・採用済みの声を確認します。ホットスポットのアドレスが1件なら自動選択します。声はPCサンプル試聴後に採用でき、一覧へ即時追加・保存されます。
3. 端末を起動し、「スタックチャン接続」を押します。初回は既存操作で端末を設定APにし、対応する有線PC構成で自動設定します。端末のPC Online / Sleepingを確認し、短押しでReadyへ移行します。
4. 登録した呼びかけの後、ビープ終了から約0.7秒後に質問します。回答音声と口パクの後、待受へ戻ります。

初回Wi-Fi設定、3回の呼びかけ登録、声の試聴・採用は[Windows利用手順](desktop/README.ja.md)を参照してください。APIキーはホストだけで使用し、保存を選んだ場合はWindowsユーザー専用に暗号化します。

## 現行構成

端末がローカルで呼びかけを検出し、16kHz PCMの質問音声をWi-Fiでホストへ送ります。ホストがGemini Liveへ中継し、24kHz PCMの回答を端末で再生します。録音・再生は半二重、現在は1質問1回答です。

ホストはWi-Fi固定。端末画面は上段のモードと顔・接続状態を表示し、開発用レベルモニタは表示しません。回答はスピーカー有効化から200ms待って再生します。通常の通信はホットスポット内HTTP/WS、クラウド側はWSSです。

## 確認状況

ホスト0.7候補で、設定引継ぎ・保存済み接続・スマホなしのAP自動設定・手動質問の回答音声/口パク/待受復帰を利用者確認済み。正式版は同じ接続処理を使用します。初回自動設定は有線上流・未接続Wi-Fi1台・2.4GHz/WPA2を対象とし、他構成・長期精度・反復成功率は未検証です。

ホスト変更の検証はPython77件、Node38件、正式版EXE自己診断16項目成功。[0.7.0の配布・検証](docs/HOST_RELEASE_0_7_0_2026-10-25.md)を参照してください。端末を含む旧RC1の検証は[検証と未確認事項](docs/validation.md)に保持しています。

## 文書・開発

- [ホスト接続ボタンの実装・候補版での実機確認](docs/HOST_AP_PROVISIONING_IMPLEMENTATION_R01.md)
- [ホスト0.7.0正式版の配布・GitHub記録](docs/HOST_RELEASE_0_7_0_2026-10-25.md)
- [次段階: ホストからのAP設定・接続簡素化R01（仕様のみ・未実装）](docs/HOST_AP_PROVISIONING_R01.md)
- [Windows実証R01（診断経由の設定・接続を基本確認、製品ホストへの組込みは未実施）](docs/HOST_AP_PROVISIONING_WINDOWS_PROBE_R01.md)
- [構成スライド](docs/architecture-slide.pptx)
- [再開時の現在地](docs/RESUME.md) / [短い開発履歴](docs/DEVELOPMENT_HISTORY.md)
- [ToDo・将来構想](TODO.md) / [RC1成果物・導入記録](docs/RELEASE_CANDIDATE_0_6_0_2026-10-21.md)
- [通信と信頼の範囲](docs/HOTSPOT_TRUST.md) / [固定依存](docs/dependencies.md)
- [資料整理前の文書](docs/archive/2026-10-21/rc1-before-compaction/README.md)

端末はPlatformIOの`atoms3r-gemini-device`を使用します。ホストのソースは`desktop/`、中継は`server/`です。APIキー・Wi-Fi設定・登録特徴・個人設定・音声はGit対象外。端末更新は実機表とSHAを確認してアプリ領域だけを書き込み、NVS/SPIFFSを保持します。

旧0.1.0-devの端末単体OpenAI併用版は[保管した旧README](docs/archive/2026-10-21/rc1-before-compaction/README-before-compaction.md)を参照してください。将来のESP32-S3直結Gemini構想とは別の履歴です。
