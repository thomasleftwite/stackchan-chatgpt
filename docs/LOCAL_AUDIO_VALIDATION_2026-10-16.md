# ローカル通信・音声検証

版: R02 / 日付: 2026-10-16（日本時間）

目的: Starter公開を保留し、PC内WebSocketの連続PCM通信とAtomS3R単体の録音・再生を段階的に検証する。この記録は単体確認時点の履歴。その後の実機LAN試験・一括録音修正は [LAN検証記録](LAN_PCM_VALIDATION_2026-10-16.md) を現在地とする。TLS・Gemini Live会話は未検証。

対象: COM4のM5AtomS3R（ESP32-S3-PICO-1、Flash/PSRAM各8MB）+ Atomic Voice Base（A149）。フレームワークはArduino-ESP32 2.0.17、PlatformIO espressif32 6.11.0、M5Unified 0.2.26、M5GFX 0.2.33。その他の固定依存はplatformio.iniとdocs/dependencies.mdを参照。

## PC内WebSocket

`server/test/pcm-stream.test.ts` はloopbackの空きポートに検証済みechoを起動。認証トークンは乱数でプロセス内だけに生成し、保存・表示しない。1kHzの合成16kHz/16bit/mono PCMを640Bずつ20ms間隔で100フレーム送り、byte一致とsequence、合計32,000サンプル、正常closeを確認した。サーバ全試験12/12成功。LANの実機を相手にした結果ではない。

再現: `server` フォルダで `npm test`（Node.js22.9.0以上、今回24.15.0）。外部API、録音、ネットワーク設定変更は不要。

## 端末の音声診断版

`src/LocalAudioProbe.cpp` は `STACKCHAN_AUDIO_PROBE` がある場合のみ有効。`atoms3r-audio-probe` 環境でAudioDevice.cppとこのファイルだけをアプリソースとしてビルドする。通常版のsrc/main.cppや会話/登録処理とは別。SPIFFS/NVSの読書き、Wi-Fi接続、API呼出しは行わない。

- 短押し: 24kHz/16bit/mono、1kHzの合成PCMを1秒再生。通常版と同じ音量100。先頭/末尾に短いフェードを付ける。
- 長押し: **Release to recordの表示まで保持し、離す**。開始音の後、16kHz/16bit/monoで48,000サンプル（3秒相当）を320サンプルずつ録音。診断用の線形補間で24kHzの72,000サンプルへ変換し再生する。Geminiが返す音声を再生した結果ではない。
- 画面の時間表記は設定上の値であり、ハードウェアクロックの実測校正ではない。最初の案内「約2秒」では録音開始を確認できず、約3秒を目安に表示を待つ手順へ修正した。押下時間不足だった可能性はあるが断定しない。
- 録音はPSRAM内の96KB、出力は144KB。再生完了/失敗後、音声停止とバッファ消去を行う。音声ファイルの作成やシリアル送信はしない。
- PCの `tools/observe_audio_probe.py` は許可リストの成功/失敗・サンプル数・時間・音量統計だけを表示。音声内容や任意の生ログは表示・保存しない。録音開始のUSBコマンドは用意せず、物理操作のみ。

再現: `python -m platformio run -e atoms3r-audio-probe`。ビルド成功、RAM22,568B、Flash541,105B。PSRAMの音声バッファ240,000Bは実行時に確保する。

## 書込み・結果・復帰

1. COM4のパーティション表を `.pio/device-check/partitions-audio-probe-20261016.bin` に読み取り保存。app0は0x10000/0x300000、NVSは0x9000、SPIFFSは0x610000。以前の表は上書きせず保持した。
2. 診断版 `.pio/firmware-snapshots/audio-probe-r01.bin` は541,472B、SHA256 `2A8B339F63490B0DD54F36DF22082FC03EC90DB0C12A42ACF770B5EB550607C1`。`tools/flash_application_only.py --image <このイメージ> --partition-table <今回の表> --sha256 <上記値>` で0x10000のアプリだけを書込み、esptoolのハッシュ検証成功。消去範囲0x10000〜0x94fff。設定・登録領域への書込みなし。
3. USBのINFO応答で診断版起動とinput16000/output24000/mono/16bitを確認。短押し確認音は端末の処理完了とユーザーの聴取を確認した。
4. 初回はユーザーが「確認音は聞こえるが声は再生されない/不明瞭」と回答、監視窓ではTONE PASSのみ。表示までの長押し手順で再試行後、**自分の声が聞こえる**とユーザー確認。録音/再生の基本動作を確認したが、その操作の数値統計は55秒の監視窓では取得できず、録音時間・RMS・クリップ率は未評価。音質/距離/雑音の定量評価、連続反復は未実施。
5. 復帰用の通常版もビルド成功。`.pio/firmware-snapshots/normal-before-audio-r01.bin` は1,910,720B、SHA256 `6D11689010E6F42B14241BEBEF41A715A94A73A03DA2997F62970D39851BD9DC`。同じアプリ限定ツールと表で戻せる。単体試験後にLAN診断版へ更新しており、現在の状態は [LAN検証記録](LAN_PCM_VALIDATION_2026-10-16.md) を参照。通常会話・呼びかけ待機は行わない。

## ソフトウェア検証と次の段階

- C++実コードのホスト試験17/17成功。既存14件に、診断用補間のDC/出力境界、フルスケール、1秒1kHzの長さ/周期保持を追加。実機I2Sや音響の証拠とは分ける。
- Python補助3ファイルの構文検査成功。書込みツールはプロジェクト内.pioのイメージだけを受け付け、SHA/領域サイズを確認する。表取得ツールは出力済みファイルを上書きしない。既存表の上書き、.pio外イメージ、SHA不一致の拒否3件もシリアル操作なしで確認した。
- ローカルGemini設定のバイト一致・Git除外を維持。実API呼出し、クラウド公開、課金設定、録音保存なし。
- 次はPCと端末の同一LAN接続条件を確認し、認証付きWebSocketへ実機をつなぐ。16kHzアップリンクと24kHzダウンリンクを別々に確認し、PC内echo/端末単体の結果で統合済みと扱わない。

音声仕様の一次資料: [M5公式Atomic Voice Base](https://docs.m5stack.com/ja/atom/Atomic%20Echo%20Base)。ボードは16〜64kHzをサポートするが、本記録の合格範囲は上記の設定・基本動作に限る。
