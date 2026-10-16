# Starter枠での接続確認サービス公開準備

版: R01 / 更新日: 2026-10-16（日本時間）

最新のソース束は `.pio/publish/stackchan-echo-probe-r02.zip`。公開版のソフトウェア環境はNode.js22.9.0以上を要求する。実行確認は24.15.0、コンテナはNode24を候補としており実ビルドは未確認。

## 目的と現在地

AI Studio BuildからStarter枠で、Geminiを呼ばない認証付きechoを公開し、AtomS3R向けのWSS経路を先に検証する。ユーザーは課金設定なし・Starter利用を希望。課金有効化や標準公開への自動切替は行わない。

ローカル版は [server/README](../server/README.md) を参照。型検査・ローカル接続試験は成功。AI Studioでアプリ作成要件を入力・送信したが、処理が進まず、ブラウザに `The caller does not have permission` が記録された。再読込み後のアプリ一覧も取得できず、新規画面へ戻った。原因は未特定で、Starter対象外と判定したわけではない。公開サービスURLはまだ存在を確認できていない。

その後ユーザーがGemini API用の「Stackchan-chatgpt」プロジェクトを作成し、一覧の無料枠表示を確認した。BuildアプリやStarter公開枠とは別の区分である。再試行するとBuildアプリの作成が開始され、現在生成中。プロジェクト作成が先のエラーの原因を解消したと断定する切り分け試験ではない。

## 公開する内容

- 名称案: Stackchan Echo Probe。
- ソース: server.ts / echo.ts / protocol.ts / package.json / package-lock.json / tsconfig.json。認証付き `/live` と公開ヘルス `/healthz`。
- 同梱Dockerfile/.dockerignoreは、上記の必要ファイルだけをビルドへ渡す。実キー、`.env`、data、node_modules、dist、録音は含めない。
- `DEVICE_TOKEN` のみサーバのSecretに設定する。新規の32文字以上のランダム値を使用し、Geminiキーを流用しない。ブラウザ表示・URLクエリ・コード・ログにトークンを入れない。
- この版はGeminiへ接続しない。AI Studioのアプリ生成機能の利用と、公開アプリがGemini APIを使うことは別。公開アプリ用のGemini Secretは不要。

公開前に生成コードがこの検証済みソースと一致するか確認する。AI Studioに説明を送っただけで、生成されたコードや公開版まで試験済みとはしない。ZIPの直接インポート対応も本作業では確認できていない。必要なら展開してCode画面へ反映するためのソース束として使う。

## アカウント側で確認すること

1. 通常のブラウザ操作でAI Studio Buildから新規アプリを作成できるかを確認する。現在の権限エラーと、公開段階でのStarter資格は別に扱う。
2. アプリが作成できたら、Code上のサーバ・起動コマンド・必要なSecretを確認する。Googleキーのアップロードは行わない。
3. Publish/Get StartedでStarterの選択肢を確認する。課金アカウント作成や有料アップグレードが出たら止める。利用資格は [公式公開ガイド](https://ai.google.dev/gemini-api/docs/aistudio-deploying) に従い、その画面で確認する。
4. 公開ボタンを押す前に、公開対象がこのechoだけであることと、Starter利用の条件を確認する。サービスへの外部アクセスが新しく可能になるため、その時点で対象画面に基づき利用者の確認を取る。

## 公開後の合格条件

1. 公開されたHTTPSホストで `/healthz` がechoモードを返す。
2. 同じホストの `wss://<host>/live` は未認証/誤トークンをHTTP401で拒否する。
3. 正しいトークンで `npm run smoke` を実行し、既知PCMのbyte一致と正常切断を確認する。接続にはTLS証明書検証を必須とする。
4. この後にAtomS3RのTLS/PCM試験、Gemini Live統合へ進む。echo成功だけを音声会話成功としない。

現在はクラウド公開、外部WSS、端末TLS、Gemini会話はいずれも未確認。Docker実行環境もないため、Dockerfileのビルド確認は別途必要。
