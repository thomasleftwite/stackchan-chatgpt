# Gemini Live中継の接続確認版

版: 0.2.0-dev / 更新日: 2026-10-16。

現在の方針: AI Studio公開設定でStarter対象外を確認したため、ユーザー指定でクラウド公開を保留。まずローカルの通信・音声を検証する。PC内loopbackと端末単体の結果は [ローカル検証記録](../docs/LOCAL_AUDIO_VALIDATION_2026-10-16.md) を参照。LAN常駐サービスや課金設定への変更ではない。

目的: AtomS3Rの16 kHz PCM入力と24 kHz PCM出力をGemini Liveへ中継するクラウドサービスを作る。ユーザーの指定により、稼働先はAI Studio Buildから公開するCloud Runを想定する。このWindows PCのNode.jsは開発・試験用で、常駐サーバの前提にはしない。

現在は共通PCMフレームに加え、認証付きWebSocket echo、HTTPヘルスチェック、接続数/時間/サイズの上限、疎通クライアントを実装済み。Gemini接続と端末クライアントは未実装。Geminiキーは使用・複製・送信していない。Cloud Run向けDockerfileは用意したが、Dockerが手元にないためコンテナビルド・実デプロイは未検証。

## 再現方法

Node.js 22.9.0以降を使用する（今回の試験環境は24.15.0、npm 11.14.0）。起動の `--env-file-if-exists` に必要な最低版を含めた条件。このフォルダで次を実行する。

```text
npm ci --ignore-scripts
npm test
```

依存はpackage.jsonで固定し、package-lock.jsonを保持する。`npm test`はTypeScriptを型検査・ビルドし、Node組込みテストで既知PCM、フレーム検査、未認証拒否、バイナリ往復、接続上限、時間切れ、不正・過大データ拒否、疎通クライアントを確認する。連続PCM試験を追加し現在12/12成功。20ms間隔で100フレーム（16kHz mono、計32,000サンプル）をPC内で往復し全データ一致を確認した。音声品質やGoogle/実機LANへの接続を確認する試験ではない。

## 起動と外部疎通試験

`.env.example`を参考に、32～256文字の推測困難な `DEVICE_TOKEN` を自分の `.env` またはクラウドのSecretへ設定する。これは端末認証専用で、Geminiキーとは別。キーやトークンをチャットへ貼らない。`npm run build`後に`npm start`で起動する。HTTPサーバは `0.0.0.0:PORT`（既定8080）で待つので、開発PCでは公開ネットワークから到達できない環境で使用する。自動起動・常駐設定は行っていない。

- `/healthz`: 認証なしで `{"status":"ok","mode":"echo"}`。秘密や環境変数を返さない。
- `/live`: `Authorization: Bearer <DEVICE_TOKEN>` をupgrade前に確認。クエリ付きURL、別パスは拒否。正しい接続にはmode=echoのreadyを送り、入力のPCMパケットを変更せず返す。
- 1インスタンスにつき同時1接続、30秒で終了、無通信10秒で終了。WebSocketメッセージ最大1,024 B、1秒窓の入力最大65,536 B、送信滞留32,768 B超過で切断。クラウド全体の同時数上限ではない。
- 公開後、手元の `.env` に `PROBE_URL=wss://<公開ホスト>/live` と同じ認証トークンを設定して `npm run smoke`。既知640 Bの合成PCMを送り、byte一致と正常closeを判定する。音声を録音・保存・ログ出力しない。

echoは16 kHz入力をそのまま返す疎通用で、24 kHzへの変換やGeminiの返答ではない。端末ではこのechoを24 kHz音声として再生しない。

## 形式

1メッセージは12 byteのlittle-endianヘッダとraw PCM本文。generationId:u32、sequence:u32、sampleCount:u16、flags:u16（0）の順。本文はsigned 16-bit little-endian mono。上りは最大640 byte（16 kHzで20ms）、下りは最大960 byte（24 kHzで20ms）。順序・世代・creditの状態管理は次の実装範囲で、現モジュールは構造の検証のみ。

## 秘密設定と次の段階

`.env.example`は空の見本。`.env`とその派生ファイル、node_modules、distはGit対象外。Cloud RunではGoogleキーをサーバのSecretとして扱い、端末やGitに埋め込まない。準備済み `../data/secrets.json` は保持し、この試験では読み込まない。

クラウド方式の一次資料は [実現可能性の確認](../docs/AI_STUDIO_CLOUD_FEASIBILITY_2026-10-16.md)、通信全体は [構成検討](../docs/GEMINI_LIVE_ARCHITECTURE_REVIEW.md)、公開前の操作と現状は [Starter公開準備](../docs/STARTER_ECHO_DEPLOYMENT.md) を参照。ローカル試験は完了し、Cloud Run上のWSS疎通→Gemini接続→AtomS3R統合は未完了。
