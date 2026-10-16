# GeminiキーとローカルPC音声中継の検証

版: R01 / 完了日: 2026-10-16（日本時間）。対象: Windows PC、保存済みGeminiキー、Node.js中継0.2.0-dev。目的: キーの有効性と16kHz入力/24kHz出力を実APIで確認し、実機接続へ進めるPC環境を用意する。

案件ルート（以下の成果物の基準）: `C:\Users\thoma\SynologyDrive\H\Doc\Src\Codex\projects\ai-stackchan\AI_StackChan_Ex\Stackchan-chatgpt`

その後の追記: 実機v2対応・実質問/回答の端末再生も1回成功した。[実機検証](GEMINI_DEVICE_VALIDATION_2026-10-16.md)を参照。以下はPC単独試験時点の結果で、当時の未実施記述は履歴として読む。

## 結果

| 検証 | 結果・範囲 |
|---|---|
| 保存済みキーとモデル | HTTP200。`gemini-3.8-live` のメタデータ取得とLive音声応答に成功 |
| テキスト→Live音声 | 24kHz / 22,081サンプル / 4チャンク、2,454ms、totalTokens=609。PCM本文は保存しない |
| PCの音声入力→Live回答 | 生成した試験音声を16kHzへ診断用変換。入力13,440サンプル→PCの認証付きWebSocket→Gemini→24kHz回答78,000サンプル、163フレーム。総量・連番・正常終了を確認 |
| PCサービス単独起動 | 非公開.envから起動、loopbackの空きポートでhealthz HTTP200、mode=gemini-live、protocolVersion=2。作成したプロセスだけを停止 |
| 自動テスト | TypeScriptビルドと25/25成功。実APIを呼ばない試験 |
| 設定の保持 | 元のsecrets.jsonは保存時のチェックポイントと一致。新.envにGeminiキーを複製せず、既存.envの再初期化で内容を保持。ignoreと未追跡を別々に確認。公開候補95ファイルに実キー/端末トークンの一致0件、git diff --check成功 |
| 実機統合 | 未実施。COM4はLAN診断版R04/protocol v1のままで変更なし |

実APIのLive接続は計3回（単純応答1回、試験音声生成1回、音声質問への応答1回）。非個人の短い試験用発話だけを使用し、ユーザーの録音や会話履歴は送っていない。音声回答の内容・自然さを人が聴取した試験ではない。PCクライアントのplayed通知は模擬であり、実スピーカーの再生完了を証明しない。利用枠の残量・料金は未確認。課金・AI Studio・クラウド公開設定は変更なし。

## 成果物と依存

- 起動: `Start-Gemini-PC.cmd`、`server/gemini-pc.ts`、`server/init-private-env.ts`。
- キー読込みとAPI確認: `server/private-config.ts`、`server/gemini-check.ts`。
- 実API音声処理: `server/gemini-turn.ts`、`server/gemini-isolated.ts`、`server/gemini-worker.ts`。
- PC WebSocketと試験クライアント: `server/gemini-server.ts`、`server/gemini-probe-client.ts`、`server/gemini-local-check.ts`。
- 新テスト: `server/test/gemini-turn.test.ts`、`gemini-server.test.ts`、`private-config.test.ts`。既存15件に10件を追加。
- 実行環境: Node.js24.15.0 / npm11.14.0。最低Node22.9.0。固定依存は@google/genai2.30.0、express5.3.0、ws8.22.0、TypeScript7.0.2。package-lock.jsonを保持。新しいPCサービスのHTTP処理はNode標準HTTPを使用する。

Geminiへの音声はsigned16bit little-endian monoのraw PCM、入力16,000Hz、回答24,000Hz。manual VADのactivityStart/Endを使用する。3.8用AUDIO応答を選び、thinkingConfigやproactivityの無効化は設定しない。モデルとPCM仕様は[Googleモデル資料](https://ai.google.dev/gemini-api/docs/models/gemini-3.8-live)・[Live機能資料](https://ai.google.dev/gemini-api/docs/live-api/capabilities)に基づく。

## 再現と運用

`server` で `npm ci --ignore-scripts` → `npm test`。通常起動は案件ルートの `Start-Gemini-PC.cmd`。キーは `data/secrets.json` の既存gemini_api_keyを読むのでOpenAIキーは不要。初期化する `server/.env` は端末専用のランダムトークンとHOST/PORTだけを保存する。値をチャットへ送らない。

既定HOSTは127.0.0.1、PORTは8080でPC内限定。起動中の画面でCtrl+Cで停止する。今回の起動確認後は停止済み。OS自動起動・Windowsサービス・ファイアウォール規則は追加していない。

実API再確認は `npm run gemini:check`（Live1回）、PC全経路の再確認は `npm run gemini:local-check`（Live2回）。API利用枠を消費するので日常の回帰には `npm test` を使う。ログは固定状態・HTTPステータス・サンプル数・時間などに限定し、キー・端末トークン・音声・文字起こし・APIエラー本文を保存しない。

新サービスはprotocol v2の1質問1回答。入力最大5秒、回答最大10秒、同時1接続、期限/データ量/連番/世代の制限あり。下りは1フレームごとのnext要求と最終played通知を必須にする。詳細は [server/README](../server/README.md)。SDKの接続確立待ちをabortだけで止められない場合に備え、APIを専用子プロセスへ分離し、親側の期限でその子だけを終了する。親終了時もIPC切断で子を終了する。

## 残る作業

1. ESP32側をprotocol v2へ対応し、可変長24kHz回答の有界再生キューと実際のdrain通知を実装する。現在のv1診断版を新PCサービスへ接続するだけでは使えない。
2. PCのHOSTを家庭内LANのIPv4へ設定し、端末専用トークンを公開せず端末へ渡す方法を整える。LAN内の平文WSを前提にした限定試験で、外部公開しない。
3. 実マイク→PC→Gemini→端末スピーカーを検証し、実聴取・遅延・切断時復帰を記録する。
4. その後、連続録音/会話履歴/発話割込み/長時間・複数条件評価を検討する。WSS/TLS、クラウド公開、Dockerビルドは別途未検証。

ファームウェアの変更・書込みは今回なし。NVS/SPIFFS/呼びかけ登録・既存Wi-Fi設定を保持。今回の新実装はローカル作業ブランチに保存し、GitHubへの追加push/mergeは行わない。
