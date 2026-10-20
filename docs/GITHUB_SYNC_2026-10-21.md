# Windowsホスト0.5と構成資料のGitHub統合

2026-10-21 / R01。目的: 検証済みのWindowsホスト・端末コードと開発資料をGitHub mainへ統合し、ローカルの秘密設定・成果物を保持する。

## 公開内容と統合

- [PR #6](https://github.com/thomasleftwite/stackchan-chatgpt/pull/6): `codex/hotspot-trust-windows` → `main`、MERGED。公開先は `thomasleftwite/stackchan-chatgpt`。
- PR先頭: `88ec21fdb4db32b0b46c31763d5aea5cf2e62f03`。マージ: `e106a54fe5e425673476d7bad5c239a49a1f9db1`。2026-10-21 01:32:52 JSTにGitHub上のマージを確認。
- ホットスポット参加を信頼するコード不要接続、ホットスポット側IPv4への限定待受と接続元サブネット照合、秘密値を含まないGemini失敗分類、Windowsホスト0.5.0-preview.1のソースとテストを公開。
- 開発経緯・再開資料、現行構成スライド、復元したスライドの元版・改訂前の資料全文を含める。
- 通常経路はHTTP/WSで、保護と参加確認はWi-Fi暗号化・パスワードに委ねる。同じホットスポットへ参加できる機器も利用可能という利用者指定を反映。USB診断と汎用サーバーの旧TLS対応は保持。

## 今回の公開前検証

| 対象 | 結果 |
|---|---|
| Python | 39件成功。TEMP/TMPは案件内`.pio/merge-check-temp` |
| Node / TypeScript | `npm test`、32件成功（型検査を含む） |
| C++ | 8スイート、145項目成功 |
| PlatformIO | atoms3r / atoms3r-gemini-device / atoms3r-face-demoの3構成成功 |
| 配布物の既存検証との一致 | EXEのSHA256、ランチャー6ソース、配布入力1367ファイル、server lockのmanifest一致 |
| 公開前の秘密情報検査 | 公開ツリー182ファイル・未公開履歴72 blobを、既存秘密値および既知キー形式とメモリ内比較。一致0件。PPTXは展開内容も検査 |
| 差分とPR状態 | 差分確認、Gitの空白検査成功。対象SHA一致、MERGEABLE/CLEAN。CIチェック・レビューは登録なしで、独立レビューを受けたとは扱わない |

PlatformIOは最初の制限環境で`platforms.lock`へのアクセス拒否となり、対象を明示した権限要求後に3構成成功した。コンパイル失敗とは区別する。

最終EXE自己診断11項目（API0回）、スライドの構造・配置検査/再読込み/画像表示、実機のコードなし接続・3回登録・呼びかけ会話・口パク・待受復帰・端末リセット後再会話は既存の検証記録に基づく。今回は端末操作・実API試験・EXE再生成を行っていない。[現行実装](HOTSPOT_TRUST.md)、[スライド検証](ARCHITECTURE_SLIDE_PRESERVATION.md)を参照。

## 保存物と同期確認

GitHub上のMERGEDとマージSHAを取得し、fetch後にローカルmainをfast-forward。マージ直後のローカルmainとorigin/mainが`e106a54`で一致し、検証済みPR先頭とのツリー差分0、作業ツリーの未コミット変更0を確認した。この完了記録・TODO・再開案内は後続の記録コミットとしてmainへ反映する。

作業前後に案件内の秘密設定2パスと、実際のWindowsユーザー領域にあるホスト設定/旧ペアリングの2パスを確認し、存在状態・ファイルハッシュ一致を確認。秘密値・認証トークンは出力・保存していない。確認用ハッシュと検査スクリプトは案件内`.pio/push-merge-20261021`（Git除外）。制限環境の一時LOCALAPPDATAと実ユーザー領域を区別して確認した。

EXE、ファームウェア、`.pio`、`.packaging`、依存生成物、秘密設定、登録特徴、音声・会話本文は公開しない。既存EXE・端末スナップショットは案件内で保持し、GitHub Releaseへのアップロードは実施していない。構成スライドと保管元版はGit上のバイト一致を確認した。

## 残る検証

長期精度・誤検出率、物理的な電源断、別PC移行、長時間/異常系、Raspberry Piホスト、連続ストリーミング・会話履歴は未確認または未実装。今回の公開・統合によって実機検証範囲を拡大したとは扱わない。
