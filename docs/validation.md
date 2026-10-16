# 検証記録

更新日: 2026-10-16（日本時間）。対象: 開発版0.1.0-dev、AtomS3R + Atomic Echo Base、Arduino/PlatformIO。

## 最新追記: 実機からGemini回答再生まで成功（2026-10-16）

COM4を新Gemini端末版R01/protocol v2へアプリ限定で更新、ハッシュ検証と起動成功。SPIFFS/NVS/保存済みWi-Fi/呼びかけ登録は書き換えない。TypeScript/PC25/25、C++19/19成功。APIなしの合成試験で可変長12,001サンプル/26フレームと実再生終了通知を確認した。

実マイクのGoogle送信は、自動承認レビューによる拒否後にユーザーの明示許可を得て実施。物理ボタンで3秒録音48,000サンプル/3,052ms/clip0、Gemini回答47,040サンプル/24kHz/98フレーム、実再生終了通知まで14,454msでPASS。ユーザーが「日本語の回答が聞こえた」と回答。今回Live1回、音声本文・資格情報の保存なし、課金/クラウド変更なし。

一時PCサービスは停止済み、端末は新Gemini端末版R01。再試験は `Start-Gemini-Device.cmd`、USBから接続先とランダムトークンをRAMだけへ設定する。最長約10分、1質問1回答の検証版。詳細、SHA、復帰、未検証範囲は [実機Gemini検証](GEMINI_DEVICE_VALIDATION_2026-10-16.md)。以下は各段階の履歴。

## ローカルPCのGemini Live実API検証（2026-10-16）

保存済みGeminiキーの検証とPC環境実装をユーザーが指示。`gemini-3.8-live` のHTTP200、テキスト入力からの24kHz音声応答（22,081サンプル/4チャンク/2,454ms）、生成試験音声16kHzを認証付きPC WebSocketへ送ってGeminiの24kHz回答を受け取る実API試験（入力13,440→出力78,000サンプル/163フレーム）に成功した。OpenAIキーは不要。

Node.js24.15.0、固定SDK2.30.0。新しいPCサービス `server/gemini-pc.ts` / protocol v2、1質問1回答、認証・世代/連番・入力/回答量・期限・next/playedを実装。SDK処理は期限付き子プロセスへ分離。TypeScriptビルド・自動テスト25/25成功、非公開.envからの単独起動とhealthz200を確認し停止。起動は `Start-Gemini-PC.cmd`、既定は127.0.0.1:8080でPC内限定。詳細と再現方法は [PC検証記録](GEMINI_PC_VALIDATION_2026-10-16.md)。

元の秘密設定を保持し、新.envには端末専用トークンとHOST/PORTだけを作成。ignore/未追跡を確認。音声・文字起こし・APIエラー本文・資格情報は記録しない。Live接続は計3回、課金設定変更なし、利用残量/料金は未確認。COM4はLAN診断R04/v1のまま、端末/設定領域への書込みなし。再生完了通知はPC上の模擬であり、実マイク→Gemini→端末再生は未検証。次は端末v2対応と家庭内LAN接続を実装する。以下は各段階の履歴。

## 再開時の状態

- GitHub mainは設計・セットアップ段階の `acc1d58b133a54e7e5a0ad5408235445124902f8`。取得元は `https://github.com/thomasleftwite/stackchan-chatgpt.git`。
- 手元には後続の実装ファイルがあったが、このフォルダ専用の.gitとホストテストがなく、検証記録も未完了だった。
- ファイルを保持して専用.gitを初期化し、main履歴を取得。`codex/resume-implementation` をmainのコミットから作成した。GitHubへのpush/mergeは実施していない。
- 対象フォルダの属性0x80410（Pinned=true、Unpinned=false、Offline=false）を確認。診断ファイル1件の作成・更新・10秒待機・改名・削除に成功、残存0件。

## PC上で確認済み

| 確認 | 結果 | 再現方法・範囲 |
|---|---|---|
| ファームウェアビルド | 成功 | `pio run -e atoms3r`。Arduino-ESP32 2.0.17、依存版はdependencies.md参照 |
| 静的な使用量 | RAM 56,232 / 327,680 bytes、Flash 1,910,213 / 3,145,728 bytes | ビルド報告値。実行時のTLS・JSON・PSRAM使用量を含まない |
| SPIFFSイメージ生成 | 成功 | `pio run -e atoms3r -t buildfs`。サイズ2,031,616 bytes |
| SPIFFS内容一覧 | 3ファイルのみ | /ca.pem、/config.yaml、/secrets.example.json。実キー/登録データを含まない |
| ホストテスト | 14 / 14成功 | `python tools/run_host_tests.py`。MSVC 19.50、C++17、/W4 /WX |
| 公開CAの構文 | 成功 | Python sslで121証明書を読込み。PRIVATE KEYマーカーなし。接続試験ではない |
| 秘密情報の除外 | 確認済み | secrets.example.jsonは空の値のみ。git check-ignoreで実キー・登録データ・.pioの除外を確認、git ls-filesに実キー/登録データなし |

ホストテストは実際のConversationRules.hをコンパイルして実行する。確認内容は、無発話タイムアウト、発話前の無音と発話上限の分離、最低発話時間、短い雑音の棄却、途中の再発話、DCオフセット、int16最大振幅、長さ0入力、リセット、24 kHzの標本数換算、登録ヘッダーの版数・符号付き長さ・次元・上下限・切詰め/余剰データ。ネットワーク、M5Unified、MFCC/DTW、実ファイル更新の挙動をホストテストしたという意味ではない。

## 再開後の修正

- ArduinoJson 7.4.2のコピー不可proxyを修正し、コンパイル可能にした。
- WebSocketsのWiFi依存探索と受信上限の上書きを修正。128 KiBの適用はstatic_assertで検証する。
- 未使用のSimpleVox VADをビルド対象外にし、MFCC/DTWとアプリの発話判定を利用する構成を明示。
- 設定APの一時パスワードは画面にだけ表示し、シリアルログには出さない。
- ホストテストで無音がmax_speech_msに含まれる不具合を再現して修正。録音全体の待機上限にもidle_timeout_msを反映した。
- 再生完了イベントが来ていても音声が残ったままタイムアウトした場合は失敗とする。
- 文字起こし確定は空でないitem_idの一致を必要とし、GeminiはHTTP 200、JSON MIME、STOP、空でない回答を確認する。

APIの形式は2026-10-15に[OpenAI文字起こし](https://developers.openai.com/api/docs/guides/realtime-transcription)、[Realtime会話](https://developers.openai.com/api/docs/guides/realtime-conversations)、[Gemini generateContent](https://ai.google.dev/api/generate-content)と照合。アカウントごとのモデル利用可否、実API応答は未確認。

## 生成物（初回ビルドの履歴）

案件ルート: `C:\Users\thoma\SynologyDrive\H\Doc\Src\Codex\projects\ai-stackchan\AI_StackChan_Ex\Stackchan-chatgpt`

以下は初回ビルド時の情報。現在のfirmware.binは末尾の音声調整版へ更新済み。SPIFFSイメージは変更していない。

- ファームウェア: `.pio/build/atoms3r/firmware.bin`、1,910,576 bytes、SHA256 `9A8ED6F76BBBAF7E718349965F7694EE08578F1A22C3002EB2A56B3003CEC1C1`。
- SPIFFS: `.pio/build/atoms3r/spiffs.bin`、2,031,616 bytes、SHA256 `8F1304F122703A23D346844F4834708F792B9E0AB5412C37F6B67557F862E741`。

ビルド時刻等により再生成時のハッシュは変わり得る。これらは書込み前の開発用生成物であり、実機検証済みのリリースではない。

## 未実施・次の確認

1. 独立したレビュー。今回の自己点検を第三者レビュー済みとして扱わない。
2. COM4接続、アプリ単体書込み、画面表示による起動確認は下記追加記録のとおり完了。
3. プロジェクト用パーティション/SPIFFSの導入、キー未設定起動、スマートフォンからの設定AP検出を確認済み。PSRAM全領域・マイク・スピーカーの基本動作は別途確認する。
4. WiFiManager初回設定・再起動・接続失敗・AP変更。時刻同期、CA検証、内部ヒープ/最大連続空き容量、PSRAMを実測する。
5. ローカル登録、破損/保存失敗時の旧データ保持、呼びかけ精度、待機音声が送信されないことを実機で確認する。
6. 明示承認後、OpenAI文字起こし → Gemini → Realtime読み上げを段階別に試験。モデル権限、内容保持、遅延、音切れ、切断/429/503、連続運転を確認する。

現同梱CA集合は大きく、TLSの実行時メモリ負荷が未測定。各ターン開始前に10分上限を確認する方式なので、進行中のターンによる時間超過がある。ウェイク精度、Realtime読み上げの逐語一致、応答遅延はいずれも保証しない。

## COM4実機書込み（2026-10-15追加）

- ユーザーがCOM4接続を指定し、「ファームウェア書込みを許可」と回答。SPIFFSを今回書き換えない範囲で実施した。
- OSでVID 0x303A / PID 0x1001を確認。esptoolでESP32-S3-PICO-1 rev0.2、Flash 8 MB、PSRAM 8 MB、40 MHz水晶を確認。PSRAMの実行時初期化や読書き試験が成功したという意味ではない。
- 書込み前に0x8000から3,072 bytesのパーティション表だけを読み取った。アプリ/ファイルシステム/NVSの内容は取得していない。チップMACや未加工シリアルログは保存・出力していない。

| 既存領域 | 開始位置 | サイズ |
|---|---|---|
| nvs | 0x9000 | 0x6000 |
| phy_init | 0xF000 | 0x1000 |
| factory（アプリ） | 0x10000 | 0x510000 |
| sys（data subtype 0x81） | 0x520000 | 0x100000 |
| vfs（data subtype 0x81） | 0x620000 | 0x1DF000 |

既存レイアウトは本プロジェクトのpartitions.csvと異なる。通常のpio uploadはパーティション表と0xE000の初期データも書き込み、既存NVSの範囲へ影響するため使用しなかった。

- `tools/flash_application_only.py` に対象イメージのSHA256を指定し、既存factory領域に収まることを確認して0x10000だけへ書込み。実際の消去範囲は0x10000～0x1E2FFF、書込み1,910,576 bytes、12.6秒、`Hash of data verified.`。再起動指示まで成功した。
- ブートローダー、パーティション表、NVS、sys、vfsは書込み対象にしていない。今回の内容は通常の導入完了ではなく、既存データを保持したアプリ単体書込みである。
- `tools/observe_boot_status.py` でCOM4を20秒監視。接続できたが、許可リスト内の状態行は0件。シリアル監視単独では起動成功と判定しなかった。
- その後ユーザーが画面の「設定領域なし」「Upload Filesystem」を確認。新アプリの実行と画面表示を確認した。この表示に到達する前のpsramFoundチェックも通過しているが、PSRAM全領域の読書き・負荷試験を行ったという意味ではない。
- 既存表にSPIFFS領域がないことと、`SPIFFS.begin(false)` の失敗表示は整合する。設定領域の未導入として扱う。
- マイク録音、音声再生、Wi-Fi設定、実API接続は未実施。

読取り表: `C:\Users\thoma\SynologyDrive\H\Doc\Src\Codex\projects\ai-stackchan\AI_StackChan_Ex\Stackchan-chatgpt\.pio\device-check\partitions-before.bin`（Git対象外、秘密情報領域を含まない）。補助手順はtools/inspect_device_partitions.py、tools/flash_application_only.py、tools/observe_boot_status.py。3件のPython構文確認は成功。

## 領域移行（2026-10-15実施）

対象はCOM4の同じAtomS3R。本プロジェクトのpartitions.csvと生成済みSPIFFSイメージを導入し、初期設定を読める状態にする。2026-10-15にファームウェア/SPIFFSのSHA256が上記生成物と同一であることを再確認した。

| 対象 | 位置・範囲 | 変更内容 |
|---|---|---|
| ブートローダー | 0x0000 | このビルドに付属するものを導入 |
| パーティション表 | 0x8000 | nvs、otadata、app0、app1、spiffs構成へ変更 |
| 新NVS | 0x9000～0xDFFF | 初期化。以前のWi-Fi等の設定は引き継がない |
| OTA初期情報 | 0xE000～0xFFFF | 初期データを書込み。旧NVS末尾とphy_initの配置を置き換える |
| アプリ本体 | 0x10000 | 上記検証済みファームウェア |
| SPIFFS | 0x610000～0x7FFFFF | config.yaml、公開CA、空のsecrets.example.jsonだけを導入 |

この移行では旧sys/vfsは利用できなくなり、vfsの内容はSPIFFS書込みで上書きされる。旧sysの一部が未消去で残る場合もあるため、旧データの完全消去・安全な廃棄を保証する手順ではない。旧設定・ファイルを保全したい場合は実施しない。秘密情報を含み得る領域のバックアップは作成していない。

APIキーは導入せず、会話APIは試験しない。初期化後はWi-Fi設定画面または待機画面への到達を確認する。前回の許可はアプリ本体のみだったため、領域移行について別途明示承認を受けて実施した。

### 実施結果

- 旧設定・保存ファイルを引き継がない旨を提示した確認に、ユーザーが「ok、デバイスをフラッシュモードにしました」と回答。COM4を再確認して実施した。
- `tools/migrate_device_layout.py` で書込み前に5イメージのサイズ/SHA256を照合し、新NVS（0x9000、0x5000 bytes）を初期化。消去成功を確認した。
- 同じ端末へ、ブートローダー（15,104 bytes）、領域表（3,072 bytes）、OTA初期情報（8,192 bytes）、アプリ（1,910,576 bytes）、SPIFFS（2,031,616 bytes）を書き込んだ。
- esptoolの `Hash of data verified.` が5件、補助手順の集計も5/5。書込みと検証に成功し、RTSによる再起動指示を送った。
- アプリ書込み12.6秒、SPIFFS書込み6.3秒。これは書込み所要時間であり、起動やAPIの応答性能ではない。
- OTA初期情報はArduino-ESP32付属boot_app0.bin。SHA256 `F94C5D786A7A8FAB06AC5D10E33BF37711A6697636DC037559EA19CC410A17F0`。他4件のハッシュは上記生成物・前回記録と同じ。
- 設定イメージには公開CA、config.yaml、空のsecrets.example.jsonだけを導入。実APIキーや登録済み呼びかけデータは含まない。
- 起動状態の限定シリアル監視で設定エラーとInvalid secrets.jsonを取得し、ユーザーも同じ画面を確認した。初回の領域書込み成功だけでは通常起動完了としなかった。下記の修正版で回復を確認した。
- 移行補助手順と監視補助手順のPython構文確認に成功。ファームウェア本体は前回ビルドから変更していない。

## キーなし初期起動の修正・回帰確認（2026-10-15）

### 再現と原因

- 初期SPIFFSは公開CA、config.yaml、空キー例だけで、secrets.jsonは存在しない。生成イメージ内にもsecrets.jsonの名前が含まれないことを確認した。
- `python tools/check_boot_config.py --port COM4` で修正前に `FAIL: Invalid secrets.json` を再現。画面エラーとAPが検出できないこともユーザーが確認した。
- 一時診断版でファイルの有無・種別・サイズだけを出力し、不在のsecrets.jsonが `open=1 dir=1 size=0` と返ることを実機で確認。ファイル内容は読取り・出力・保存していない。
- Arduino-ESP32 2.0.17のVFSはstatで見つからないパスをopendirでも試し、SPIFFSではこのディレクトリハンドルが有効として返っていた。従来の `if (!s)` だけでは不在を判定できず、空のディレクトリをJSONとして解析してエラーになっていた。SPIFFS自体が通常の階層ディレクトリを持たないことは[Espressif公式資料](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/storage/spiffs.html)も参照。

### 修正と確認範囲

- Config.cppで通常ファイルかどうかを判定。任意のsecrets.jsonは不在/ディレクトリならキー未設定として続行し、実ファイルのJSONが不正なら従来どおり停止する。必須のconfig.yamlもディレクトリを拒否する。
- LocalWake.cppの初回保存でもSPIFFS.existsだけに頼らず、旧テンプレートが通常ファイルかを確認するよう修正。実際の初回録音・再登録・保存失敗からの復旧は未検証。
- 修正版ビルド成功。静的RAM 56,232 bytes、Flash 1,910,309 bytes。firmware.binは1,910,672 bytes、SHA256 `FFCF5B42ED8F41F14E28A3E5DC20C7330EE1B890773AC83F853FF4C6196A50C6`。
- 既に導入した表のapp0（0x10000）へアプリだけを更新し、esptoolハッシュ検証成功。SPIFFS/NVSは追加初期化していない。
- 同じ実機・同じ初期SPIFFSで回帰判定が `PASS: configuration loaded; reached Wi-Fi/idle` へ変わった。これは設定読込みとWi-Fi開始処理到達の確認であり、家庭内Wi-Fi接続成功ではない。
- その後ユーザーがスマートフォンにStackchan-Setupが「表示される」と回答。設定APの検出まで確認できた。APへの接続とWi-Fi情報保存は未確認。
- 自動リセット後にログを取り逃す場合があったため、回帰補助手順には `--manual-reset` の待受モードを用意。必要時は監視を開いてから本体リセットを短押しする。ログ未取得は成功とせずINCONCLUSIVEを返す。
- 一時診断出力はsrc/include/toolsから除去済み。キー、パスワード、未加工のシリアル出力は保存していない。補助手順の構文確認と案件差分チェックは成功。

この障害はArduino/SPIFFSの実機上で再現・回帰確認した。PC上の発話判定14件は別の検証範囲であり、この不具合を検出したテストとして扱わない。今後はキー未設定の初期起動と、実ファイルに不正JSONがある場合の拒否を導入確認項目に含める。後者の実機試験は未実施。

現在の主要成果物: `C:\Users\thoma\SynologyDrive\H\Doc\Src\Codex\projects\ai-stackchan\AI_StackChan_Ex\Stackchan-chatgpt\.pio\build\atoms3r\firmware.bin`。導入・回帰補助手順: tools/migrate_device_layout.py、tools/flash_application_only.py、tools/check_boot_config.py。

## Wi-Fi設定後の待機確認（2026-10-16）

- ユーザーがWi-Fi設定手順を行った後、端末が「待機中」「APIキー未設定」になったと報告した。設定エラーを出さず待機ループへ進むことを確認した。
- ソース上は設定ポータルのタイムアウト後にも待機へ進むため、この2行だけで家庭内Wi-Fiの接続成功とは判定しない。「Wi-Fi未接続」「音声検出停止」の追加表示があるか確認中。
- PC上のdata/secrets.jsonは存在しないことだけを確認した。今回キーの作成、設定領域の再書込み、ネットワーク資格情報の読出しは行っていない。
- 次はキーなしで開始音・マイク・呼びかけ登録・保存を確認する。待機画面の「音声検出停止」がないことを確認してから、画面ボタンを2秒長押し→離す→開始音後に「スタックちゃん」と話す。「登録しました」と表示されるか、開始音が聞こえるかを記録する。
- 呼びかけ登録は現在の実装では端末内で完結する。録音音声やWi-Fi情報をチャットやログへ送らない。実際の登録成功、再起動後の保持、検出精度はこの時点では未検証。
- 家庭内Wi-Fiへの接続確認、再起動後の自動再接続、インターネット疎通、NTP/TLS、実API試験は別項目として記録する。

## ローカル呼びかけ登録の調査開始時点（2026-10-16）

- ユーザーが「登録タイムアウト」後に「呼びかけ未登録」へ戻ると報告した。
- 追加確認では開始音が聞こえず、「呼びかけを話してください」「スタックちゃん」の案内が表示されてから発話しているとのこと。発話タイミングの誤りを原因と決めつけない。
- 登録モード到達とタイムアウトを確認したが、マイク入力・開始音・保存の成功は確認できていない。キーは未設定のまま、会話API試験も行っていない。
- 仮説は音声ベース未装着/接続不良、コーデック/I2Sの初期化・録音再生切替、入力レベルと閾値の不一致。開始音も聞こえないため、音声ベースの装着状態を先に確認する。原因はこの時点で未確定。
- `tools/check_registration.py` を用意した。ユーザーが画面ボタンで開始した登録の状態行だけを監視し、登録成功はPASS、タイムアウト/音声初期化・保存の明示エラーはFAIL、結果を取得できなければINCONCLUSIVEを返す。音声やネットワーク資格情報は取得・出力しない。構文確認のみ成功、実機での本補助手順実行はまだ行っていない。
- Atomic Echo Baseの公式資料URLは現在Atomic Voice Base（SKU A149）のページへ転送される。実機の型式・装着状態はユーザー確認待ち。呼称の転送だけで実機型式を断定しない。[メーカー資料](https://docs.m5stack.com/en/atom/Atomic_Voice_Base)

## 音声入出力の切り分けと調整（2026-10-16）

目的: 開始音が聞こえず、案内表示後の発話が登録タイムアウトになる実機障害を調査する。対象はCOM4のAtomS3Rと、ユーザーが提示した[商品10193](https://www.switch-science.com/products/10193)のAtomic Voice Base（A149）。販売元は2026年4月にAtomic Echo Baseから改称したと記載している。[メーカー資料](https://docs.m5stack.com/en/atom/Atomic_Voice_Base)およびM5Unified 0.2.26の実装を参照し、既存ライブラリを維持した。

### 診断結果と原因の範囲

- 一時診断版でES8311のI2C ACK（0x18、SDA38/SCL39）を取得。実際のI2S設定はスピーカーBCK8/WS6/DOUT5、マイクBCK8/WS6/DIN7で、ライブラリのAtomic Echo Base設定と一致。ACKだけで録音再生品質全体の正常性は保証しない。
- 開始音100 ms版では再生要求が受理され約97 msで終了しても聞こえず、登録は成功と失敗が混在した。初回成功だけでは解消としなかった。DACの読取りACKとレジスタ09/12/13/31/32の値0/0/16/0/255、音量100を取得した。
- 開始音を1,200 Hz・500 msへ延ばした版では、ユーザーが開始音を聴取。実機での待機時間は約505～506 ms。この変更で聞こえた理由の内部機序は未確定であり、アンプ起動遅延などを実証したものではない。
- 増幅1倍・閾値500では発話のピークRMS約1,401、静音時約25～87。最初の発話で閾値を超えた区間は約140 msで、判定に必要な200 msに足りなかった。後の別発話を含む累積voiced_msは、一発話の判定時間とは区別する。
- マイク増幅のみ4倍へ変更し、閾値500と500 msの開始音を維持。ユーザーが同じ距離・声量で2回連続の「登録しました」を確認。2回目は監視も `PASS: local registration saved`。最初の結果はユーザー報告による確認で、監視待受の切れ目にあたり数値ログは取得していない。
- 2回目の増幅4倍ログは、静かな区間のピークRMS156、発話区間ピークRMS2,165、取得範囲min=-6,540/max=7,380、累積voiced_ms=320。観測範囲で16 bit上下限への到達はなかった。開始直後の1フレームはRMS680だったため、開始直後の過渡成分がないと断定しない。初回発話前の静音区間では登録完了しなかった。
- 登録成功ログは通常保存処理の正常終了を意味するが、電源断後の保持や破損時復旧、誤検出率の試験とは別。現時点の結果はこの端末・利用者・周囲条件での限定確認である。

### 通常版への反映と再現方法

- `src/AudioDevice.cpp`に増幅4倍と500 msの開始音を反映。開始音のキュー投入失敗・1秒以内に再生完了しない場合はfalseを返す。失敗注入は未実施。
- 一時I2C読取り、数値ログ、診断カウンター、`diagnose()`をsrc/includeから除去し、監視補助手順から診断専用正規表現を除去。`tools/check_registration.py`は状態行のみを扱う。音声データ、パスワード、キー、未加工のシリアル出力は保存していない。
- 通常版ビルド成功: 静的RAM56,232 bytes、Flash1,910,361 bytes。firmware.binは1,910,720 bytes、SHA256 `8F59313C8000835103C71E4427EEC15682B63DA04F9F3AEE83535C056F3B9042`。Python補助手順9件の構文確認と差分空白チェック成功。既存PCテスト14/14は発話判定ロジックの過去結果で、今回のハードウェア調整を検証するテストとして再分類しない。
- COM4へapp0（0x10000）のみを書込み成功。1,910,720 bytes、12.8秒、esptoolの `Hash of data verified.` を確認。今回NVSとSPIFFS、パーティション表、ブートローダーは更新していない。`migrate_device_layout.py`の照合値はこの通常版へ更新したが、領域移行処理自体は再実行していない。
- 通常版での回帰確認: 書込み完了後に `python tools/check_registration.py --port COM4 --seconds 180` で待受し、画面ボタンを約2秒長押し→離す→開始音が終わり案内表示後に同じ距離・声量で「スタックちゃん」と話す。ユーザーが「開始音が鳴り、登録しました」と回答。監視も `PASS: local registration saved`（終了コード0）を取得。診断版から計3回連続の登録成功となり、通常版での本障害の回帰確認は完了した。
- 主成果物: `C:\Users\thoma\SynologyDrive\H\Doc\Src\Codex\projects\ai-stackchan\AI_StackChan_Ex\Stackchan-chatgpt\src\AudioDevice.cpp`、`.pio\build\atoms3r\firmware.bin`、`tools\check_registration.py`。

### リセット後の保持・呼びかけ検出

- 通常版で本体リセットボタンを短押し1回し、待機後に画面ボタンを押さず「スタックちゃん」と話すよう依頼。ユーザーは「接続・API設定を確認してください」と表示されたと回答した。
- この操作条件とmain.cppの分岐から、登録特徴をリセット後に読込み、ローカル比較が成立して会話開始処理へ進んだことを基本確認した。APIキー未設定のガードで、音声のAPI送信処理へ進む前に待機へ戻る。この画面はWi-Fi未接続時も共通なので、Wi-Fi成功の証拠にはしない。
- 保持と呼びかけ反応はユーザーの画面観察による確認。独立したシリアル取得や電源ケーブル抜き差しでの保持試験は行っていない。雑音や他発話に対する精度を保証するものではない。

残る確認: 電源断後の登録保持、未登録の同等条件での初回保存、異なる距離・声量・雑音での検出/誤反応、24 kHzでの録音品質と増幅の適否、会話音声の再生品質、Wi-Fi保存/NTP/TLS/実API。増幅は会話の24 kHz入力にも共通適用されるが、実API試験は行っていない。登録済み特徴の保存先は端末内SPIFFSで、PCの配布用SPIFFSイメージに含めていない。

## ローカルAPI設定とGitHub公開前の確認（2026-10-16）

- ユーザーがGeminiキー設定と、実設定を除外した関連ファイルのpush・mergeを依頼。空の `data/secrets.json` を用意し、ユーザー自身がGeminiキーを保存した。値を表示せずJSON/キー形式とGemini設定あり・OpenAI設定なしを確認した。実APIの有効性・権限を検証したという意味ではない。
- 既存の `.gitignore` は `data/secrets.json` を除外済み。`data/secrets.json.*` も追加した。`git check-ignore` と `git ls-files` を別々に確認し、実設定が無視対象かつ未追跡であることを確認。空の `secrets.example.json` だけを公開する。端末向けSPIFFS再生成・書込みは行っていない。
- 公開予定のインデックス52ファイルを、ローカル設定の実キーとの一致と一般的なキー/秘密鍵パターンで検査し、一致0件。`.pio`、実設定、登録特徴が追跡対象に含まれず、サンプルの両キーが空であることも確認。これは既知パターンと当該キーに対する検査であり、一般的な秘密情報の完全な検出保証ではない。
- 同じソースを再ビルド成功（RAM56,232 bytes、Flash1,910,361 bytes）。ホストテストを再実行し14/14成功。補助Python9件の構文確認成功。自作ファイルのステージ差分空白チェック成功。SimpleVoxの同梱元由来の末尾空白は保持し、同ライブラリをチェック対象から除外した。今回ライブラリソースの空白修正は行っていない。
- 公開範囲はアプリ、依存関係、空キー例、公開CA、ビルド設定、ホストテスト、実機診断/書込み補助手順、README/TODO/検証記録。API通信は実機未検証の開発版として扱う。
- コミット `5524863ed822131d4410cdf2aa1b611166336683` を `codex/resume-implementation` へpushし、[PR #1](https://github.com/thomasleftwite/stackchan-chatgpt/pull/1)を作成。head SHA一致とmerge可能を確認後にmainへ統合し、GitHubのMERGED状態とmergeコミット `67a984ce78378b1901b8e2542aafd0bee91aebfe` を確認した。PRの自動チェックは0件で、上記はローカル検証結果である。
- 手元もmainへ同期し、ユーザーが保存した `data/secrets.json` が同期前とバイト一致し、引き続き無視対象・未追跡であることを照合した。実設定の値やそのハッシュは公開記録へ含めていない。

## 会話要約と次期構成の検討記録（2026-10-16）

- ユーザーの依頼により、同期済みmain `d06ecd573f24ca3e376bd8d3f603fae1cc54bd7b` を基準とした[再開用引継ぎR1](HANDOFF_2026-10-16.md)をローカル保存。既存の詳細証拠へ参照を付け、成果・未確認・秘密設定・次期方針・再開順を要約した。
- [Gemini Live構成の検討R01](GEMINI_LIVE_ARCHITECTURE_REVIEW.md)を作成。モデル、入力16 kHz/出力24 kHz PCM、SDK、終端通知等をGoogle・SDK等の一次資料で確認し、初期半二重と通信/キュー/検証計画を提案として区別した。旧実装方針とREADMEに案内を追加し、TODOへ完了日と未実施の実装段階を反映した。
- 文書のローカルリンク、Markdown空白/コードフェンス、音声サイズの計算を静的確認。作成・更新した文書にローカル実キーとの一致や既知キーパターンの検出がなく、秘密設定は前回保存値とバイト一致、ignored・未追跡のままであることを確認。文書検査はキー有効性やAPI動作の証明ではない。
- ソース・依存・設定データ・端末は変更していない。今回は文書だけの更新なのでファームウェア再ビルドや既存ホストテストを再実行していない。新構成の実装・依存インストール・サーバ起動・実API試験・機器書込みも未実施。
- 今回の記録はローカル作業ファイルとして残し、commit/push/mergeは行っていない。GitHub同期済みの旧実装と、ローカルに追加した次期設計検討を区別する。

## 再開後のクラウド方針と初期準備（2026-10-16）

- ユーザーの再開指示で作業ブランチ `codex/gemini-live-bridge` を作成。その後、ユーザーは稼働先をWindows PCではなくAI Studio上で作成・公開するクラウドサービスと指定し、実現可能性確認を依頼した。最新の [一次資料検討](AI_STUDIO_CLOUD_FEASIBILITY_2026-10-16.md) ではAI Studio Build → Cloud Run → Gemini Liveの構成として整理した。
- 資料からNode.js/npm/Secrets、Cloud Runサービス公開、WebSocket対応等を確認。AI Studio生成アプリで独自upgradeが公開経路を通ること、対象アカウントの公開資格、端末TLS、対象キーでの実API会話は未試験。プレビュー成功・資料確認・公開疎通・統合成功を区別する。
- 初期準備としてserverへ固定依存とlockfile、共通PCMフレーム処理を追加。Node.js24.15.0 / npm11.14.0。TypeScriptビルド成功、単体テスト2/2成功。既知のPCM値/byte順、切れたフレーム、奇数長、長さ不一致、上限超過の拒否を確認。最初に未実装モジュールを参照するテストで失敗を確認した後に実装した。
- これはフレーム構造の単体確認であり、世代・順序・creditの状態管理、認証、/live、Gemini中継、Cloud Run設定、端末クライアントは未実装。サーバ起動コマンドはまだ提供せず、READMEへ準備段階と記載した。
- npmの導入はインストールスクリプトを実行しない条件で、この案件のserver/node_modulesだけへ行った。SDKやCloud Runへの接続、Geminiキーの使用・アップロード、課金設定、端末書込みは行っていない。既存ESP32ソース・SPIFFS・NVS・ローカル秘密設定は保持した。

### Standardsレビュー

対象は `d06ecd573f24ca3e376bd8d3f603fae1cc54bd7b...cd385bd262bf0ee2d26ac68e61557258e6513063`、1コミット・15ファイル。専任レビュー担当によるStandards軸の指摘なし。一次資料へのリンク、設計案と実証結果の区別、未実装・未検証範囲、秘密設定の除外方針、ローカルTODOを確認。現段階を中継本体未実装の準備作業として扱い、公開・実API・実機統合の完了を主張していない。PCMモジュールに実害を伴う標準違反・smellは見つからなかった。レビュー中の外部サービス接続は未実施。

### Specレビュー

別担当によるSpec軸の指摘なし。同じ固定差分で、最新のクラウド希望の優先、AI Studio→Cloud Runと未検証範囲の明示、12 byteヘッダ・little-endian・sampleCountと長さ一致・flags=0・640/960 byte上限の整合を確認。状態管理・中継本体を実装済みと誤認させる完成判定はなく、秘密設定除外と既存ESP32保持に反する差分はない。実API・Cloud Run・端末操作は未実施。

指摘数はStandards 0件・Spec 0件、各軸とも最悪問題なし。この差分の静的レビューであり、旧ファームウェア全体の独立レビューや新構成の動作実証を完了したという意味ではない。

実装準備を `cd385bd` として作業ブランチへローカルコミット。GitHubへの追加push/mergeは行っていない。コミット内62ファイルに実キーとの一致なし、実設定は保存時からバイト一致、秘密設定・生成物の除外を再確認した。

## Starter向け認証付きechoの実装（2026-10-16）

- ユーザーの継続指示とStarter希望に基づき、Geminiを呼ばないechoを実装。`server.ts`はCloud RunのPORT/0.0.0.0へ対応し、SIGTERMで閉じる。`echo.ts`はupgrade前のBearer認証、1接続/インスタンス、30秒/無通信10秒、フレーム/滞留量上限を持つ。実トークンはまだ設定していない。
- まず未実装のechoを参照するテストで失敗を確認し、実装後に型検査と接続試験を実行。最後の全試験は11/11成功（PCM形式2件、echo/疎通9件）。内容はヘルス、未認証・誤認証拒否、既知PCMのbyte一致と正常close、不正/過大入力、接続容量の復帰、時間切れ、クエリURL拒否、弱いトークン拒否、smokeクライアントの正常/異常。ループバック上の試験であり、外部WSS/TLSの検証ではない。
- `smoke.ts` は公開先の既知合成PCM往復を試す準備。TLS検証は無効にせず、平文WSを許すのはloopbackだけ。URLクエリやログへトークンを載せない。echoの戻りは16kHz入力そのままであり、24kHz音声生成・変換ではない。
- Cloud Run用Dockerfileとファイル許可リストの.dockerignore、[公開準備手順](STARTER_ECHO_DEPLOYMENT.md)を作成。手元にDockerがないためコンテナビルド・公開は未検証。DockerのNode24タグは更新され得るため、実公開時に採用イメージ/ダイジェストを固定・記録する。
- 配布用ソース束 `.pio/publish/stackchan-echo-probe-r01.zip` は13ファイル、26,377 bytes、SHA256 `071BCDC7015CCB0BDA2E3C471F4CCE98C3BF2387E9FFEB6129A63BFA066A7F2E`。ZIP整合と実キー一致なし、.env/data/node_modules/distなしを確認した。AI StudioへのZIP直接インポート可否は未確認なので、公開済み成果物とは扱わない。
- AI Studio Buildで要件を送信してアプリ作成を試みたが、処理が進まずサービス側の `The caller does not have permission` を確認した。再読込み後にMy appsを開いても新規画面へ戻り、アプリ/公開URLの作成成功は確認できていない。未加工のブラウザログやアカウント情報はこの記録へ保存していない。
- 現在の停止点はアプリ作成権限。Starter対象外、Geminiキー不正、WebSocket非対応と断定する証拠ではない。ユーザーへ手動作成可否を確認中で、課金有効化や別アカウントへの切替、クラウドSecret登録、公開は行っていない。
- 既存の端末・ESP32ソース・SPIFFS/NVSとローカルGemini設定は変更していない。今回のGemini API試験、録音、機器書込みは未実施。

### echo差分のStandardsレビュー

対象は `a8d793b...2eb2b908dbb404f286a9b7b989fb556c6e14f269`。別担当からP2が1件: packageのNode>=22条件に対し、start/smokeの `--env-file-if-exists` は22.9.0以上が必要。package.json・lockfileルート・READMEを22.9.0へ揃え、限定再確認で解消、追加指摘なし。[Node公式CLI](https://nodejs.org/api/cli.html#--env-file-if-existsfile)。秘密情報・未検証範囲・Starter停止点について追加指摘なし。

### echo差分のSpecレビュー

別担当の指摘0件。同じ差分について、認証付きecho・ヘルス・既知PCM・上限・停止の実装と合格条件が整合し、16kHz echoを24kHz再生/Gemini会話と扱わず、ローカル/コンテナ/公開/実機の未確認範囲を区別していると確認。権限エラーをStarter資格不足やWebSocket非対応とは断定していない。

初回指摘数はStandards 1件（最悪P2、修正・限定確認済み）・Spec 0件（最悪問題なし）。実際のコンテナ/公開/Google APIを実行したレビューではない。

最低Node版を揃えたソース束R02は `.pio/publish/stackchan-echo-probe-r02.zip`、13ファイル、26,440 bytes、SHA256 `10B493E9AEA559018FF579B6DC7039F4AED08DC5598F27445404156B90CADA8F`。R01は履歴として保持。R02もZIP整合と実キー/秘密設定/生成物を含まないことを確認した。

その後ユーザーがAPI用「Stackchan-chatgpt」プロジェクトを作成。AI StudioのProjects一覧に名称と無料枠表示を確認したが、BuildアプリやStarter公開枠とは区別した。Build作成を再試行するとアプリの生成が始まった。先の権限エラーの原因がプロジェクト不在だったと断定する対照試験ではない。生成コード・Secret・公開条件は引き続き未確認。

### Buildアプリの実在確認とプレビュー検査

- Buildアプリ「Stackchan Echo Probe」の作成とCode/Previewを確認。初期生成版はNext.js/custom Nodeサーバであり、ローカルで試験した `server/` と同一ではない。初期ソースではready/PCM構造検証・送受信量制限・終了処理が不足し、プレビューにはヘルス404 HTMLと成功表示の混在があった。公開前の修正を依頼し、生成完了を待っている。
- 開発プレビューURLへの外部HTTP/WS試験は5秒期限、資格情報なし、合成音声も未送信で実施。ヘルス404・non-JSON、未認証WSS302。302はプレビューの認証/転送である可能性があり、公開サーバの未認証401を確認したことにはならない。WebSocket非対応とも断定しない。
- 公開案内にはアプリURLが公開される旨と無効のGet startedを確認した。生成中の観測であり恒久的な公開制限とは断定しない。Starter選択画面は未確認。公開・課金変更・Secret登録・Gemini呼出し・実機書込みなし。
- 初期の公開コードのみ `.pio/ai-studio/server-initial.ts` と `package-initial.json` に保存。秘密の可能性がある `.env.local` は開かず保存しない。ZIPダウンロードは完了を確認できていないので成果に数えない。

### Starter対象外の確定表示（同日・上記の後）

- 生成処理完了後はGet startedが有効になった。その先の公開設定「Confirm project and billing」に **You are not eligible for the Starter Tier.** を確認した。画面は `.pio/device-check/ai-studio-starter-ineligible-r01.jpg`。現在のアカウントでのStarter公開が停止点。先の一時的な権限エラーとは分ける。
- [公式ガイド](https://ai.google.dev/gemini-api/docs/aistudio-deploying#about-the-starter-tier)を再確認。Workspace、有料Cloud課金の現在/過去の利用、活動証拠不足が対象外となり得る条件。どれが該当するかは不明。標準公開には課金有効化が必要なため、課金なし希望のまま自動切替は行わない。
- 修正生成版326行を `.pio/ai-studio/server-generated-r02.ts` に保存。ready、PCMヘッダ検証、帯域制限、close期限などを確認したが、弱いトークン拒否・空PCM拒否・close中も接続枠維持などローカル版との差分が残る。ローカル11/11の結果はこの生成版へ転用しない。
- 修正後プレビューも実際のHTTP404を画面表示した。生成側の「end-to-end verified」報告は公開ホスト検証ではない。公開・Secret登録・課金設定・Gemini呼出し・端末変更は引き続きなし。

## ローカル通信・音声の段階検証（2026-10-16）

ユーザーがクラウド公開保留とローカル検証を選択。詳細な再現手順、版、依存、イメージSHA、復帰方法は [ローカル検証記録](LOCAL_AUDIO_VALIDATION_2026-10-16.md) を参照。

- PC内loopbackで16kHz/16bit/monoのPCM100フレームを20ms間隔で往復。計32,000サンプルのbyte一致・順序・正常closeを確認し、サーバ全試験12/12成功。端末LANやTLSの検証ではない。
- 診断版と通常復帰版のPlatformIOビルド成功。C++試験は補間の境界/最大振幅/周期確認を加え17/17成功。Python補助3ファイルの構文検査成功。型検査時の明示変換/型注釈不足を修正後に全試験を再実行した。
- COM4から設定領域を含まないパーティション表3,072Bだけを新しいファイルへ読み、ESP32-S3-PICO-1とapp0の開始0x10000・長さ0x300000を確認。診断版541,472Bをapp0のみへ書込み、esptoolのハッシュ検証成功。SPIFFS/NVSは書き換えない。
- 診断版起動のINFO応答を確認。短押し24kHz PCM確認音はTONE PASSとユーザー聴取を確認。初回の自声再生未確認に対し、監視窓では録音開始を確認できず、表示まで保持する長押し手順へ修正。再試行でユーザーが自分の声の再生を確認した。
- 成功操作の録音統計は55秒監視窓で得られなかったため、RMS・クリップ率・実所要時間は未評価。聞こえたという基本確認だけで品質/連続性/統合を合格にしない。現在COM4は音声診断版のまま、通常版へ戻すイメージは保存済み。
- ローカルGemini設定のバイト一致と除外を維持。音声データはPSRAM内だけ、再生後消去。録音ファイル・生ログ保存・API呼出し・クラウド変更なし。次のLAN条件をユーザーへ確認中。

## 実機LANの16kHz上り・24kHz下り（2026-10-16）

ユーザーが同じ家庭内LANへ接続可能と回答し、LAN専用診断を実装した。対象ボード/固定依存、SHA、全手順は [LAN検証記録](LAN_PCM_VALIDATION_2026-10-16.md) を参照。

- 通常echoは維持し、明示的pcm-loopbackモードを追加。150フレーム・640B入力/960B出力の補間、世代/連番・最大数を検査する。新規3件を加えサーバ型検査と全15/15成功。最初の試験で速度制限を越える無間隔送信になっていたため20ms間隔へ直して再試験した。フレーム形式/上限を緩めていない。
- LAN診断端末の最初のビルドではArduinoJson MemberProxy比較の非コピー制約に失敗。明示as変換を用い、ビルド成功を確認後にアプリ限定書込み。R01〜R04の各書込みでハッシュ検証成功、設定領域は保持。
- 初期LAN試験はWi-Fi接続とupgrade後に切断。定義済みエラー番号からWS_ERR_UNEXPECTED_RSV_2_3を特定。WebSocketsClient自身がCRLFを加えることを固定ソースで確認し、認証ヘッダに重ねていたCRLFを除去。その後合成150フレームの往復成功。ログへヘッダやトークンを出していない。
- 分割録音版でユーザーが自声のLAN往復を聴取したが、48,000サンプルの取得4,845msを確認。一括DMA録音へ変更した最終R04では3,054ms（RMS666、peak6106、clip0/48000）、自声150フレームの全データ一致、上り48,000/下り72,000、往復3,303ms、close1000。ユーザーが自然な再生を確認した。計測時間はクロックの校正値ではない。
- 最終合成試験も150フレーム照合成功（往復3,307ms、close1000）。PC private interfaceにbindしたサービスで未認証/誤認証401と正認証のPCM preflightを確認した。実機も同じサービスへ正認証接続。ファイアウォール/ルータ設定変更はしていない。
- `.pio/device-check/lan-probe-20261016-r06.json` へ許可済み状態・数値だけを保存し、最終試験後に一時サーバ停止を確認。音声ファイル・生ログ・トークン・Wi-Fi情報の保存なし。現在の端末はLAN診断版R04。ローカルGemini設定のバイト一致を再確認。
- 録音後送信のLAN診断の基本確認であり、連続/低遅延ストリーミング、credit/drain、回線断、長時間、TLS、Gemini API、クラウドは未実装/未検証。
- 監視補助のPython構文・private IPv4制限・任意認証行を除く状態フィルタを確認。保持済みの前回結果を今回の結果へ混ぜない処理と、記録の排他作成を追加。PC一時サーバの終了処理も短時間再起動/合成preflight後のstdin closeで終了コード0を確認した。端末や録音を再操作する追加試験ではない。
- 続行時にこの案件のlocal-lan.jsに該当するNodeプロセスの残存0件を確認。SynologyはPinned=true/Offline=false、ローカルGemini設定のバイト一致も再確認した。
