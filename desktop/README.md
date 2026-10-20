# Windows配布ホスト

最新: 0.5.0-preview.1 / 2026-10-21。`releases/StackchanHost-0.5.0-preview.1-win-x64/StackchanHost.exe`。利用者指示で、ホットスポット接続を信頼し、コード入力・アプリPSKを不要にした。ホットスポット側IP/サブネットのみで受け付ける。Python39/Node32/C++145/3構成ビルド/EXE自己診断11項目成功。アプリ限定導入、コードなし接続、呼びかけ再登録/会話/口パク/待受復帰、端末リセット後の自動再接続と再会話を実機確認。[方式・検証範囲](../docs/HOTSPOT_TRUST.md) R03を優先する。

最新診断表示版: 0.4.0-preview.2 / 2026-10-21。`releases/StackchanHost-0.4.0-preview.2-win-x64/StackchanHost.exe`。無線TLS接続後のGemini問い合わせ失敗について、安全な固定分類とHTTP状態のみを表示する。Python37件/Node29件/最終EXE自己診断10項目成功。実機の回答取得・呼びかけ不反応は調査継続。[現在の記録](../docs/WIRELESS_RESUME_2026-10-21.md)を優先する。ビルドとアプリの版番号はsettings.pyに共通化。以下の0.3.0記述は初版の履歴。

版: 0.3.0-preview.1 / 2026-10-19。目的: 開発用Python/Node.js/npm/VS Code/PlatformIOを用意しない別のWindows 10/11 x64 PCで、既存Stage2端末の実機用中継を起動する。

## 成果物

案件ルートの`releases/StackchanHost-0.3.0-preview.1-win-x64/StackchanHost.exe`。EXE内へ実行環境と固定版依存を同梱。操作説明・ライセンス・検証記録・チェックサム付きZIPも同じreleasesフォルダに生成。配布物はGit除外。操作は[利用説明](README.ja.md)、最終結果は配布フォルダのVALIDATION.md / SELF_TEST.jsonを参照。

## 構成と依存

host_app.py: 日本語Tk GUI、COM/LAN選択、開始/停止、同梱環境の自己診断。settings.py: 任意のWindows DPAPI暗号化保存。network.py: .NETからネットワークのIPv4アドレスを検出、CIMは不要。既存tools/run_gemini_device.pyをGUIから呼ぶため、ランタイム位置・実行ファイル・停止イベント・状態コールバック・子プロセス環境を注入できるよう拡張。CLI既存動作も維持する。

Python 3.14.4 / Tk 8.6 / Node.js 24.15.0 / pyserial 3.5 / PyInstaller 6.22.3。ビルドツール固定版はrequirements-build.txt、Node依存はserver/package-lock.json。Nodeはローカルの公式署名済み同版バイナリをコピーし、公式ライセンスを取得。実行依存はnpm ci --omit=dev --ignore-scriptsでロックから復元する。ビルドマニフェストにランタイムのファイルハッシュと起動コードのハッシュを記録する。

## 再ビルド

1. Python 3.14.4 x64、Node.js 24.15.0を開発PCに準備。
2. 案件フォルダをSynology Driveで永久ピン留めし、同期と書込み権限を確認。
3. `python -m venv .packaging/venv`
4. `.packaging/venv/Scripts/python.exe -m pip install -r desktop/requirements-build.txt`
5. `.packaging/venv/Scripts/python.exe desktop/build_windows.py`
6. EXEを別フォルダへ単独コピーし、`--ui-self-test --report <JSON保存先>`で自己診断。配布前に機密ファイル/実キー一致の検査、ライセンス/チェックサム/検証結果を確認する。

builderはソース/JSの許可リストとnpmの実行依存だけを収集し、秘密ファイルをコピーしない。ユーザーの実キーと比較して一致があればビルド停止。配布先でAPIキーを入力する。端末ファームウェアやドライバ/ファイアウォールを変更する機能は含まない。Windows標準コンポーネントとドライバ初回認識はOSへ依存する。

## テスト

- `python -m unittest discover -s tests -p test_desktop_host.py -v`: 設定/暗号化/検証/子環境、7件。
- `python -m unittest discover -s tests -p test_gemini_coordinator.py -v`: 監視/停止/GUI連携、17件。
- serverで`npm test`: 中継、26件。
- 同梱host-self-check.js: localhostでPCM往復と子プロセスを確認。ダミーキーと無効入力でSDK接続前に止め、外部APIは呼ばない。
- 最終EXEの`--ui-self-test`: 同梱Node/Tkの起動、配置、設定暗号化、非保存、ローカルPCM/子プロセス、機密ファイルの不在、USBモジュール読込。別PCでの実機会話は別途確認。

## 一次資料

- PyInstaller: https://pyinstaller.org/en/stable/operating-mode.html
- Windows DPAPI: https://learn.microsoft.com/en-us/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata
- Espressif USB Serial/JTAG: https://docs.espressif.com/projects/esp-iot-solution/en/release-v2.0/usb/usb_overview/usb_serial_jtag.html
- Nodeライセンス: https://github.com/nodejs/node/blob/v24.15.0/LICENSE
