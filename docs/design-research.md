# AtomS3R + Atomic Echo Base: チャットデバイス設計調査

調査日: 2026-10-15。実装・ビルド・実機試験は行っていない。以下の推奨は設計判断であり、動作保証ではない。

## 結論

端末側は **C++ + Arduino-ESP32 + PlatformIO** を第一候補とする。既存コード・WiFiManager・M5Unified を直接利用でき、音声バッファと通信処理のメモリ使用量を制御しやすい。MicroPython / Rust を選ぶと、要求された Arduino 用 WiFiManager の再利用に追加の接続層・移植が必要となるため、今回の条件では利点が小さい。[WiFiManager upstream](https://github.com/tzapu/WiFiManager)、[M5Stack 開発環境](https://docs.m5stack.com/en/core/AtomS3R)

ただし **Arduino-ESP32 自体は ESP-IDF を基盤にしている**。「ESP-IDF のプロジェクト・開発環境を使わず Arduino API 中心で開発する」なら適合するが、「内部依存まで禁止」ならこの案は適合しないため、条件の意味を確認する。[Espressif Arduino 公式](https://docs.espressif.com/projects/arduino-esp32/en/latest/)

## 手元の資料と再利用候補

- `firmware/platformio.ini:217`: AtomS3R 環境は `espressif32@6.11.0`、`framework=arduino`、`board=m5stack-atoms3`。OPI PSRAM を明示。M5Unified 0.2.7、M5GFX 0.2.27、ArduinoJson 7 系を指定している。これは現行設定の記録であり、最新版または今回の実機での検証済み版という意味ではない。
- `firmware/src/main.cpp:540`: `cfg.external_speaker.atomic_echo=true` で Echo Base を有効化済み。まずこの経路を再利用し、別の I2S ドライバを同時に初期化しない。
- `firmware/src/main.cpp:491`: 入出力初期化、`firmware/src/driver/Audio.cpp`: 録音・WAV ヘッダ生成が存在する。ただし旧来の WAV は 16 kHz 固定。新しい API の PCM 条件へ無検証で流用しない。
- `firmware/src/main.cpp:605` 付近: 現状の Wi-Fi は YAML 設定と独自設定ポータル。WiFiManager の追加だけではなく、Wi-Fi 接続管理を一本化する必要がある。
- `doc/atoms3r.md`: SPIFFS への YAML 転送手順。WiFiManager 導入後は Wi-Fi 設定の保存元との二重管理を避ける。
- `firmware/platformio.ini:29`: Realtime 用 WebSocket は公式 upstream ではなく大きなデータ向け fork。切り替える場合は受信フレーム長・分割受信・再接続を検証し、採用版を固定する。

## ハードウェアと音声

AtomS3R は 8 MB Flash、8 MB OPI PSRAM、2.4 GHz Wi-Fi を搭載する。PSRAM の容量だけで TLS / I2S DMA の余裕を保証できないため、内部ヒープと最大連続空き領域も測定する。公式資料には 2026-05-14 の LCD ドライバ GC9107 → ST7735 変更が記載されている。手元の個体と M5GFX 対応を確認し、古い版を新規購入機へ無条件に固定しない。[AtomS3R 仕様・改版履歴](https://docs.m5stack.com/en/core/AtomS3R)

Atomic Echo Base は ES8311、MEMS マイク、NS4150B アンプを使用し、メーカー README は全二重通信対応を明記している。ただし「全二重で入出力できる」と「スピーカー音を除去しながら自然に割り込める」は別の要件であり、AEC の動作をこの資料だけでは保証できない。[M5Atomic-EchoBase](https://github.com/m5stack/M5Atomic-EchoBase)

| MCU 視点の機能 | GPIO |
|---|---:|
| I2C SDA / SCL | 38 / 39 |
| I2S マイク入力 DIN | 7 |
| I2S スピーカー出力 DOUT | 5 |
| I2S LRCK / BCLK | 6 / 8 |

ピンは公式サンプルと M5Unified 0.2.7 で確認できる。Base 側の DIN / DOUT 表記と MCU 側の入出力の向きを混同しない。[公式 RecordPlay](https://github.com/m5stack/M5Atomic-EchoBase/blob/main/example/RecordPlay/RecordPlay.ino)、[M5Unified 0.2.7](https://github.com/m5stack/M5Unified/blob/0.2.7/src/M5Unified.cpp)

M5Unified 0.2.7 は Echo Base のマイク・スピーカーを同じ I2S ポートへ割り当てている。既存 `main.cpp` の通知音も `Mic.end()` / `Speaker.end()` で切り替える。初期版はボタンで録音し、再生中は録音しない半二重を推奨する。全二重を必須とする場合は、クロック共有、録音再生同時動作、AEC、発話割り込みを独立した実機検証項目とする。M5Atomic-EchoBase は低水準音声制御の代替候補であり、M5Unified の音声機能との二重所有を避ける。

## ライブラリ方針と確認項目

| 用途 | 第一候補 | 注意点・対策 |
|---|---|---|
| 表示・ボタン・音声 | M5Unified + M5GFX | まず既存版を比較基準とし、実機リビジョンを確認して組合せを固定 |
| Wi-Fi 設定 | tzapu/WiFiManager | ESP32-S3 対応。非ブロッキング動作、接続・ポータルのタイムアウト、ボタンからの再設定を検証 |
| HTTPS | Arduino-ESP32 の WiFiClientSecure / HTTPClient | 証明書検証・時刻同期・接続タイムアウトを設計 |
| WebSocket | arduinoWebSockets | 既存 fork と upstream の差分、最大フレーム長、断片化、受信中の音切れを評価 |
| JSON | ArduinoJson 7 | 音声 Base64 を含む大きい JSON の複製を減らし、チャンク単位で処理 |
| 小さな設定 | Arduino Preferences | Wi-Fi は WiFiManager/Arduino に任せ、API キー等の独自項目はアプリが保存を担当 |

WiFiManager には非ブロッキングモードとパスワード付き設定 AP がある。独自パラメータの永続化は利用側の責務。既存 WebServer とポートを奪い合わないよう、設定モードと会話モードのライフサイクルを分離する。AP の自動ポップアップが出ない場合のため、画面に設定先 IP を表示する。[WiFiManager README](https://github.com/tzapu/WiFiManager/blob/master/README.md)

WiFiManager と Arduino core には組合せ依存の不具合報告があるため、最新版追従ではなく、初回設定・誤パスワード・AP 消失・再起動・再設定の実機確認後に版を固定する。特定の過去 issue だけで全バージョンが非対応とは判断しない。[upstream issue #1797](https://github.com/tzapu/WiFiManager/issues/1797)

ユーザー回答を反映した構成と残る確認事項は、以下のAPI連携と会話開始設計に記す。上記PTTは初期ハード検証案であり、最終操作はボタンまたはウェイクワードで会話開始する。

## API連携と全体構成の検討（2026-10-15追記）

### 前提と暫定結論

ユーザー確認済み: OpenAI Realtime APIを音声認識と読み上げの両方に使用。Geminiが回答生成を担当。端末から直接APIへ接続し、ボタン押下またはウェイクワードで会話を開始する。Arduino IDEまたはVS Code/PlatformIOを利用可能。Arduino-ESP32で開発し、ESP-IDFプロジェクトを直接使わない構成を提案する。コード変更、ビルド、実機/API試験は実施していない。

端末の言語はC++、フレームワークはArduino-ESP32、環境は既存に合わせPlatformIOを第一候補とする。WiFiManagerとM5UnifiedのC++資産を直接使え、音声バッファの寿命とメモリ配置を管理しやすい。MicroPythonはこのライブラリ構成の直接再利用が難しく、Rustも追加の接続層と保守負担が生じるため本件の第一候補ではない。中継を認める場合は端末をC++のまま、TypeScript/Node.js等でAPI間の制御を担当させる選択肢がある。サーバー運用が増える代わりに鍵とAPI変更を端末から分離できる。

### 要求を満たす処理分担

マイク → OpenAI Realtime文字起こし → 確定テキスト → Geminiテキスト回答 → 音声生成 → スピーカー。

OpenAIは文字起こし専用セッションを提供する。調査時点の公式ガイドはgpt-live-transcribeを推奨し、24 kHz PCMの例を掲載している。このモデルではserver_vad/semantic_vadを利用せず、端末の発話検出またはボタンでcommitする。確定イベントとitem_idで発話を対応付ける。モデル名は実装直前に利用可能性を再確認し設定化する。[OpenAI Realtime transcription](https://developers.openai.com/api/docs/guides/realtime-transcription)

GeminiはgenerateContentで確定回答、streamGenerateContentでSSEストリーミングを取得する。会話履歴をcontentsに含める。最初は確定回答方式で制御を検証し、その後文単位の読み上げを検討する。今回GeminiLiveの音声対話をそのまま使用すると、音声担当をOpenAIに分離する構成にはならない。[Gemini生成API](https://ai.google.dev/api/generate-content)

読み上げまでRealtime必須なら、文字起こしセッションとは別のRealtime会話セッションでGemini回答をテキスト入力し、読み上げ指示で音声応答を生成する案。ただしこれは生成モデルへの指示であり、文字列の完全一致を保証するTTS仕様として扱わない。conversation:noneと明示入力で履歴混入を減らす設計は可能だが、改変・省略・拒否の評価が必要。Geminiだけを唯一の生成LLMにするという厳密な意味では、Realtime音声生成との条件整理が必要。[Realtime会話制御](https://developers.openai.com/api/docs/guides/realtime-conversations)

音声認識のみRealtime必須なら、Gemini回答を専用Speech APIに渡す構成を推奨。PCM出力で圧縮音声デコーダーを省ける。これはRealtime APIとは別のAPIであり、無断で要件を置き換えない。[OpenAI TTS](https://developers.openai.com/api/docs/guides/text-to-speech)

### 通信・データ用ライブラリ案

- arduinoWebSockets（Links2004）: WSS通信。既存はronron-ghのfix/max_data_size分岐を使用している。上流への単純置換はしない。公式READMEは受信上限と分割フレームのアプリ側再構成を明記している。比較後、バージョンまたはコミットを固定する。[上流README](https://github.com/Links2004/arduinoWebSockets)
- ArduinoJson 7: イベント/設定/応答の処理。JSON全体とBase64を何重にも複製せず、上限とPSRAM allocatorを設計する。[外部RAM対応](https://arduinojson.org/v7/how-to/use-external-ram-on-esp32/)
- Arduino-ESP32標準のWiFiClientSecure/HTTPClient: Gemini REST/HTTPS。SSEは行境界、HTTP chunk境界、UTF-8の途中分割を別々に扱う。
- Preferences: 小さい設定値。履歴が必要なら容量制限したファイル保存。NVSへの保存だけでAPIキーが暗号化されるとは扱わない。
- PCM再生ならESP8266Audioは必須ではない。アバターは任意。最小構成ではカメラ、サーボ、FTP、MCP、ローカルTTSは対象から外す提案。

### ローカルで確認した注意点

- firmware/platformio.ini: AtomS3R Arduino環境、PSRAM設定、M5Unified/M5GFX、ArduinoJson、WebSockets派生版がある。
- firmware/src/main.cpp: atomic_echo有効化済み。WiFi.beginによる既存接続をWiFiManagerに置き換える責務整理が必要。
- firmware/src/WebAPI.cpp: 既存Webサーバーは80番。WiFiManagerポータル停止後にアプリWebサーバーを開始する案。
- firmware/src/llm/RealtimeLLMBase.h: RT_REC_SAMPLE_RATE=16000。cppではこの値をM5.Mic.recordへ渡す。
- firmware/src/llm/ChatGPT/RealtimeChatGPT.cpp: セッション入力の申告は24000。確認範囲では変換なし。24 kHz録音へ揃えるか明示リサンプルする。実機障害を確認したという意味ではない。
- 同cppのinput_audio_buffer.committed処理: 録音停止→Mic.end→Speaker.beginを行う。既存音声会話はそのまま全二重化できる構造ではない。
- RealtimeLLMBase.cpp: 再生バッファ100 KiB×2をmallocしている。容量上限、割当失敗、PSRAM使用、再生完了までの所有権を見直す。

### 懸念と対応策

| 懸念 | 対応案 |
|---|---|
| 自分の発話の再認識・ハウリング | 初期はPTTまたは再生中録音停止。割込み会話は全二重I2SとAECを実機検証した後に追加 |
| 音声形式不一致 | 録音・申告・再生のrate/16bit/mono/符号/バイト順を明示。24 kHz PCMを基準候補にする |
| 内部RAM不足、断片化 | PSRAMに上限付きリングバッファ、DMA用領域はドライバー要件に従う。最大連続空き容量も測定 |
| 通信で再生が途切れる | 音声処理とネットワーク制御を分離。キューと事前バッファ、タイムアウトを設ける |
| 二社API連携の遅延 | 文字起こし確定→Gemini初文→音声先頭の各時間を計測。初期は短い回答、後に文単位ストリーミング |
| 再接続で重複発話 | 発話ID・応答ID・状態機械で旧イベントを破棄。切断時に再生/録音を安全停止 |
| TLSとAPIキー | 時刻同期とCA検証。ログへ秘密を出さない。端末保管の限界を認識し配布用途では中継を検討 |
| 課金・429・サービス障害 | 発話長/回答長/履歴上限、無音時停止、待ち時間付き再試行。再送による二重処理に注意 |
| API・モデル・依存の更新 | モデルを設定化し動作確認した依存を固定。previewモデルの既存固定値を無条件に再利用しない |
| WiFi再設定時の競合 | 設定モードと会話モードを分離。ポータルの期限、長押し復帰、資格情報消去手順を設計 |

24,000 sample/s × 2 byte × mono = 48,000 byte/s。Base64は約64,000 byte/sで、JSON/TLS分は別。5秒分の生PCMは240,000 byte。通信帯域よりTLS・JSONの瞬間メモリとバッファ寿命が問題になりやすい、という設計上の見立てであり実測ではない。

### 実装承認後に行う検証の順序

1. 製造ロットに合った表示、PSRAM、マイク録音とPCM再生。
2. WiFiManager初回設定、再起動、AP変更、接続失敗からの復帰。
3. OpenAI文字起こしのみ。日本語、固有名詞、無音、雑音、短い発話を評価。
4. Geminiテキスト会話のみ。履歴とエラー応答の扱いを確認。
5. 読み上げを接続し、内容保持、遅延、音切れ、連続運転時のメモリを計測。
6. WiFi切断、APIタイムアウト、長文、再接続を試験。その後ハンズフリーを検討。

現段階では設計調査だけであり、動作保証や性能値の確定は行っていない。

### 回答を反映した会話開始・ウェイクワード設計

ボタンは押している間だけ録音するPTTではなく、会話モード開始に用いる。AtomS3R画面は物理ボタンのため、静電容量タッチ操作とは区別する。待機→ボタン/ローカルウェイク検出→会話開始→発話終了検出→文字起こし確定→Gemini回答→Realtime音声化→再び聞く、という状態遷移を推奨。無発話タイムアウトまたはボタンで待機へ戻る。初期は再生中の録音を止め、ボタンによる再生中断を設ける案。

SimpleVoxはESP32-S3向けVAD/MFCC/DTWを提供し、既存資産を利用できるため初期検証候補。ただし本リポジトリのAtomS3Rビルドはウェイクワード未対応として無効化しており、フラグだけで動作すると扱わない。既存WakeWord.cppの16 kHz入力とRealtime用24 kHz入力を、待機/会話モード切替で使い分ける案。最初の発話の欠落を避けるため、開始音後に話すUIまたは短い先行録音を検討する。[SimpleVox公式](https://github.com/MechaUma/SimpleVox)

任意の日本語ウェイクワードを録音登録したいか、固定語でよいかによって選択が変わる。SimpleVox方式は話者・距離・雑音による誤検知/未検知を評価する。代替WakeNetはESP32-S3対応だが、Arduinoでの組込み、モデル領域、使用可能な語、カスタム語の提供方法を別途検証する必要がある。[WakeNet公式](https://docs.espressif.com/projects/esp-sr/en/latest/esp32/wake_word_engine/README.html)

音声認識・読み上げの両方にRealtimeを使う条件は確定したため、専用Speech API案は採用案ではなく条件変更時の参考に留める。推奨構成は文字起こし専用WSS + Gemini HTTPS + 読み上げ用Realtime WSS。接続数と内部RAMが厳しい場合は認識/読み上げ接続の交互利用を評価するが、TLS再接続による待ち時間を測る。単一Realtime会話セッションで入力文字起こしと自動応答抑制を併用する代替もあるが、モデル別の設定適合・自動応答抑制・会話混入防止を検証しない限り優先しない。

### 追加回答による決定事項

- 推奨方式はユーザー了承済み。C++ + Arduino-ESP32 + VS Code/PlatformIO、WiFiManager、端末からAPIへ直接接続する構成を採用方針とする。
- 割り込み・全二重対応は不要。再生中は録音およびウェイク検出を停止し、再生完了後に聞き取りへ戻る半二重方式とする。上記の割り込み/AEC検証とボタンによる再生中断は必須要件から外す。
- ボタンまたはウェイクワードで会話開始する要件は維持する。初期検証をボタンで行い、その後ウェイク検出を接続する。
- Gemini Live APIも必要であれば利用可能。基本構成はOpenAI Realtimeによる文字起こし・音声化とGeminiによる回答生成のまま。Gemini Liveへの許可だけをもって、OpenAIの音声処理要件を撤回したとは解釈しない。
- ハードウェア購入時期は2026年9月。購入時期からLCDドライバーや製造ロットを断定せず、実機確認で対応ライブラリを確定する。
- 推奨方式への了承は設計方針の了承として扱う。ユーザーの「まだコーディングには進まない」という指示を維持し、今回は文書のみ更新した。

### ウェイクワードの確定方針

ユーザーは呼びかけ検出までの端末内処理を必須とし、録音比較方式を許容した。初期の呼びかけ名は「スタックちゃん」。テキスト変更だけで検出対象を変える要件は、録音再登録で変更する方式に置き換える。待機中のクラウド文字起こし案は採用しない。

SimpleVoxのVAD/MFCC/DTWと既存の録音登録処理を再利用する方針。録音から抽出した特徴データと待機中の音声を端末内で比較する。AtomS3Rでの対応・精度は未検証であり、既存の無効化フラグを外すだけで動作するとは扱わない。[SimpleVox](https://github.com/MechaUma/SimpleVox)

設定案は `doc/chat-device-config.example.yaml`。`method: local_template` は提案する方式識別子で、既存ファームウェアの設定項目ではない。`phrase` は表示・登録案内用、`template_file` は録音登録から生成する特徴データの端末内パス。初期値 `/wakeword0.bin` は既存命名に合わせた案であり、登録済みデータを同梱したという意味ではない。設定反映は初期実装では再起動時とする案。

登録フロー案:
1. 初回は未登録を表示し、ボタンで会話開始できる状態にする。
2. 明示的な登録操作後に「スタックちゃん」を発声してもらい、端末内で特徴データを生成・保存する。登録音声はクラウドに送信しない。
3. 検出試験を行ってから有効化する。呼びかけ変更時は設定の表示名を変更し、新しい呼びかけを再登録する。
4. 再登録失敗時には既存の有効なデータを保持する。ファイル欠落・破損・不正な設定の場合はウェイク検出を停止し、ボタン操作は利用可能にする。

待機中の音声は端末内の比較にだけ使い、クラウドへ送らず、後からまとめて送ることもしない。ボタンまたはウェイク検出で会話モードへ入った後、開始通知を出してから会話音声を送る。呼びかけと質問を続けて話すと冒頭が欠ける可能性があるため、開始通知を待って話す操作を初期仕様とする。会話終了時には未送信音声を破棄し、再び端末内の検出へ戻る。再生中は録音・ウェイク検出を停止する。

精度検証では登録者の声、距離、声量、環境雑音、テレビ音声、別の話者を分けて評価し、誤起動と検出漏れのバランスで閾値を調整する。録音比較を話者認証として扱わない。複数サンプル登録が必要かは実測後に判断する。

現段階では設定案と設計メモのみ更新し、録音・モデル生成・設定読み込み・検出処理の実装は行っていない。
