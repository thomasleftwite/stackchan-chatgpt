# 実機LANの16kHz入力・24kHz出力検証

版: R01 / 日付: 2026-10-16（日本時間）

目的: M5AtomS3R + Atomic Voice Base（A149）とWindows PCを同一家庭内LANで接続し、認証付きWebSocketとPCMの送受信を検証する。ユーザーが同一LANに接続可能と回答し、クラウド公開は引き続き保留した。

結果: 合成音とマイク音の双方で、16kHz/16bit/monoの48,000サンプルを送り、PCで変換した24kHzの72,000サンプルを150フレームすべて照合して再生した。最終版の録音時間3,054ms、PCM往復3,303ms、クリップ0/48,000。ユーザーが自声を「自然に聞こえる」と確認した。**録音後に送信する診断方式**であり、連続録音/低遅延会話、Gemini Live、TLS、クラウドを検証した結果ではない。

## 対象・依存・成果物

- COM4、ESP32-S3-PICO-1 rev0.2、Flash/PSRAM各8MB。Arduino-ESP32 2.0.17、espressif32 6.11.0、M5Unified0.2.26、M5GFX0.2.33、WebSockets2.7.3、ArduinoJson7.4.2。固定値はplatformio.ini、他依存はdocs/dependencies.mdを参照。
- `src/LocalLanProbe.cpp` と `AudioDevice.cpp` のみを `atoms3r-lan-probe` 環境でビルド。通常会話版のsrc/main.cpp/登録処理とは別。`STACKCHAN_LAN_PROBE` がない通常ビルドではこの診断ソースは空になる。
- `server/local-lan.ts` は一時PCサーバ。`echo.ts` に明示的なpcm-loopbackモードを追加。通常server.tsは従来のechoモードのまま。`probe-resample.ts` は診断専用の16→24kHz線形補間。PC/端末の補間結果を独立に計算してbyte一致を照合する。
- `tools/run_lan_probe.py` は一時サーバの起動、USBによるRAM設定、合成試験、許可リストの状態/数値のみの監視を行う。Node24.15.0、既存PlatformIO Python/pyserialを使用。

最終書込みイメージ: `.pio/firmware-snapshots/lan-probe-r04.bin`、1,134,384B、SHA256 `ED5F913B08EDBCB6109FCF8BDF74611A10D608B526403C1BFB75C805450A26F7`。ビルド成功、静的RAM50,268B/Flash1,134,025B、別途PSRAM音声バッファ240,000B。

0x10000のapp0だけを書込み、esptoolハッシュ検証成功。消去は0x10000〜0x124fff。NVS/SPIFFS/呼びかけ特徴は書き換えない。保存済みWi-Fiへは `WiFi.persistent(false)` で接続し、接続成功を確認した。APIキーを端末へ導入していない。

## 通信・メモリ・認証

1. PCのdefault gatewayがあるRFC1918 IPv4インターフェースを1件選び、そのアドレスの空きポートだけで待受。複数候補なら `--host` で指定。0.0.0.0や公開IPへはbindしない。CIM読取りが制限環境で拒否されたため、対象の読取り/実行だけを昇格して確認した。
2. 32B乱数を64文字hexトークンとしてプロセス内生成。PCサーバへの設定はstdin、端末への設定はUSBコマンドで送る。ソース、ログ、argv、設定ファイル、NVSへ保存しない。SSID/パスワード/接続アドレスも記録へ転記しない。
3. `/live` はupgrade前にBearer認証。未認証/誤トークン401拒否を実際にbindしたPC側経路で確認し、正しいトークンでPCM preflight成功。実機も同じホストへ認証付き接続成功。ファイアウォールの追加・緩和やルータ設定変更はしていない。
4. PCMは12B little-endianヘッダ（generation/sequence/sampleCount/flags）。上り640B=320サンプル、下り960B=480サンプル、flags0。pcm-loopbackではsequence0〜149の連続性、同じgeneration、入力長を検査。端末は下りの長さ・世代・連番・flagsと全サンプルを照合してから受信バッファで再生する。
5. 同時1接続、30秒、無通信10秒、最大メッセージ1,024B、入力65,536B/秒、出力滞留32,768B。端末も1,024B受信上限と1フレームずつの往復待ち。録音48,000/再生72,000サンプルの固定バッファを使い、終了/失敗時に停止・消去する。サーバはフレームごとに処理し、録音ファイルを作らない。
6. 今回はLAN内の平文 `ws`。クラウド用WSS/TLSや盗聴耐性を検証した結果ではない。マイク開始は端末の物理長押しのみ。USBから開始できるのは合成音試験だけ。

## 実測と不具合の切り分け

| 段階 | 結果 |
|---|---|
| PC内テスト | 型検査と15/15成功。従来echo/認証/時間/サイズに加え、150フレーム変換、短い入力・順序/世代不一致・超過拒否、補間境界を確認。private host/状態フィルタのPython検査も成功 |
| 初期LAN接続 | Wi-Fiとupgrade成功後、最初のPCM前に切断。サーバの定義済みエラー番号でWS_ERR_UNEXPECTED_RSV_2_3を特定 |
| ヘッダ修正 | setExtraHeadersへ渡す文字列の末尾CRLFを除去。ライブラリ自身もCRLFを加えるため二重改行がHTTP要求を早期終了し、後続User-AgentがWSフレームとして解釈されていた。受信検証を緩めず修正 |
| 分割録音版 | 自声のネット往復・再生は成功したが、48,000サンプル取得に4,845ms。RMS832、peak9,112、clip0。短い録音要求を繰り返す際のDMA末尾/待ち時間が影響した可能性があり、時間連続性は未合格とした |
| 一括録音版・合成音 | 150フレーム、上り48,000/下り72,000、照合成功、往復3,307ms、close1000 |
| 一括録音版・自声 | 48,000サンプル取得3,054ms、RMS666、peak6,106、clip0。150フレーム照合成功、往復3,303ms、close1000。ユーザーが自然な自声の再生を確認 |

修正後は `M5.Mic.record(input,48000,16000,false)` で1つのDMA宛先を要求し、完了を最大4.5秒待つ。短いフレーム要求を150回繰り返す方法との差を実測した。3,054msには開始/終了待ちも含むため、I2Sクロックの校正値ではない。診断用の補間は20msフレームごとで、境界の最後はクランプする。高品質リサンプラの検証ではない。

採用結果は `.pio/device-check/lan-probe-20261016-r06.json` の許可済み状態/数値のみ。R01〜R05の試行記録とR01〜R03のイメージも履歴として保持。音声本文や生ログは保存していない。サーバの切断通知は値1000を確認したが、JSONにはシリアル側の状態だけを保存する。

## 再現方法・現在の状態

1. `server` で `npm ci --ignore-scripts`（初回のみ）、`npm test`。固定依存はpackage-lock.jsonを保持。
2. `python -m platformio run -e atoms3r-lan-probe`。アプリ限定ツールでSHAと今回のパーティション表を指定して0x10000へ書く。通常のフルUploadやFilesystem Uploadで代用しない。
3. ユーザー端末の対話ターミナルから `python tools/run_lan_probe.py --port COM4 --seconds 580 --report .pio/device-check/<新しい名前>.json`。必要な場合だけ `--host <PCのLANアドレス>`。アドレス・トークンをチャットへ送らない。
4. 起動するとWi-Fi接続、認証/PCMのPC内preflight、合成音の実機往復と3秒の再生を実施。長押しで **Release to record** が出たら離し、開始音後に約3秒話す。録音→送信/照合→再生の順に動く。
5. 対話ターミナルで `stop` を入力して終了。最大約10分で自動停止する。Codexから対話を維持する場合はTTYを使う。今回は最終試験後に一時サーバ停止を確認済み。終了処理を再確認するため、端末を操作しない別の短時間起動で認証/PCM preflight→stdin close→正常終了コード0も確認した。OS常駐/自動起動は設定していない。

**端末にはLAN診断版R04を残している。** 通常会話・ウェイク待機はしない。PCツール停止中はLANテストの接続先も停止しているため、再試験にはツール再起動が必要。端末をリセットするとRAM上の接続設定・トークンは消える。終了後、この案件の一時サーバに該当するNodeプロセスの残存0件も確認した。

通常版への復帰イメージは `.pio/firmware-snapshots/normal-before-audio-r01.bin`。SHAとアプリ限定の復帰手順は [単体音声検証](LOCAL_AUDIO_VALIDATION_2026-10-16.md) を参照。ローカルGemini設定は保存時からバイト一致・ignored/未追跡を維持。

## 残る範囲

連続DMA録音と並行送信、24kHz再生キューとcredit/drain、回線断・再接続・世代切替、遅延/長時間/雑音別の評価、WSS/TLS、Gemini Live API接続は未実装/未検証。次は有界バッファと終端を備えた連続送受信を実装し、このLAN試験を基準に回帰確認する。クラウド課金/公開、実API呼出し、GitHub追加pushは行っていない。

一次情報: 固定WebSockets2.7.3のプロジェクト内 `WebSocketsClient.cpp` のsendHeader/setExtraHeaders、固定M5Unified0.2.26のMic_Class.inlを確認。[WebSockets公式ソース](https://github.com/Links2004/arduinoWebSockets/blob/2.7.3/src/WebSocketsClient.cpp)、[M5公式ボード資料](https://docs.m5stack.com/ja/atom/Atomic%20Echo%20Base)。
