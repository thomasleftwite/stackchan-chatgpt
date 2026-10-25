# StackchanHost 0.7.0 公開・整理記録

2026-10-25 / R01。Windowsホスト正式版の公開資産照合と、利用者が指示した旧ホスト・中間生成物削除の証跡。対象の正式パッケージはプロジェクト内 `releases/StackchanHost-0.7.0-win-x64/`、公開先は [v0.7.0](https://github.com/thomasleftwite/stackchan-chatgpt/releases/tag/v0.7.0)。端末は0.6.0-rc.1を継続する。

- `ASSET_VERIFICATION.json`: 実ファイル7件とGitHub算出SHA256/サイズの全件一致、PR/タグの統合先。
- `REMOTE_RELEASE.json`: 公開後に取得したReleaseメタデータ。秘密情報を含まない。
- `RELEASE_AUDIT.json`: 公開前のソース・履歴・配布入力・展開物監査、EXE自己診断16項目、秘密設定保持の集計結果。配布入力は検証後に削除済み。
- `CLEANUP.json`: 削除対象、ファイル数、論理サイズ、残存0件、正式版・端末・秘密設定の保持結果。

ビルド依存関係はPython3.14.4、Node24.15.0、既存のPyInstaller環境と固定npm依存。再ビルド用の`.packaging/venv`と`.packaging/downloads`は残している。実行時のNode/Python事前インストールは不要。

再確認は配布先の`SHA256SUMS.txt`を実ファイルのSHA256と比較し、GitHub Release APIの全7資産のdigest/size/stateを本記録と照合する。タグ`v0.7.0`の対象は`131a49422e79bdb12fd60b87acf2449797237d90`。ソースの試験と実機検証の範囲は[正式版記録](../../HOST_RELEASE_0_7_0_2026-10-25.md)を参照する。正式版バイナリでの実機再試験・長期評価は未実施。
