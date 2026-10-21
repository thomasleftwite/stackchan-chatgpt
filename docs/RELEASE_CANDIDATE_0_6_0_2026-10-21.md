# 0.6.0 リリース候補 RC1

2026-10-21 / R02。対象: AtomS3R + Atomic Voice Base A149とWindows 10/11 x64ホスト。目的: デバッグ表示と接続方式の選択を整理し、通常利用向けの候補を用意する。端末アプリ限定書込み・ハッシュ検証成功。正式公開・RC1の実機統合試験は未実施。

## 今回の変更

- 端末: 待受の音量/閾値/照合距離（V/T/D）、診断理由、Listeningの音量バーを削除。上段はモード名のみとし、Say your call等を削除。顔・口パク・PC onlineなどの接続状態、登録/エラー等の操作案内は維持。
- ホスト: 接続はWi-Fi固定。接続方式プルダウン、USBポート欄、USB認識案内、USB接続の起動分岐、COM列挙を撤去。PC試聴後の声採用・即時一覧追加/選択/保存を維持。既存キー/声/ネットワーク設定を読み込む。
- 回答前200ms待機と質問/登録のビープ後700ms受付は維持。利用者は今回の変更前の200ms待機版で「冒頭欠けは無し」と報告。新RC1の実聴結果ではない。

## 成果物

| 対象 | パス | サイズ | SHA256 |
|---|---|---:|---|
| ホスト | releases/StackchanHost-0.6.0-rc.1-win-x64/StackchanHost.exe | 53,292,029 | 8C405F7BBA22639D080C19225FA4DBA4A68ABE4989AB7A19452BB752DB6AE7DA |
| 端末 | releases/Stackchan-AtomS3R-0.6.0-rc.1/firmware.bin | 1,601,712 | 931C6C234723E2B7846BD5D1FF276959560E8E5737CED14DFC93D6F548BC70BC |

同じ端末バイナリの書込み用保持先: `.pio/firmware-snapshots/gemini-release-20261021-rc01.bin`。直前版の`.pio/firmware-snapshots/gemini-reply-delay-20261021-r01.bin`とホスト0.6.0-preview.2は保持。今回変更前の作業中ソースは`.pio/source-snapshots/release-ui-before-20261021-rc01/`に保存。

依存は既存のPlatformIO/espressif32 6.11.0、Arduino-ESP32 2.0.17、M5Unified 0.2.26、M5GFX 0.2.33と`platformio.ini`。ホストは既存の同梱Node v24.15.0、Gemini Live連携。依存追加なし。

## 検証結果

Python49件、Node37件、C++9スイート169項目、端末3構成（atoms3r、atoms3r-gemini-device、atoms3r-face-demo）のビルドが成功。仕様/品質の独立レビューでコードの修正必須指摘なし。説明書の旧USB切替案内1件は修正済み。

最終EXE自己診断15項目成功、API呼出し0。Wi-Fi固定UI、USB列挙なしの検出、Wi-Fi開始処理、設定互換、声一覧と試聴画面の配置を確認。配布1370ファイルの私有値一致0。クリーンなステージから作成。旧EXEは保持。

端末の実描画コードを代替ハードウェアで実行し、Ready上段のモードのみ、呼びかけ案内なし、Listening音量バーなしを確認。`.pio/host-tests/release-ready-preview.svg`とPNGで配置を確認（実LCD表示ではない）。回帰結果は`.pio/host-tests/release-ui-20261021-rc01.txt`、最終EXE結果は配布フォルダの`SELF_TEST.json`。

## 端末導入結果

利用者の「ok」と「USB接続済み」に基づきCOM4のESP32-S3-PICO-1 rev0.2へ導入。実機表を`.pio/device-check/partitions-release-20261021-rc01.bin`へ取得し、app0=0x10000/容量0x300000、NVS=0x9000、SPIFFS=0x610000を確認。上記候補SHAを指定し、アプリ領域だけへ1,601,712 bytesを書込んだ。消去0x10000～0x197FFF、Hash of data verified、終了コード0、RTSリセット成功。NVS/SPIFFS/保存Wi-Fi/登録パターンは書換え範囲外。書込み後のUSB監視・実API呼出しは行っていない。

## 導入後に必要な確認

1. ホスト旧版を停止・終了して新EXEへ切替。Wi-Fi固定表示と既存キー/採用声の保持を確認。
2. 端末へのアプリ限定書込みは上記のとおり完了。保存済みWi-Fiと呼びかけ登録の利用可否を起動後に確認。
3. USBを外して通常給電へ戻し、PC Online / Sleeping→短押しReady→呼びかけ→ビープ終了約0.7秒後の質問を確認。
4. 上段のモードのみ表示、モニタ/呼びかけ案内なし、回答冒頭、口パク、回答終了・待受復帰を複数回確認。

RC1の実機確認と利用者の判断後に正式版化する。長期のウェイク精度・誤検出率など、従来の未解決項目は合格扱いにしない。
