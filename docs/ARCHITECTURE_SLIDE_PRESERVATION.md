# 構成スライドの保管と改訂

2026-10-21 / R03。現行はdocs/architecture-slide.pptx。AtomS3R・Windowsホスト0.6.0-rc.1・Geminiの3区分、1枚・58編集可能図形・13⅓×7½インチ・Yu Gothicを維持する。

RC1に合わせ、端末上段モードのみ/診断表示なし、回答前200ms待機、Wi-Fi固定ホスト、声のPC試聴と採用、将来のRasPi OS/macOS/ESP32-S3直接接続を反映した。

- 現行SHA256: A7F1F0C750AF03A02535DC62D04373FF87EC8EB69C867ED2651792035C61CD67、40,528 bytes。
- 直前R02: docs/archive/2026-10-21/rc1-before-compaction/docs/architecture-slide.pptx。SHA256 89C408D88B9F5F347756D467C666B5FCBA0EC0258E54DBC82DA945FA1771BCD3。
- 最初の復元版: docs/archive/2026-10-21/architecture-slide-before-hotspot-trust.pptx。旧復旧経緯は整理前のARCHITECTURE_SLIDE_PRESERVATION.mdに保持。

最終PPTXを再読込み・描画して全1枚を画像確認し、パッケージ/寸法/文字配置/フォント検査の指摘0件、第一者ツール再インポート成功。検証レポートは.pio/release-sync-20261021/validation-r03.json、before.png/final.png。既存資料と同様、描画を含むNode親プロセスは処理完了後も終了コード1を返したため、レポート・SHA・再読込みと画像を独立確認した。PowerPoint実アプリ表示は未確認。

編集は既存図形に行い、旧版を保管してから反映。元資料の削除や別PCとの同時編集をしない。スライドと保管版はGit追跡し、再取得はGit履歴とSHAで確認する。SVGは作成しない。

依存: バンドル@oai/artifact-tool、Node24.19.0、presentation skill26.1020.11127。根拠はREADME、TODO、RESUME、RC1/200ms待機の記録と該当実装。今回の資料更新では実機・API試験を追加していない。
