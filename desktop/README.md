# Windowsホスト

開発ブランチの配布候補は **0.7.0-rc.1**（新EXEでの実機検証前）。「スタックチャン接続」で待受・必要な初回AP設定を開始します。初回自動設定の対応構成は有線上流・未接続のWi-Fi1台・2.4GHz/WPA2です。既存の声・キー設定を引き継ぎます。公開済みの0.6.0-rc.1は保持します。

- [利用手順](README.ja.md)
- [0.7候補の実装・検証記録](../docs/HOST_AP_PROVISIONING_IMPLEMENTATION_R01.md)
- [RC1の成果物・検証](../docs/RELEASE_CANDIDATE_0_6_0_2026-10-21.md)
- [開発・再開](../docs/RESUME.md)
- [旧Windows開発記録](../docs/archive/2026-10-21/rc1-before-compaction/desktop/README.md)

ソースはこのフォルダ、中継はserver/。配布は`python desktop/build_windows.py --node <Node v24.15.0のパス>`で作成し、クリーンステージ・秘密値検査・最終EXE自己診断を行います。配布物へ個人設定・キー・音声を含めません。
