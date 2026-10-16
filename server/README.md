# Gemini Live中継の準備

版: 0.2.0-dev / 更新日: 2026-10-16。

目的: AtomS3Rの16 kHz PCM入力と24 kHz PCM出力をGemini Liveへ中継するクラウドサービスを作る。ユーザーの指定により、稼働先はAI Studio Buildから公開するCloud Runを想定する。このWindows PCのNode.jsは開発・試験用で、常駐サーバの前提にはしない。

現在は共通PCMフレームの処理とテスト、依存固定のみ。`server.ts`、WebSocket受付、認証、Gemini接続、Cloud Run設定、端末クライアントは未実装。起動コマンドやデプロイ用成果物はまだ提供しない。Geminiキーは使用・複製・送信していない。

## 再現方法

Node.js 22以降を使用する（今回の試験環境は24.15.0、npm 11.14.0）。このフォルダで次を実行する。

```text
npm ci --ignore-scripts
npm test
```

依存はpackage.jsonで固定し、package-lock.jsonを保持する。`npm test`はTypeScriptを型検査・ビルドし、Node組込みテストで既知のsigned PCM、ヘッダのbyte順、切れたデータ・奇数長・長さ不一致・上限超過の拒否を検証する。現在2/2成功。音声品質やGoogleへの接続を確認する試験ではない。

## 形式

1メッセージは12 byteのlittle-endianヘッダとraw PCM本文。generationId:u32、sequence:u32、sampleCount:u16、flags:u16（0）の順。本文はsigned 16-bit little-endian mono。上りは最大640 byte（16 kHzで20ms）、下りは最大960 byte（24 kHzで20ms）。順序・世代・creditの状態管理は次の実装範囲で、現モジュールは構造の検証のみ。

## 秘密設定と次の段階

`.env.example`は空の見本。`.env`とその派生ファイル、node_modules、distはGit対象外。Cloud RunではGoogleキーをサーバのSecretとして扱い、端末やGitに埋め込まない。準備済み `../data/secrets.json` は保持し、この試験では読み込まない。

クラウド方式の一次資料と未検証部分は [実現可能性の確認](../docs/AI_STUDIO_CLOUD_FEASIBILITY_2026-10-16.md)、通信全体は [構成検討](../docs/GEMINI_LIVE_ARCHITECTURE_REVIEW.md) を参照。次は認証付き `/live` と `/healthz` を実装し、模擬端末→Cloud Run上のWSS疎通→Gemini接続→AtomS3R統合の順に確認する。
