# 実機マイクからGemini回答再生までの検証

版: R01 / 日付: 2026-10-16（日本時間）。対象: COM4のM5AtomS3R + Atomic Voice Base A149、Windows PC、Gemini Live gemini-3.8-live。

目的: 16,000Hz / signed16bit little-endian PCM monoの質問音声を端末→LAN WebSocket→PC→Geminiへ送り、返る24,000Hz PCM monoを端末で再生する。**実質問1回に成功し、ユーザーが日本語回答の聴取を確認した。** 連続会話・本番運用の完了ではない。

案件ルート: `C:\Users\thoma\SynologyDrive\H\Doc\Src\Codex\projects\ai-stackchan\AI_StackChan_Ex\Stackchan-chatgpt`

## 実装と依存

- `src/LocalGemini.cpp`、`include/LivePcm.h`、PlatformIO環境 `atoms3r-gemini-device`。既存の通常版・LAN診断版とは別ビルド。Arduino/PlatformIO、ESP-IDFへ移行しない。
- espressif32 6.11.0、Arduino-ESP32 2.0.17、M5Unified0.2.26、M5GFX0.2.33、WebSockets2.7.3、ArduinoJson7.4.2。他の固定依存はplatformio.ini。マイク/スピーカーの配線・音量・増幅は既存AudioDeviceの検証済み設定を使用。
- `server/local-gemini.ts` と `tools/run_gemini_device.py`。Node.js24.15.0、SDK@google/genai2.30.0、ws8.22.0、既存PlatformIO Python/pyserial。PCサーバは前段のprotocol v2実装を使用し、今回API処理自体は変更しない。
- `Start-Gemini-Device.cmd`: ビルドと一時サービス起動、USBによるRAM設定をまとめた起動用。ファームウェアの書込みはしない。COM4と既存PlatformIOのユーザー配下Pythonを使用。

新端末版R01のイメージ: `.pio/firmware-snapshots/gemini-device-r01.bin`、1,136,048 bytes、SHA256 `5A6898D75F3622A0BF01A7C23A046E4893D71E251904357ED415A4994364BF8B`。ビルド成功、静的RAM50,284B/Flash1,135,677B。PSRAMの固定音声バッファは入力96,000B、出力480,000B。送受信失敗・終了時は音声停止とバッファ消去を行う。

## 書込みとデータ保持

継続指示に基づき、`tools/flash_application_only.py` にイメージSHAと `.pio/device-check/partitions-audio-probe-20261016.bin` を指定し、COM4へapp0/0x10000だけを書き込んだ。消去範囲0x10000〜0x125fff、esptoolのハッシュ検証成功、通常起動とUSB状態応答を確認。ブートローダー・パーティション表・SPIFFS/NVS・既存Wi-Fi/呼びかけ登録は書き換えない。

PCの実キーは既存 `data/secrets.json` を読む。Googleキーは端末へ渡さず、OpenAIキーも不要。LAN接続先とランダムな64桁端末トークンはPCプロセスからUSBを通じてRAMへ渡し、NVSや設定ファイルに保存しない。値はログ/argv/この記録へ載せない。端末のリセット後は再度起動ツールを使う。

## 試験結果

| 段階 | 結果 |
|---|---|
| C++ホスト | 19/19成功。既存17件に、可変長最終フレーム、総量、世代、連番、長さ、flags、上限の検証2件を追加 |
| PCホスト | TypeScriptビルド、25/25成功。新runnerも型検査に含む。Python構文と状態出力の許可リスト検査も成功 |
| 起動用ファイル | Start-Gemini-Device.cmdを実行し、ビルド→一時サーバ→USB設定→Wi-Fi接続→質問待機を確認。stop入力で終了コード0、追加録音/API呼出しなし |
| 秘密設定 | 保存済みsecrets.jsonはチェックポイントとバイト一致。実設定/生成物はignoredかつ未追跡。公開候補101ファイルに実キー/保存済み端末トークンの一致0件。git diff --check成功 |
| 実機・APIなし | Wi-Fi接続成功、無認証/誤認証401、合成入力48,000サンプル→確認音12,001サンプル/26フレーム。最終フレーム1サンプルも形式検証し、再生終了通知、completeと切断までPASS。4,074ms |
| 実機・質問録音 | 48,000サンプル/16kHz、3,052ms、RMS1,765、peak12,048、clip0。開始音後、物理長押し/離す操作でのみマイクを開始 |
| Gemini実API | 音声入力48,000→24kHz回答47,040サンプル、98フレーム。API処理8,437ms（手動VADでの3秒入力送信も含む） |
| 回答の端末再生 | 送信開始から受信/再生完了14,454ms、played=1、completeと切断を確認。ユーザーが「日本語の回答が聞こえた」と回答 |

許可済み状態・数値だけを `.pio/device-check/gemini-device-fixture-r01.json` と `gemini-device-live-r01.json` に保存。音声本文・文字起こし・回答内容・生APIエラー・キー・接続先は保存しない。実マイク送信については自動承認レビューの拒否後、ユーザーから「質問音声のGemini送信を許可」の明示回答を得て起動した。サービスはボタン操作前に自動で録音/API送信を行わない。今回の実機試験によるLive接続は1回で、課金設定変更なし、利用枠残量・料金は未確認。

## 操作と再現

1. 同じ家庭内LANのPCへUSB/COM4で端末を接続する。現在端末は新Gemini端末版R01。Wi-Fi設定済みであること。
2. `Start-Gemini-Device.cmd` を開く。PCのdefault gatewayがあるRFC1918 IPv4を1件選び、そのアドレスの空きポートだけで待受し、USBからRAM設定を渡す。候補が複数なら手動コマンドの `--host` で選ぶ。アドレスやトークンをチャットへ送らない。
3. 画面のGemini PC readyを確認。短押しはローカル確認音のみ。質問は画面ボタンを **Release to recordが出るまで長押し→離す→開始音後に約3秒で発話**する。個人情報を含まない内容で試す。
4. 録音後にPCへ送り、Gemini回答を全受信してから再生する。通信/再生中の短押しは中止。失敗時の自動再送なし。
5. PCの画面で `stop` と入力して停止。最長約10分で自動停止する一時検証用で、常駐Windowsサービスではない。再試験は起動用ファイルを再実行する。端末リセット時も再実行が必要。

手動ではserverフォルダで `npm run build` 後、案件ルートで次を使用する。既存PlatformIO Pythonを使用する。

```text
python tools/run_gemini_device.py --port COM4 --mode fixture
python tools/run_gemini_device.py --port COM4 --mode gemini
```

fixtureだけはマイクを使わない合成試験を自動実行する。geminiは物理ボタンを待つ。LANの平文WSに限った試験で、ファイアウォールやルータ設定は変更していない。従来の `Start-Gemini-PC.cmd` はPC内限定のサーバ起動で、USB設定を行わないため実機には今回の起動用ファイルを使う。

## 現在の状態と未検証

実機試験後の一時PCサービスは停止済み。端末はGemini端末版R01を保持し、通常版の呼びかけ待機は行わない。通常版への復帰は `.pio/firmware-snapshots/normal-before-audio-r01.bin`、LAN診断へは `lan-probe-r04.bin` をアプリ限定ツールで書く。SHAと復帰手順は [単体音声記録](LOCAL_AUDIO_VALIDATION_2026-10-16.md)・[LAN記録](LAN_PCM_VALIDATION_2026-10-16.md)にある。Filesystem UploadやフルUploadで代用しない。

現版は3秒の一括録音、1質問1回答、最大10秒の回答を固定上限で受ける半二重方式。全受信後に実再生し、M5.Speaker.isPlayingが終わった後にplayedを送る。連続ストリーミング、会話履歴、ウェイクとの統合、AEC/割込み、可変の発話長、WSS/TLS、クラウド、長時間安定性、雑音/距離/認識率・音質の定量評価は未検証。今回の1回成功をリリース品質の証拠とは扱わない。
