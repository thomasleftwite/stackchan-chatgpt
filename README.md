# Stackchan-chatgpt

開発版 `0.1.0-dev`。2026-10-16までにファームウェア/SPIFFSビルドとホストテスト14件、実機の起動・画面表示、設定領域導入、キー未設定での起動、スマートフォンからの設定AP検出を確認。音声調整後の通常版で開始音・呼びかけ登録、短押しリセット後の登録保持・ボタンなし呼びかけ反応も確認済みです。家庭内Wi-Fiの接続保存、音声の録音再生品質、実API、呼びかけ精度の定量評価は未確認です。再開位置は [TODO](TODO.md) と [検証記録](docs/validation.md) に記載しています。

AtomS3R + Atomic Voice Base（旧称Atomic Echo Base、SKU A149）用の音声チャット端末です。
C++ / Arduino-ESP32 / PlatformIOで開発し、ESP-IDFプロジェクトは使用しません。

- Wi-Fi: WiFiManager
- 待機: 端末内の録音比較（SimpleVox MFCC/DTW）またはボタン
- 会話: OpenAI Realtime文字起こし → Gemini回答生成 → OpenAI Realtime読み上げ
- 録音・再生は切替式。全二重・発話割り込みは対象外です。

実機・実APIの検証状況は [検証記録](docs/validation.md) を参照してください。

## セットアップ

1. VS Codeでこのフォルダを開き、PlatformIO拡張をインストールします。
2. `data/secrets.example.json` を `data/secrets.json` にコピーし、OpenAIとGeminiのAPIキーを入力します。キーのファイルはGit管理対象外です。
3. `data/config.yaml` でモデル、音量、呼びかけ名などを設定します。モデルは各アカウントで利用可能なものを指定してください。
4. PlatformIOの `atoms3r` 環境をBuildし、USBでファームウェアをUploadします。
5. 同じ環境の `Upload Filesystem Image` を実行します。ファイルシステムの書き込みは既存の設定・登録音声特徴を置き換えます。既に登録済みの場合は再登録が必要です。
6. 初回起動時の画面に出る `Stackchan-Setup` のパスワードでスマートフォン等を接続し、Wi-Fiを設定します。画面が自動表示されなければ `http://192.168.4.1` を開きます。

### APIキーの設定先

ローカルの `data/secrets.json` に設定します。`gemini_api_key` はGemini回答生成、`openai_api_key` は音声の文字起こしと読み上げに使用し、会話開始には両方が必要です。キーはチャット、README、config.yaml、サンプルファイルへ記載しないでください。

`data/secrets.json` とそのバックアップ（`data/secrets.json.*`）は `.gitignore` で除外します。GitHubには空の `data/secrets.example.json` だけを含め、キーを含むSPIFFSイメージや `.pio` 内の生成物も公開しません。無視対象かどうかと、既に追跡されていないかは別々に確認します。

PC上で保存しただけでは端末へ反映されません。端末は起動時にSPIFFS内の `/secrets.json` を読みます。Filesystemの再書込みは現在の呼びかけ登録も置き換えるため、端末への導入は登録保持方針を確認して行ってください。

CLIでは `pio run`、`pio run -t upload`、`pio run -t uploadfs`、`pio device monitor` を使用します。
Uploadに失敗する場合はAtomS3Rをリセットボタン長押しでダウンロードモードにし、USBポートを確認してください。

## 操作

- 短押し: 会話開始。接続完了後の開始音を待ってから話します。
- 待機中に2秒長押し: 呼びかけの録音登録。ボタンを離し、開始音が終わって「呼びかけを話してください」と表示されてから「スタックちゃん」と話します。登録は端末内で行います。
- 登録後: 登録した呼びかけで会話を開始できます。開始音を待ってから質問してください。
- 会話中: 発話が止まると回答します。回答終了後に再び開始音が鳴り、次の質問を受け付けます。無発話が続くと待機へ戻ります。
- 登録中に短押し: 登録をキャンセルします。
- 画面ボタンを押しながら起動: Wi-Fi再設定。リセットボタンとは別のボタンです。

音声検出は話者認証ではありません。別の人やテレビの音声に反応する場合があります。

## 設定変更

`wake_word.phrase` は表示名です。名前の変更だけでは検出対象は変わりません。
呼びかけ変更時は設定を書き換えた後、新しい呼びかけを録音登録します。
変更した設定はファイルシステムへ再書き込みし、再起動すると反映されます。

`distance_threshold` を小さくすると判定が厳しくなります。シリアルログの `Wake distance` と実際の検出結果を見て調整してください。
無音でも反応する場合は `audio.energy_threshold` を上げ、声が検出されない場合は下げます。

Atomic Voice Baseではマイク増幅を4倍、開始音を1,200 Hz・0.5秒にしています。増幅は登録用16 kHzと会話用24 kHzで共通ですが、24 kHzの実API音声品質は未検証です。増幅変更前の呼びかけ特徴を使用している場合は再登録してください。

## 通信と制約

待機中・登録中の音声は外部送信しません。ボタン/ウェイク検出後の会話モードでのみ音声を送信し、待機時の録音は破棄します。会話中の聞き取りでは無音区間もAPIへ送られます。
APIキーは端末のSPIFFSに平文で保存されます。第三者へ端末やファイルシステムイメージを渡す前にキーを削除してください。
TLS証明書検証を必須とし、初回接続前に時刻同期します。`data/ca.pem` はMozillaの公開CA集合です。

Realtimeでの読み上げは生成モデルによるもので、Gemini回答の逐語一致を保証しません。
接続は段階ごとに切り替えるため、音声応答まで待ち時間があります。初期版はGemini回答全体を受信してから音声化します。
会話履歴はRAM上で最大4往復、会話終了時に破棄します。各ターンの開始前に10分の会話上限を確認します。進行中の通信・再生は個別のタイムアウトまで続くため、終了時刻が10分を超える場合があります。

APIエラー・切断時はその会話を終了し、待機へ戻ります。同じ発話を自動再送しません。
カメラ、サーボ、MCP、OTA更新、Gemini Liveへの自動切替は含みません。

## 資料

- [実装方針](docs/implementation-plan.md)
- [設計調査](docs/design-research.md)
- [依存ライブラリ](docs/dependencies.md)
- [検証記録](docs/validation.md)

## 開発時の検証

`pio run -e atoms3r` でファームウェア、`pio run -e atoms3r -t buildfs` でSPIFFSイメージを生成します。いずれも実機へ書き込みません。

`python tools/run_host_tests.py` で実際のC++発話判定・登録ファイルヘッダー検証をPC上で実行します。C++17対応のGCC/Clang、またはVisual Studio C++ Build Toolsが必要です。Windowsではインストール済みのMSVCを自動検出します。

ビルド時に `tools/prepare_websockets.py` がこの案件の `.pio/libdeps` 内にあるWebSockets 2.7.3のESP32受信上限定義へガードを追加します。設定した128 KiBが上書きされる問題への対処で、想定外のソースではビルドを停止します。詳細は [依存関係](docs/dependencies.md) を参照してください。
