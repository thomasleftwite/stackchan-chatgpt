# AI Studioを使うクラウド中継の実現可能性

- 版: R01 / 調査日: 2026-10-16（日本時間）
- 目的: 「このWindows PCを常用サーバにせず、Google AI Studioでクラウドサービスを作り、AtomS3Rから利用する」構成を一次資料で検討する。
- 状態: 資料による実現可能性確認。AI Studio操作、クラウド作成・公開、キー登録、課金API呼出し、端末書込みは行っていない。
- 関連: 音声・ターン管理の設計は [Gemini Live検討書](GEMINI_LIVE_ARCHITECTURE_REVIEW.md)。同書のLAN/Windows常駐を前提にする部分は今回の希望構成に採用しない。

## 結論

**クラウドで中継し、PCを停止しても端末から音声会話する構成は、公式に提供される機能を組み合わせれば実現可能な構成と判断できる。** AI Studioは開発・プレビュー・公開操作の入口となり、公開後の中継プログラムの実行基盤は **Cloud Run**、推論・音声生成の実行先は **Gemini Live API**となる。

根拠は、AI Studioがサーバ側Node.js・npm・Secretsを提供すること、AI Studioからの公開がCloud Runサービスを作ること、Cloud Runが外部WebSocket接続をサポートすること。[AI Studio full-stack](https://ai.google.dev/gemini-api/docs/aistudio-fullstack)、[AI Studio公開](https://ai.google.dev/gemini-api/docs/aistudio-deploying)、[Cloud Run WebSocket](https://docs.cloud.google.com/run/docs/triggering/websockets)

一方、**AI Studioの生成テンプレートのまま独自 `/live` WebSocket upgradeが公開先まで通ること、ESP32から実際に接続・会話できることは未検証**。Geminiから「可能」と説明されたことだけを動作証明にはしない。「基盤の仕様として可能」と「このアプリが接続試験に合格」は分ける。

```text
開発・管理: Google AI Studio Build / Code / Secrets / Publish
                             │ Cloud Runへ公開
                             ▼
[AtomS3R + Atomic Echo / Voice Base]
  │ wss://<実際に発行されたCloud Runホスト>/live
  │ 端末認証 + 制御JSON + binary PCM
  ▼
[Cloud Run: Node.js / Express + ws / server.ts]
  │ @google/genai ai.live.connect()
  │ Geminiキーはサーバ側Secretのみ / Google向けWSS
  ▼
[Gemini Live API: gemini-3.8-live]
```

これは「自作のクラウド音声API」を端末が呼ぶ構成。AI Studioのチャット画面やプレビューURLを音声APIとして使う構成ではない。実行時にWindows PC、AI Studioのブラウザタブ、手元Node.jsを常駐させる前提は置かない。

## 一次資料で確認できたこと

| 項目 | 確認した仕様 | 本案件での意味 |
| --- | --- | --- |
| AI Studio実行環境 | サーバ側Node.js、npmパッケージ、外部API、秘密情報を利用できる。開発中はdev container | Express/ws/@google/genaiを使う中継の作成場所として整合する。プレビューだけを常設サービスと扱わない [1] |
| AI Studioからの公開 | 公開時にCloud Runサービスが作られ、URLが発行される | 端末は公開済みサービスのURLへ接続する [2] |
| 外部WebSocket | Cloud RunはWebSocketをサポート。HTTP/2 end-to-endは有効にしない | Node HTTPサーバの `/live` upgradeを実装し、外部はWSSで利用する [3] |
| 実行ポート/TLS | `0.0.0.0` の `PORT` をlisten。既定8080。TLSはCloud Run側で終端 | `127.0.0.1`/固定3000専用を避ける。コンテナ内に公開用証明書を置かない [4] |
| 公開範囲 | Cloud RunはInvoker IAM検査を既定で行い、公開アクセスへ変更する方法がある | 公開URLを知るだけで有料中継を使える状態にせず、端末認証も設計する [5] |
| 秘密情報 | Cloud RunはSecret Managerを環境変数/ファイルとして利用できる。環境変数のSecret版は固定推奨 | AI Studio Secretsまたは標準Cloud RunのSecret設定でキーを管理 [1][6] |

出典は末尾。表の機能が存在することは確認済みだが、対象アカウントの権限・利用資格・公開設定は調べていない。

## AI Studio公開ルートと代替

**第一候補:** AI Studio Buildで、ブラウザUIとは独立したNode HTTPサーバの `/live` エンドポイントを作る。必要なら管理/状態確認用の小さなWeb画面を同じサービスに持たせる。サーバ側でGoogleへ接続することを明示し、ブラウザのマイクやブラウザ内SDKに音声処理を依存させない。

AI StudioのSecretsで `GEMINI_API_KEY` を扱えることは公式説明にある。ただし自動設定されたキーが今回準備したキーと同じか、公開先に意図したSecretが渡ったかは個別確認が必要。キー値は会話・コード・ログへ記載しない。[1]

最初に、Geminiを呼ばない認証付きWebSocket echoを公開し、外部クライアントから `/live` のupgradeとバイナリ往復を確認する。プレビュー内で動いたことだけで公開版成功としない。独自upgrade・起動コマンド・公開ポートをAI Studioの公開手順で保持できるかが、このルートの最初の確認点。

**代替:** AI Studioで作成したコードをGitHub同期/ZIP出力し、同じ中継を通常のCloud Runサービスへ直接デプロイする。AI Studioはこれらのコード出力手段を提供する。[Build概要の外部開発・公開](https://ai.google.dev/gemini-api/docs/aistudio-build-mode) これは「中継をクラウドで動かし、PC常駐を不要にする」希望には合う。公開操作までAI Studio内で完結させる要望がある場合は、その点だけ異なる。**現時点でAI Studioが独自WebSocketを禁止すると確認したわけではなく、制約が判明した場合の代替**である。

## 接続・音声・認証の設計案

以下は今回の提案で、Googleがこの端末向けに保証する完成手順ではない。

- 端末 → Cloud Run: 発行されたHTTPSサービスホストを使う `wss://.../live`（通常443）。ローカルIPや `ws://` を公開用に流用しない。独自ドメインは初回検証に不要。
- Cloud Run → Gemini: 固定版 `@google/genai` の `ai.live.connect()` を使う。モデル・音声形式・SDK注意点は [既存検討書](GEMINI_LIVE_ARCHITECTURE_REVIEW.md) の一次資料照合を引き継ぐ。`gemini-3.8-live` のキー利用資格・クォータは未試験。
- 音声: 入力16,000 Hz、signed 16-bit little-endian mono PCM（32,000 B/s）、出力24,000 Hzの同形式（48,000 B/s）。端末との通信はbinary、中継とGoogleのSDK境界ではbase64。単独のSTT/TTSサービスやOpenAIキーは使わない設計。
- ターン: 1端末の1会話に1Liveセッション。初版は半二重、自動VAD。最初の返答前に録音停止、`turnComplete` と実再生drainの両方を確認して次の録音へ移る。有限キューとcreditで、ネットワークの遅れを無制限メモリへ逃がさない。
- 認証: 初版はCloud Runのネットワーク入口を外部端末から到達可能にし、その内側のアプリで端末専用の失効可能な認証を実施する案。Google APIキーを端末用パスワードに流用しない。接続時ヘッダでの端末認証をupgrade前に検証し、失敗した接続はGoogleへ接続しない。トークンをURLクエリやログへ載せない。
- IAM必須の私有サービスを使う場合は別の端末向けIDトークン発行・更新設計が必要。Cloud Runの認証とアプリの端末認証を同じものとして扱わない。サービスアカウント秘密鍵を端末へ埋め込む案は採用しない。
- 端末のTLS: CA検証、ホスト名/SNI、時刻同期、WSSハンドシェイク、認証ヘッダを固定ライブラリで試験する。証明書検証の無効化で成功扱いしない。

## 接続寿命・費用・運用制約

Cloud Run公式WebSocket資料では、接続は長いHTTPリクエストとして扱われ、**既定5分、設定上限60分**のrequest timeoutが適用される。1本でもWebSocketが開いているインスタンスは稼働中とされCPUが割当て・課金対象となる。最大同時接続は1コンテナあたり1,000までと説明されるが、音声中継でその数を処理できるという性能保証ではない。再接続先が同じインスタンスになる保証もない。[3]

従って、端末が無音でも常時WebSocketを開く設計は避け、呼びかけ後に接続し会話終了で閉じる。初回は1端末・1会話のアプリ制限、低い最大インスタンス数、短い音声/会話上限から始める。これは費用・負荷を抑える案で、厳密な請求上限の保証ではない。

Cloud Run側のtimeout延長だけではGoogle側Live接続の寿命は延びない。Google側のgoAway、Cloud Run timeout、Wi-Fi断、コンテナ停止を別々に扱う。初版は切断時にキューと旧セッションを破棄して待機へ戻り、次回の呼びかけで新会話とする。再接続後も古い会話が自動保持されると仮定しない。

Cloud Runは無通信時に既定で最小インスタンス0へ縮小し、ファイルシステムは永続保存ではない。起動遅延・インスタンス再起動がある前提で、録音案内はサーバ/Live準備完了後に出す。Secretやセッション再開状態をローカルファイルの永続性へ依存させない。[4]

公開の料金条件には2経路がある。現行AI Studio公開ガイドでは、**利用資格を満たすアカウントのStarter Tierは課金口座設定なしで最大2サービス**、標準公開は課金有効なGoogle Cloudプロジェクトが必要。Starter資格や利用枠は個別確認であり、本ユーザーが対象とは断定しない。[2] Gemini API、Cloud Run等の費用/枠は分けて確認し、AI Studioで開発できることから無料運用と推定しない。

## 次の検証手順

| 段階 | 確認すること | 成功とする範囲 |
| --- | --- | --- |
| 1. 公開準備 | AI Studioの利用アカウント、Starter/標準公開、必要権限、モデル利用資格、費用枠を確認。キーなし中継と端末認証を実装 | 仕様/設定が揃った。会話成功ではない |
| 2. クラウドのecho | AI Studioから公開した `/live` へ外部WSS接続。正しい認証はbinary往復、誤認証は拒否。Google APIは呼ばない | AI Studio → Cloud Run公開経路と独自upgradeの可否が判明 |
| 3. 端末TLS/音声 | AtomS3Rから発行ホストへ接続。16kHz入力byte数、既知24kHz PCM再生、切断復帰、キュー上限を確認 | 端末 ↔ クラウドの通信・音声確認 |
| 4. Gemini Live | Secretを設定し、短い同意済み音声でLive開始・返答・終端を試験 | 対象キー/固定SDK/モデルで実APIが動作。接続だけを会話成功としない |
| 5. 実機統合 | 呼びかけ→発話→返答→次ターン、自己再録音防止、無音、timeout、再起動、メモリ/遅延/利用量を確認 | クラウド構成の統合結果として別途記録 |

公開、キーのクラウド登録、実API試験は本調査では未実施。手元にあるNode.js試作の依存導入やローカル実行だけで、このクラウド経路が検証済みとはしない。

## 一次資料

調査日はいずれも2026-10-16。数値・UI・利用資格は実装/公開時に再確認する。

1. [Develop full-stack apps in Google AI Studio](https://ai.google.dev/gemini-api/docs/aistudio-fullstack): Node.js、npm、Secrets、dev container。
2. [Deploying from Google AI Studio](https://ai.google.dev/gemini-api/docs/aistudio-deploying): Cloud Run公開、発行URL、Starter/標準公開。
3. [Using WebSockets on Cloud Run](https://docs.cloud.google.com/run/docs/triggering/websockets): WebSocket対応、timeout、課金、同時接続、再接続。
4. [Cloud Run container runtime contract](https://docs.cloud.google.com/run/docs/container-contract): PORT/0.0.0.0、TLS終端、縮小・一時ファイル。
5. [Allowing public access](https://docs.cloud.google.com/run/docs/authenticating/public): Invoker IAM検査と公開設定。
6. [Configure secrets for services](https://docs.cloud.google.com/run/docs/configuring/services/secrets): Secret Managerと環境変数、版固定。
