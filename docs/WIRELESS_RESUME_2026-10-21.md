# 無線版の開発再開と実機導入

2026-10-21 / R02。対象: AtomS3R + Atomic Voice Base A149、Arduino-ESP32 / PlatformIO、Windowsホスト0.4.0-preview.1。

## GitHubとの整合性・スライド

`git ls-remote origin refs/heads/main` でGitHubのmainが `3283ad2d70c6fa6839aeb9398b76c075f7dddb88` と確認でき、ローカルの開発基点と一致。ローカルmainにのみスライド保管の `cd58beb` が1件追加された状態から再開。未公開差分はスライドとその保管手順・README・TODO・除外解除の5ファイルで、開発コードの競合はなかった。

GitHub CLIの既定認証はHTTP 401。Gitによるリモート参照照会は成功した。今回pushやPR操作は行っていない。

`docs/architecture-slide.pptx` はGitへ保存済み。SHA256 `3DE43ECC02C08145EDB15DF4FCCD4E87C1956B746A0DAD59C9CE26D07A27C241` を再確認。内容は変更していない。詳細は [スライド保管手順](ARCHITECTURE_SLIDE_PRESERVATION.md)。

## PC上の再検証と変更

- Python: `python -m unittest discover -s tests -p 'test_*.py' -v`、34件成功。TEMP/TMPは既存の案件内 `.pio/merge-check-temp` を使用。
- Node24.15.0: server内で `node node_modules/typescript/bin/tsc` と `node --test --test-timeout=10000 dist/test/*.test.js`、28件成功。TLS認証・模擬PCM往復を含む。外部API未使用。
- C++: `python tools/run_host_tests.py --suite <suite>`、rules27 / gemini-ui6 / face-ui27 / wake-storage29 / wake-enrollment13 / wake-enrollment-ui28 / wifi-ui4 / wireless-link10、計144項目成功。
- Windows MSVCの警告をエラーにする設定でwireless-linkが `strncpy` の非推奨警告C4996により停止した。`tests/wireless_stubs/Fakes.h` の代替strlcpyを、長さを制限したmemcpyと終端文字の設定へ変更。警告抑制は追加せず、再実行成功。実機用コードは変更していない。
- 配布EXEの `--ui-self-test` は10項目PASS、apiCalls=0。`.pio/device-check/wireless-resume-host-self-test-20261021-r01.json` に保存。
- 配布manifestのランチャーソース6件は5件がバイト一致、`tools/run_lan_probe.py` はCRLFへ正規化すると一致。server/package-lock.jsonも一致。

## 再ビルド・固定候補

前記録の `.pio/firmware-snapshots/gemini-wireless-v1.bin` は再開時に存在しなかったため、現在の追跡ソースから再ビルドした。前回候補と同一ハッシュとは扱わない。

PlatformIO `run -e atoms3r-gemini-device` 成功。固定依存は `platformio.ini`（Espressif32 6.11.0、Arduino-ESP32 2.0.17、M5Unified 0.2.26、M5GFX 0.2.33、WiFiManager 2.0.17、WebSockets 2.7.3等）に記載。PlatformIO管理領域のロック更新のみ追加権限で実行。

- 候補: `.pio/firmware-snapshots/gemini-wireless-resume-20261021-r01.bin`
- サイズ: 1,606,672 bytes
- SHA256: `16799F01945F35AD0723AE4F0B6E0FC9E62DC9881951159A927E6A7E4C4A60E3`
- RAM: 58,872 / 327,680 bytes、ELF Flash: 1,606,301 / 3,145,728 bytes。

## 実機導入

利用者が候補のアプリ限定導入と専用認証鍵のNVS保存を明示承認。USB接続依頼後にCOM4（VID0x303A/PID0x1001）を確認。

`tools/inspect_device_partitions.py` で実機パーティション表を確認し、`.pio/device-check/partitions-wireless-resume-20261021-r01.bin` に保存。app0は0x10000、容量0x300000。NVSは0x9000、SPIFFSは0x610000。

`tools/flash_application_only.py` に候補・SHA256・実機表を指定し、COM4の0x10000へ書込み成功。`Hash of data verified` を確認。書換え範囲は0x10000〜0x198FFFで、NVS/SPIFFS/パーティション表は対象外。実際の設定・登録読込みは起動応答で別途確認する。

書込み直後のGEM INFO照会では応答を取得できず、利用者へリセット短押しを依頼した。利用者の実施回答後、COM4で以下を確認し、`.pio/device-check/wireless-resume-boot-20261021-r03.json` へ固定状態と数値のみ保存した。生ログ・認証鍵は取得・保存していない。

- `GEM READY v=2 input=16000 output=24000 mono=1 bits=16`: アプリ起動・音声形式。
- `GEM WAKE available=1`: 保存済み呼びかけテンプレートの読込み。実際の検出精度は別途。
- `GEM LINK TLS`: 無線TLSモード。
- `GEM STATE configured=1 wifi=1 pc=0`: Wi-Fi接続済み、PC認証接続は未成立。ホットスポット接続済みという意味ではない。
- `GEM UI stage=3 mode=2 asleep=1 mic=0`: スリープ・マイク停止。
- `GEM RESULT NONE`: この起動で会話試験未実施。20秒の観察中に起動障害を示す固定文字列は検出されなかった。

PCのWi-Fi Direct仮想アダプタはDownで、ホットスポット側の接続は未稼働。利用者へ2.4GHzホットスポットの有効化を依頼中。SSID・パスワード・登録コードをチャットへ送らないよう案内。

## 次の検証

1. 起動、TLSモード、Wi-Fi、呼びかけ登録の読込み、スリープ中マイク停止は確認済み。
2. PCの2.4GHzホットスポットと端末の接続先を設定。Windows設定変更は別途確認してから行う。
3. 登録コードは端末画面からPCホストへ直接入力する。チャット・記録へ送らない。
4. 必要な受信許可は `tools/configure_wireless_firewall.ps1` の計画表示でホットスポット側だけを対象に確認してから適用する。
5. 実機TLSペアリング、質問/回答、回答後の再受付、再起動後の自動再接続を個別に確認する。PC上の模擬試験と区別し、未実施項目を合格にしない。

ホットスポット・受信規則はこの時点では未変更。実機録音・外部API呼出しは未実施。
