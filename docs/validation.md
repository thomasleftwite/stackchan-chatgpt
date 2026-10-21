# 現行版の検証

2026-10-21 / RC1。過去の詳細は[整理前検証記録](archive/2026-10-21/rc1-before-compaction/docs/validation.md)に保持。

| 検証層 | 結果・根拠 |
|---|---|
| Python | 49件成功。設定保存・声採用/復元・Wi-Fi専用UIと開始処理 |
| Node / TypeScript | 37件成功。型検査、PCM、会話/試聴排他、声指定 |
| C++ | 9スイート169項目成功。実コードと代替ハードウェアによるタイミング/状態/保存/録音検証 |
| ビルド | atoms3r / atoms3r-gemini-device / atoms3r-face-demoの3構成成功 |
| 最終ホストEXE | 15項目PASS、API呼出し0。配布SELF_TEST.json |
| 端末導入 | RC1をCOM4 app0 0x10000だけへ書込み、ハッシュ検証成功。設定保存領域は範囲外 |
| 利用者確認 | RC1導入後の確認依頼にokと回答。直前の200ms待機版で冒頭欠け無しを明示確認 |

[RC1のSHA・導入・検証記録](RELEASE_CANDIDATE_0_6_0_2026-10-21.md)。回帰コマンドはPython: `python -m unittest discover -s tests -p test_*.py`、Node: serverで`npm test`、C++: `python tools/run_host_tests.py --suite <suite>`。9スイート名は同記録を参照する。

公開前の検査とGitHub同期結果は[公開記録](GITHUB_RC1_SYNC_2026-10-21.md)。資料整理で実機・API試験を追加したとは扱わない。長期間のウェイク精度/誤検出率、物理電源断、別PC/他OS、全候補の日本語品質は継続検証。
