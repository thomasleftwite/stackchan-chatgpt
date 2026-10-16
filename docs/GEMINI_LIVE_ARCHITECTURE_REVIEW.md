# Gemini Live 中継構成の検討

- 版: R01 / 調査日: 2026-10-16（日本時間）
- 状態: 設計検討。実装・依存インストール・サーバ起動・実API接続・端末書込みは未実施。
- 目的: AtomS3R + Atomic Echo / Voice Base の会話を、Node.js 中継経由の Gemini Live 音声入出力へ移す判断材料を残す。
- 現行基準: main `d06ecd573f24ca3e376bd8d3f603fae1cc54bd7b`、PR #1 merge 後。基準と実機履歴は [引継ぎ記録](HANDOFF_2026-10-16.md)・[実機検証](validation.md)を参照。
- 本書の「確認した仕様」は一次資料、「提案」は今回の設計案。「実機で動作した」とは扱わない。

追記: ユーザーは稼働先をAI Studio上で作成・公開するクラウドサービスと指定した。実行基盤はCloud Runを想定し、端末URLを `wss://<公開サービスのホスト>/live` とする。[クラウド方式の実現可能性](AI_STUDIO_CLOUD_FEASIBILITY_2026-10-16.md)を優先する。以下のLAN/PC案は通信仕様検討時の初期案として残す。外部サービスになるため端末認証とTLSを初期実装に含める。

## 判断

希望構成は採用候補にできる。入力を **16,000 Hz / signed 16-bit / little-endian / mono の raw PCM**、出力を **24,000 Hz / 同形式**に統一すれば、通信のためのアプリ独自のリサンプリングを挟まずに構成できる。M5ドライバ内部の変換やI2Sクロック設定は別途確認する。公式の Live API 概要も入力16kHz・出力24kHzを規定し、バックエンド中継方式を説明している。[Google Live API 概要](https://ai.google.dev/gemini-api/docs/live-api)

新構成では音声認識・応答生成・音声生成を Gemini Live にまとめる設計なので、**OpenAI APIキーは不要**。Googleキーは中継サーバの環境変数だけから読む。既存 `data/secrets.json` の値を端末へ導入する作業は、新構成のためには不要になる。これは移行後の設計判断であり、現行ファームウェアのキー要件が変わったわけではない。

初版は **半二重**を推奨する。ローカル呼びかけで会話を開始し、聞く間だけ上り音声を送り、返答の再生が完全に終わるまでマイクを止める。割込み会話・AEC・複数端末への拡張は別段階とする。

```text
AtomS3R + Atomic Echo / Voice Base
  │ LAN WebSocket: 制御JSON + binary PCM
  ▼
Node.js 22 LTS / Express + ws / server.ts
  │ @google/genai ai.live.connect() / Google向けWSS
  ▼
Gemini Live API: gemini-3.8-live
```

サーバが3000番ポートで待つなら端末の接続先は `ws://<server-ip>:3000/live`。希望表記 `ws://<server-ip>/live` は80番ポートを意味する。WebSocket URLの既定ポートは [RFC 6455 §3](https://www.rfc-editor.org/rfc/rfc6455#section-3) による。

ExpressのHTTP処理だけでWebSocket受信を実装する構成にはしない。同じHTTPサーバへ `ws.WebSocketServer` を接続し、`/live` の upgrade を扱う案とする。HTTPサーバ共有・パス振分けは [ws公式例](https://github.com/websockets/ws#multiple-servers-sharing-a-single-https-server) に沿う。サーバ稼働先はPC・Raspberry Piなど未決定で、実装時に決める。

## 確認した仕様

| 項目 | 一次資料で確認したこと | 設計への反映 |
| --- | --- | --- |
| モデル | `gemini-3.8-live` はstable、最新更新は2026年9月。移行注意では音声応答を選択し、文字が必要なら出力文字起こしを使う | `responseModalities: [Modality.AUDIO]`。キーの利用資格・クォータは未試験 |
| 3.8の設定 | `thinkingLevel` / `thinkingConfig` を指定しない。proactive audioは常時有効でfalseはエラー。affective dialogue設定は削除 | 古いLiveサンプルの設定をコピーしない。無応答も許容して期限で復帰 |
| SDK | `@google/genai` に `ai.live.connect()` がある。現在のmain実装はsetupCompleteを待ってconnectのPromiseを完了する | onopenだけで録音開始しない。固定版の接続完了仕様を実装時に再確認 |
| 音声送信 | `sendRealtimeInput({audio:{data: base64, mimeType:'audio/pcm;rate=16000'}})` | LANのPCMを中継でbase64化。WAVヘッダを付けない |
| 音声受信 | `serverContent.modelTurn.parts[]` の `inlineData.data` に音声。1イベントに複数partがあり得る | 全partを調べ、MIME確認後base64を復号し端末へ送る |
| 自動VAD | デフォルトで有効。停止した入力を `audioStreamEnd: true` で終え、次のaudio送信で再開できる | 初版は自動VADに統一。手動activityStart/Endを混ぜない |

モデルの根拠: [Gemini 3.8 Live モデル仕様・移行注意](https://ai.google.dev/gemini-api/docs/models/gemini-3.8-live)。音声・SDK呼出し・VADの根拠: [Live capabilities](https://ai.google.dev/gemini-api/docs/live-api/capabilities)。接続完了の根拠: [公式SDK live.ts](https://github.com/googleapis/js-genai/blob/main/src/live.ts)、入力型: [LiveSendRealtimeInputParameters](https://googleapis.github.io/js-genai/release_docs/interfaces/types.LiveSendRealtimeInputParameters.html)。

標準の3.8 Liveでは `turnComplete` がターン終了の基準。Extended Thinkingでは `interactionStatus` が別途必要で、同じ判定を流用できない。[Thinkingガイドのモデル比較](https://ai.google.dev/gemini-api/docs/live-api/thinking)

生成音声は実時間より速く届き得る。`generationComplete`、`turnComplete`、`interrupted` はそれぞれ別の通知で、端末スピーカーの再生完了を直接測ったものではない。[SDK LiveServerContent](https://googleapis.github.io/js-genai/release_docs/interfaces/types.LiveServerContent.html) 初版ではGoogle向けの追加の再生状態報告機能を使わず、独自の端末drainを管理する。

公式ページ間には時点差がある。capabilities冒頭にはpreview表記が残るが、モデル固有ページは3.8をstableとする。SDK Sessionの古いコメントに `media` しかない場合でも、現行入力型には `audio` / `audioStreamEnd` が存在する。**モデル固有ページ、現行ガイド、採用する固定SDKの型・実装**を照合し、SDKのmainを固定リリースの保証として扱わない。

## 現行コードとの関係と変更対象（提案）

現行 `src/AudioDevice.cpp` は `listen(rate)` を持ち、`playRaw(...,24000,false,1,0)` で24kHz mono出力を要求する。`stop()` はマイク・スピーカー双方を終了する。増幅4倍と開始音0.5秒、呼びかけ登録・保持の実機結果は `validation.md` の範囲。**これだけでは連続24kHz再生、音切れ、ネットワークとの同時動作は未検証**。

| 対象 | 移行時の変更案 |
| --- | --- |
| `src/CloudConversation.cpp` / `include/CloudConversation.h` | 現行OpenAI STT → Gemini REST → OpenAI TTSを置換する。新しいLive中継クライアントとして責務を分ける |
| `src/main.cpp` | 呼びかけ/ボタンからLive接続し、録音・応答待ち・再生・終了を状態機械へ移す。既存 `config.hasKeys()` による開始拒否を新構成に合わせる |
| `src/AudioDevice.cpp` / `include/AudioDevice.h` | 16kHz収録を使う。24kHz再生の有界キュー、DMA完了待ち、バッファ寿命、停止・消去を追加する |
| `src/Config.cpp` / `include/Config.h` / 設定例 | 端末は中継URL、プロトコル版、音量等を保持。端末でGoogle/OpenAIキーを要求しない |
| `src/LocalWake.cpp` | ローカル検出を保持し、呼びかけ前の音声をサーバへ送らない |
| 将来の `server/server.ts` | ExpressのHTTPサーバとws、端末ごとのLiveセッション、PCM変換、接続期限、キュー、秘密情報を管理 |
| 将来の `server/package.json` / lockfile / `.env.example` | 正確な依存版と再現手順、キーを含まない環境変数名を記録。実 `.env` と派生バックアップをGit除外 |

サーバは **Node.js 22 LTS**案。公式現行リリース一覧で22はLTS。SDKドキュメントは現状Node20以上、将来3.0.0から22以上と説明するため、20を新規採用しない。[Node公式一覧](https://nodejs.org/en/about/previous-releases)、[SDK要件](https://googleapis.github.io/js-genai/release_docs/index.html)

Express、ws、@google/genai、TypeScript、型定義は実装開始時に互換性を確認して正確な版を固定し、lockfileとNode版を記録する。今回はインストール・依存変更をしていない。SDKのGoogleGenAIには `apiKey: process.env.GEMINI_API_KEY` を明示し、変数の自動検出差を避ける。この初期化方式は [SDK公式例](https://googleapis.github.io/js-genai/release_docs/index.html) にある。

## 音声データ・バッファの算術（提案）

以下はAPI仕様を前提とした独自の初期値。性能保証ではない。Bはbyte、1 KiB = 1024 B。

| 方向/用途 | 計算 | 初期案 |
| --- | --- | --- |
| 上りPCM | 16,000 sample/s × 2 B × 1ch = **32,000 B/s** | 20ms = 320 sample = **640 B**ずつ収録・送信 |
| 下りPCM | 24,000 × 2 × 1 = **48,000 B/s** | 中継で20ms = 480 sample = **960 B**へ再分割 |
| 上りbase64 | 640 Bを個別符号化すると `4×ceil(640/3)=856`文字 | 50回/sで42,800 B/s相当＋JSON・TLS等。端末では符号化しない |
| 下りbase64 | 960 Bなら1280文字 | 同じ20ms単位換算で64,000 B/s相当＋JSON等。Googleの実チャンク長は固定と仮定しない |
| 端末録音リング | 200ms × 32,000 = **6,400 B** | 溢れたら会話失敗として停止し、黙って欠落させない |
| 端末再生リング | 2s × 48,000 = **96,000 B** | PSRAM候補。I2S/DMAが使える領域へ別途コピーする必要性を確認 |
| 再生開始待ち | 120ms × 48,000 = **5,760 B** | 下限の初期案。短い応答は終端到着後に不足量でも開始 |
| サーバ下り待ち行列 | 30s × 48,000 = **1,440,000 B**/会話 | PCM上限。base64原文を保存し続けず復号後だけ保持 |

ここで20msはPCMのサンプル数から求めた音声時間であり、現在の `AudioDevice::read()` がその周期で連続収録できることを実測した値ではない。ネットワーク処理を併行したときの実サンプル数・経過時間・欠落を音声単体/LAN試験で確認する。

LANのバイナリは12 Bの独自ヘッダ＋raw PCM payload案とする。ヘッダは little-endian の `generationId:u32`、`sequence:u32`、`sampleCount:u16`、`flags:u16`（初版0）。本文は上り640 B / 下り960 B以下、最終だけ短くてよい。`payload.length == sampleCount*2`、偶数長、上限、順番を検査する。JSONでformatを合意済みのため毎フレームのbase64やWAVヘッダは不要。これはGoogle仕様ではなく、本中継プロトコルの案である。

ヘッダ分は片方向20msで600 B/s。上り計32,600 B/s、下り計48,600 B/sとなり、WebSocket/TCP/IP/Wi-Fi等のオーバーヘッドは別。Googleのpart境界を音声の発話境界にしない。奇数byteや形式不一致が届いた場合は不正データとしてその会話を止める。

`playRaw` はキュー投入後も元メモリを参照する可能性を前提に、DMA消費完了まで再利用しない。`playing()==false` だけで物理的な末尾の再生完了を保証できるか、M5Unified固定版の実装と実音で確認する。未確認の固定遅延だけをdrain判定には使わない。

## 制御プロトコルと状態遷移（提案）

**1端末の1会話に1つのGemini Liveセッション**を割り当て、会話中の複数ターンは同じセッションで保持する。端末間共有・音声broadcastはしない。呼びかけ検出まではクラウドに接続せず、会話を終えたらSDKセッションとLAN接続を閉じる。1会話は初期上限10分、無操作終了は初期30秒案で実測調整する。

制御はJSON text frame。全制御に `protocolVersion:1` を付け、初回start後の制御には `sessionId`（readyで通知するサーバ採番）、該当時 `generationId` を付ける。generationIdは会話内の入力・応答サイクル識別子で、GoogleのIDを仮定せず中継が管理する。seqは方向別・generation別に0から管理する。新たな会話は新sessionIdを使い、古い非同期callbackを両IDで拒否する。

| 順序 | 制御/状態 | 意味・処理 |
| --- | --- | --- |
| 1 | 端末 `start` → `CONNECTING` | 音声形式・版を通知。中継がLiveへ接続。開始前の音声は送らない |
| 2 | Google `setupComplete` → 中継 `ready` | API設定完了とセッション参照確保の両方の後だけreadyを出す。接続期限初期10秒 |
| 3 | 端末 `LISTENING` | 開始音が終わってから16kHz録音開始。マイク音声のみPCMで送る。表示もこの時点で「話してください」 |
| 4 | 自動VADが発話終端を判断 | サーバ自動VADを使い、初期silenceDurationMs=600、prefixPaddingMs=100案。端末の登録用400msゲートをLiveの終端として重ねない |
| 5 | 最初の応答音声 → 中継 `capture_stop` | 端末は録音停止して `capture_stopped` を返す。以後の上りPCMは拒否。中継は入力停止をaudioStreamEndでGoogleへ通知 |
| 6 | 中継 `audio_start` → `PLAYING` | capture_stopped後だけ再生開始を許可。最初のPCMはその間サーバに有界保持。開始音は返答に重ねない |
| 7 | Google `generationComplete` | 生成完了の情報として保持。これ単独でマイクを再開しない |
| 8 | Google `turnComplete` → 中継 `turn_complete` | 全PCMをcreditに従って下りWebSocketへ送出し終えてから、同一接続へ終端を送る。未送信キューへの格納完了だけでは終端を送らない |
| 9 | 端末リング・再生キュー・DMAの全消費 → 端末 `drain` | 実再生完了をサーバへ通知。マイクはまだ停止。サーバの全下り送出も確認 |
| 10 | `turnComplete` と `drain` の両方成立 → 中継 `listen` | 次generationIdを割当て、16kHz録音へ戻る。最終音声を再録音しない |
| 終了 | `stop` / `error` / 切断 | 端末録音・再生・両側待ち行列を停止消去。中継SDK.close。ローカル呼びかけ待機へ戻る |

ここで **録音停止、Googleターン終了、端末drainは別イベント**。`audioStreamEnd` は入力ストリーム終端であり、会話全体のcloseでも、再生完了でもない。`activityStart/End` と手動 `sendClientContent({turnComplete:true})` をこの経路に混ぜない。

中継はcapture_stopをbinary音声より先に同じWebSocketへ送る。capture_stoppedまでのin-flight上りPCMは中継で破棄し、停止通知待ちの間もGoogleへ転送しない。各録音停止につきaudioStreamEndは1回だけ送る。onopenはWebSocketが開いた段階で、API設定の完了とは区別する。

応答なしでも永久待ちにならないよう、録音は初期10秒上限で止め、audioStreamEndを1回だけ送り応答待ちへ移す。端末が `capture_stopped` を返さない、入力終了から初期20秒以内に完了しない、proactive audioにより音声なしのturnCompleteとなる場合を区別する。無音完了ならPCMなしのdrain相当を処理して次のlistenへ戻し、期限超過はerrorで会話終了する。初期値は日本語発話とネットワーク実測で調整する。

Googleイベントに音声とturnCompleteが同居した場合も、**全partの音声処理を先に行ってから終端**を処理する。字幕はオプションとし、初版では入力・出力全文をログへ残さない。`interrupted` が予期せず届いたら当該generationの再生を止め、以後の遅延PCMを破棄して安全に会話を終える。ユーザー割込みを積極的に受ける設計は後続段階とする。

切断、APIエラー、不正形式、上限超過、期限超過は正常なturnCompleteに置き換えず、異常終了として表示・記録する。正常な音声なし完了と、通信途絶で音声が来ないケースを混同しない。

## バックプレッシャーと切断（提案）

`ws.bufferedAmount` は送信してネットワークへまだ出ていないbyte数で、端末が再生した量ではない。[ws API](https://github.com/websockets/ws/blob/master/doc/ws.md#websocketbufferedamount) そのため再生リングの空きと独立に監視する。

- 下りは端末が `credit` で受入可能な**PCM byte数**を返す。ヘッダは別計算。中継は未消費送信量を差し引き、空きを超えて送らない。120msの開始待ちを満たす初期creditを必ず与え、デッドロックを避ける。
- credit更新は受信時ではなく、DMAを含め参照が終わり元領域を安全に再利用できる消費確認後。seqの二重消費を防ぐ。アプリ待ち行列、wsのbufferedAmount、実際の端末空きの上限を別々に持つ。
- サーバはGoogleから早着した音声を最大1,440,000 Bだけ保持する。Google受信を止めれば生成も止まると仮定しない。上限・送信期限超過ならerror/stopで終了し、音声を途中削除して続行しない。
- 上りは20ms周期で実時間に合わせる。端末キュー200ms、ネットワーク待ち・SDK送信量も上限を持つ。SDKのsendRealtimeInputをawaitすれば排出が終わるとは仮定しない。固定版のtransport APIで送信滞留量を観測する実装を確認し、観測できなければその点を未解決のまま本実装完了にしない。
- 初版のLAN WebSocket maxPayloadは例えば4KiBに絞り、制御JSONも別の小さい上限を持つ。Google側の受信イベントサイズは別の上限として評価する。binaryのfragment再構成を含む利用ライブラリの挙動を確認する。
- stop時には両IDを無効化し、リングとSDK側を閉じる。再接続後の新会話へ古い音声・設定完了通知が流れ込まないようにする。

接続寿命の公式案内は、圧縮なしの音声セッション15分、接続自体はおおむね10分。`goAway.timeLeft`、contextWindowCompression、sessionResumptionがあり、再開handleの有効期間は切断後2時間とされる。[セッション管理](https://ai.google.dev/gemini-api/docs/live-api/session-management)

このため「1会話10分上限」だけで自然切断を回避できるとは考えない。初版はgoAwayで新ターンを止め、再生を終えて会話を閉じ、次の呼びかけで新セッションとする。通信断中の発話は保存・再送せず、再試行表示へ戻す。将来の連続会話では最新のresumableなhandleをサーバメモリだけに保持し、期限・再接続・未確認入力の重複を検証してから再開対応する。handleやGoogle接続URLをログへ出さない。

## 秘密情報・運用範囲（提案）

- Googleキーはサーバ環境変数。端末ファーム、端末SPIFFS、Git管理ファイル、通信URL、ログには埋め込まない。既存ignored設定を勝手に移動・削除しない。
- `ws://` は音声・制御が暗号化されないため、最初の検証は信頼できるLAN内の単一端末だけとする。端末以外から無制限にAPIを使える公開中継を作らない。接続数1、会話時間、音声byte数等の初期制限を設ける。
- 外部公開・共有LAN・複数端末に拡張する前に、`wss://`、端末認証、接続許可、同時数、レート制限を追加設計する。Google向け接続のTLS検証は維持する。
- マイク音声は会話中にGoogleへ送る構成になる。呼びかけ待機中はローカルのみ。検証用にも実音声・全文・キーをログ保存せず、時間、形式、byte数、状態、エラー分類で観測する。

## 検証計画（未実施）

| 段階 | 方法 | 合格の判断材料 |
| --- | --- | --- |
| 1. プロトコル単体 | Googleへ接続しない偽セッションとPCM fixture。正常・複数part・同居終端・音声なし完了・遅延通知・切断を入力 | ID、seq、偶数長、上限、終端順序、credit、タイムアウト、stop後の廃棄が守られる |
| 2. 音声単体 | 端末で16kHz収録と既知24kHz PCMを別々に検証 | sample数/時間が一致、符号/エンディアン/monoが正しい。実音で速度・ピッチ・末尾欠けなし。増幅4倍のクリッピング確認 |
| 3. LANだけ | 中継から既知PCM再生、端末PCMを同意した範囲でfixture照合。遅延・一時停止・遅い消費を注入 | 送受信長一致。リング溢れなし。溢れは検出して終了。最大heap/PSRAM/滞留時間を数値化 |
| 4. Google単体 | サーバから短い既知音声でLive接続 | キー/モデル利用可否、setupComplete、入力形式、全part復号、turnComplete、エラーを区別。接続成功だけを会話成功としない |
| 5. 実機統合 | 呼びかけ→案内→日本語発話→返答→drain→次ターンを複数回 | 開始音と返答を自己録音しない、語頭と語尾が欠けない、音切れなし。無音・長文・応答なし・stop・切断・goAwayでも待機へ復帰 |
| 6. 長時間/公開前 | 会話上限・接続寿命・連続リセット・認証/外部TLSを検証 | メモリ増加なし、古い音声再生なし、秘密情報なし。公開範囲の仕様を別途承認 |

既存ホストテスト14件と通常版の登録成功はこの新構成の検証には数えない。実装前に固定SDKの型チェック・プロトコル試験を追加し、実機書込み前にはSPIFFS/NVSを保持する手順を明記する。

## 未確認と次の着手点

未確認: 準備済みGoogleキーでの3.8 Live利用可否・課金/クォータ、SDK固定リリースとAPIバージョン、サーバ稼働機、LAN到達性、Google送信transportの滞留監視、連続24kHz再生とDMA末尾、音切れ耐性、16kHz増幅4倍の会話品質、初期VAD値、日本語応答、実遅延、無応答と接続寿命時の挙動。

次は、サーバ稼働機を決め、**キーを使わないプロトコル実装・シミュレーション → 単体音声 → LAN → 実API → 実機統合**の順で進める。本書作成時点では設計の記録だけを行い、プログラム変更、依存追加、端末へのキー導入、API呼出し、書込みは行っていない。
