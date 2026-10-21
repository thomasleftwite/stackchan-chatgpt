# 回答再生前200ms待機 R01

2026-10-21。目的: 回答の冒頭約0.2秒欠けという利用者報告に対し、ToDoの端末修正を実装する。対象: AtomS3R + Atomic Voice Base A149、Wi-Fi/Gemini端末。端末への書込み・実聴は未実施。

## 変更

`src/LocalGemini.cpp`の回答受信完了後、`AudioDevice::output()`成功時点を記録し、200ms経過後に元のPCM先頭ポインタと全サンプル数をそのまま再生へ渡す。待機はループ内で行い、通信・画面・ボタン中止を継続する。口パクはPCM再生要求成功後に開始し、再生完了通知は実再生終了後に送る。12秒の再生監視時間はPCM再生要求から計測する。質問と登録のビープ後700ms受付は変更しない。無音PCMの追加は今回行っていない。

## 依存・検証

PlatformIO / espressif32 6.11.0 / Arduino-ESP32 2.0.17、M5Unified 0.2.26、M5GFX 0.2.33。その他の固定依存は`platformio.ini`。依存追加なし。

実コードとハードウェア代替によるC++9スイート168項目成功（face-ui34項目）。修正前は200ms待機のテストが失敗し、修正後成功。200ms境界、PCM先頭・長さ保持、待機中の口パク/完了通知抑制、取消・切断からの復帰を確認。既存の質問/登録700ms境界を含む回帰テストも成功。

実行方法: `C:\Python314\python.exe tools/run_host_tests.py --suite <suite>`。suiteはrules、gemini-ui、face-ui、wake-storage、wake-enrollment、wake-enrollment-ui、wifi-ui、wireless-link、wake-detection。記録: `.pio/host-tests/reply-delay-20261021-r01.txt`。

`platformio.exe run -e atoms3r-gemini-device -e atoms3r -e atoms3r-face-demo`で3構成成功。無線版RAM78,724 bytes、ELF flash1,602,037 bytes。仕様・品質の独立レビューとも修正必須指摘なし。

## 成果物・保持

- 導入候補: `.pio/firmware-snapshots/gemini-reply-delay-20261021-r01.bin`、1,602,400 bytes。
- SHA256: `CAD4BEF99F08BCD3A5D6F5B0F71AED930E15A22FEF19C7CADC13E38B4419BC64`。
- 旧診断版: `.pio/firmware-snapshots/gemini-wake-diagnostic-20261021-r01.bin`、SHA256 `B432DB868E55F11B6CD8784D310D0C674CCFE457D4DD8D2A8DD710DC54DDE91A`不変。
- 変更前の作業中ソース/テスト: `.pio/source-snapshots/reply-delay-before-20261021-r01/`に保持。

## 導入・実聴確認（未実施）

USB接続を確認し、`tools/inspect_device_partitions.py`と`tools/flash_application_only.py`を用いて、確認済みの実機表・候補SHAを明示してapp0 0x10000だけを書き込む。NVS/SPIFFS/登録パターン/保存Wi-Fi設定は書換え対象にしない。通常のUSB制御・監視は行わない。

導入後は通常給電とWi-Fiへ戻し、PC Online / Sleepingから短押し→呼びかけ→ビープ終了約0.7秒後に質問する。最初の音節、口パク、回答終端、Ready復帰を複数回確認する。待機による冒頭欠け改善は実聴前のため未確認。改善しない場合は、音声自体の先頭と再生回路立上りを切り分け、無音PCMの先行駆動を別途検討する。
