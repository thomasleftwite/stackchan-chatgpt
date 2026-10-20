# GitHub同期・統合検証

版: R01 / 2026-10-20（日本時間）。目的: 顔表示・呼びかけ登録R07とWindowsホストのソース/記録をmainへ統合する。

対象: codex/face-modes-stage1。依存: platformio.ini、server/package-lock.json、desktop/requirements-build.txt。

## 今回の検証

- Python: `python -m unittest discover -s tests -p 'test_*.py'`、28件成功。一時フォルダは案件内`.pio/push-merge-temp`を指定。
- サーバー: serverで`npm test`、26件成功。
- C++: `python tools/run_host_tests.py --suite <suite>`、rules 27 / gemini-ui 6 / face-ui 27 / wake-storage 29 / wake-enrollment 13 / wake-enrollment-ui 28、合計130項目成功。
- PlatformIO: atoms3r / atoms3r-gemini-device / atoms3r-face-demo、3構成ビルド成功。
- `node server/dist/host-self-check.js`: PASS、ローカルPCM往復12,001サンプル/worker起動確認、API呼出し0。
- 公開前検査: 公開候補150ファイル、未push履歴140 blob、ローカル秘密値/既知キーパターン一致0。秘密設定2件のGit除外・未追跡を確認。内容保持確認用ハッシュはGit除外領域だけへ保存。
- `git diff --check`: 成功。

## 統合前の修正

既存のdesktopテストが参照する`fresh_stage`が欠落していた。ビルド領域内の既存ステージを一意名で`prior-stages`へ退避し、空のステージを作る処理を復元してビルダーへ接続。既存内容保持、繰返し実行、範囲外拒否の回帰テスト成功。旧ステージの不要ファイル混入を防ぐ。

## 範囲と注意点

端末書込み、実マイク取得、外部Gemini呼出しは実施していない。今回のビルダー修正後の配布EXE再生成は未実施。別PCのUSB/実会話、長期検出精度、電源断耐性、保留中のUSB原因究明は既存記録の未検証/保留を維持する。配布物releases/と秘密設定はpushしない。

## 完了結果

2026-10-20 10:33 JST、[PR #4](https://github.com/thomasleftwite/stackchan-chatgpt/pull/4)をmainへmerge完了。作業コミット`58de3c0dc4bbb01459fd6b0f4927517de2a52a05`、mergeコミット`93a90718a0936f974dd4ebcb7197fb0a94074455`。未pushだった15コミットと今回の1コミットを統合。競合なし、GitHub側の必須チェック設定なし。

ローカルmainをfast-forwardし、merge後のツリーが検証済み作業コミットと一致すること、秘密設定2件の内容保持・除外・未追跡を確認した。この完了記録とTODOは追加の文書コミットとしてmainへpushする。
